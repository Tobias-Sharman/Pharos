#include "migrations.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <sqlite3.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "fs.h"
#include "queries.h"

struct ScribeMigration {
	int version;
	char* filename;
	char* filepath;
};

struct ScribeMigrationPlan {
	struct ScribeMigration* items;
	size_t count;
};

static int compareMigrationsByVersion(const void* lhs, const void* rhs) {
	const struct ScribeMigration* left = lhs;
	const struct ScribeMigration* right = rhs;

	if (left->version < right->version) {
		return -1;
	}

	if (left->version > right->version) {
		return 1;
	}

	return 0;
}

static void destroyMigrationPlan(struct ScribeMigrationPlan* plan) {
	if (plan == NULL) {
		return;
	}

	for (size_t i = 0; i < plan->count; ++i) {
		free(plan->items[i].filename);
		free(plan->items[i].filepath);
	}

	free(plan->items);

	plan->items = NULL;
	plan->count = 0;
}

static int hasUpSuffix(const char* filename) {
	size_t len = strlen(filename);
	static const char suffix[] = ".up.sql";
	size_t suffixLen = sizeof(suffix) - 1;

	if (len < suffixLen) {
		return 0;
	}

	return strcmp(filename + (len - suffixLen), suffix) == 0;
}

// TODO: Reduce complexity by splitting off some sections into helpers
static enum ScribeError createMigrationPlan(const char* migrationsDir, struct ScribeMigrationPlan* outPlan) {
	if (outPlan == NULL) {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	outPlan->items = NULL;
	outPlan->count = 0;

	if (migrationsDir == NULL || migrationsDir[0] == '\0') {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	DIR* dir = opendir(migrationsDir);
	if (dir == NULL) {
		return SCRIBE_ERR_IO;
	}

	enum ScribeError err = SCRIBE_OK;
	struct dirent* entry = NULL;
	struct ScribeMigrationPlan plan = {0};
	while ((entry = readdir(dir)) != NULL) {
		const char* filename = entry->d_name;
		if (strcmp(filename, ".") == 0 || strcmp(filename, "..") == 0) {
			continue;
		}

		if (!hasUpSuffix(filename)) {
			continue;
		}

		if (!isdigit((unsigned char)filename[0])) {
			continue;
		}

		errno = 0;
		char* end = NULL;
		unsigned long version = strtoul(filename, &end, 10); // Base 10

		if (*end != '_') {
			continue;
		}

		if (errno == ERANGE || version > INT_MAX) {
			continue;
		}

		int migrationVersion = (int)version;

		for (size_t i = 0; i < plan.count; ++i) {
			if (migrationVersion == plan.items[i].version) {
				err = SCRIBE_ERR_DUPLICATE_MIGRATION_VERSION;
				goto cleanup;
			}
		}

		char* filepath = NULL;
		char* filenameCopy = NULL;

		err = makeJoinedPath(migrationsDir, filename, &filepath);
		if (err != SCRIBE_OK) {
			goto cleanup_entry;
		}

		struct stat st;
		if (stat(filepath, &st) != 0) {
			goto cleanup_entry;
		}

		if (!S_ISREG(st.st_mode)) {
			goto cleanup_entry;
		}

		filenameCopy = strdup(filename);
		if (filenameCopy == NULL) {
			err = SCRIBE_ERR_OUT_OF_MEMORY;
			goto cleanup_entry;
		}

		struct ScribeMigration* newItems = realloc(plan.items, (plan.count + 1) * sizeof plan.items[0]);

		if (newItems == NULL) {
			err = SCRIBE_ERR_OUT_OF_MEMORY;
			goto cleanup_entry;
		}

		plan.items = newItems;
		plan.items[plan.count].version = migrationVersion;
		plan.items[plan.count].filename = filenameCopy;
		plan.items[plan.count].filepath = filepath;
		plan.count++;

		filepath = NULL;
		filenameCopy = NULL;

	cleanup_entry:
		free(filenameCopy);
		free(filepath);

		if (err != SCRIBE_OK) {
			goto cleanup;
		}
	}

	if (plan.count > 1) {
		qsort(plan.items, plan.count, sizeof plan.items[0], compareMigrationsByVersion);
	}

	*outPlan = plan;

	plan.items = NULL;
	plan.count = 0;

cleanup:
	closedir(dir);
	destroyMigrationPlan(&plan);
	return err;
}

// TODO: Should be a query
static enum ScribeError migrationsTableExists(sqlite3* db, int* outExists) {
	struct ScribeMigrationsTableExistsRow row;
	int hasRow = 0;

	enum ScribeError err = scribeMigrationsTableExists(db, "scribe_migrations", &row, &hasRow);
	if (err != SCRIBE_OK) {
		return err;
	}

	if (hasRow) {
		scribeMigrationsTableExistsRowFree(&row);
	}

	*outExists = hasRow;
	return SCRIBE_OK;
}

// TODO: Add a force migration so deal with bricked db due to failed update
static enum ScribeError getCurrentMigrationVersion(sqlite3* db, int* outVersion) {
	*outVersion = -1;

	int tableExists = 0;
	enum ScribeError err = migrationsTableExists(db, &tableExists);
	if (err != SCRIBE_OK) {
		return err;
	}

	if (!tableExists) {
		return SCRIBE_OK;
	}

	struct ScribeGetLatestMigrationRow row;
	int hasRow = 0;

	err = scribeGetLatestMigration(db, &row, &hasRow);
	if (err != SCRIBE_OK) {
		return err;
	}

	if (!hasRow) {
		return SCRIBE_OK;
	}

	int isFailed = strcmp(row.status, "failed") == 0;
	scribeGetLatestMigrationRowFree(&row);

	if (isFailed) {
		return SCRIBE_ERR_PREVIOUS_MIGRATION_FAILED;
	}

	*outVersion = row.version;
	return SCRIBE_OK;
}

static enum ScribeError applySingleMigration(sqlite3* db, const struct ScribeMigration* migration) {
	if (db == NULL || migration == NULL || migration->filepath == NULL || migration->filepath[0] == '\0') {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	enum ScribeError err = scribeInsertMigrationAttempt(db, migration->version, migration->filename, "failed");
	if (err != SCRIBE_OK) {
		return err;
	}

	char* sql = NULL;
	err = readFile(migration->filepath, &sql, NULL);
	if (err != SCRIBE_OK) {
		return err;
	}

	char* sqliteErr = NULL;
	int rc = sqlite3_exec(db, "BEGIN IMMEDIATE;", NULL, NULL, &sqliteErr);
	if (rc != SQLITE_OK) {
		sqlite3_free(sqliteErr);
		err = SCRIBE_ERR_SQL;
		goto cleanup_sql;
	}

	rc = sqlite3_exec(db, sql, NULL, NULL, &sqliteErr);
	if (rc != SQLITE_OK) {
		sqlite3_free(sqliteErr);
		sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
		err = SCRIBE_ERR_SQL;
		goto cleanup_sql;
	}

	rc = sqlite3_exec(db, "COMMIT;", NULL, NULL, &sqliteErr);
	if (rc != SQLITE_OK) {
		sqlite3_free(sqliteErr);
		sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
		err = SCRIBE_ERR_SQL;
		goto cleanup_sql;
	}

	/* Migration's own transaction committed successfully - now flip the tracking
	 * row from 'failed' to 'applied', as its own separately-committed statement. */
	err = scribeMarkMigrationApplied(db, "applied", migration->version);

cleanup_sql:
	free(sql);
	return err;
}

static enum ScribeError applyMigrationPlan(sqlite3* db, const struct ScribeMigrationPlan* plan) {
	if (db == NULL || plan == NULL || (plan->count > 0 && plan->items == NULL)) {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	int currentVersion = -1;
	enum ScribeError err = getCurrentMigrationVersion(db, &currentVersion);
	if (err != SCRIBE_OK) {
		return err;
	}

	if ((currentVersion < 0) && (plan->count == 0 || plan->items[0].version != 0)) {
		return SCRIBE_ERR_INVALID_MIGRATION_PLAN;
	}

	for (size_t i = 0; i < plan->count; ++i) {
		const struct ScribeMigration* migration = &plan->items[i];

		if (migration->version <= currentVersion) {
			continue;
		}

		err = applySingleMigration(db, migration);
		if (err != SCRIBE_OK) {
			return err;
		}
	}

	return SCRIBE_OK;
}

enum ScribeError applyMigrations(const char* dbPath, const char* migrationsDir) {
	if (dbPath == NULL || dbPath[0] == '\0' || migrationsDir == NULL || migrationsDir[0] == '\0') {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	sqlite3* db = NULL;
	int rc = sqlite3_open(dbPath, &db);
	if (rc != SQLITE_OK) {
		if (db != NULL) {
			sqlite3_close(db);
		}
		return SCRIBE_ERR_IO;
	}

	struct ScribeMigrationPlan plan = {0};

	enum ScribeError err = createMigrationPlan(migrationsDir, &plan);
	if (err != SCRIBE_OK) {
		goto cleanup;
	}

	err = applyMigrationPlan(db, &plan);

cleanup:
	destroyMigrationPlan(&plan);
	sqlite3_close(db);
	return err;
}
