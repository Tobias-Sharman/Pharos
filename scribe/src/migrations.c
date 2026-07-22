#include "migrations.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <scribe/db_schema.h>

#include "fs.h"

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

		const char* extension = strrchr(filename, '.');
		if (extension == NULL || strcmp(extension, ".sql") != 0) {
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

static enum ScribeError recordMigration(struct ScribeDb* db, const struct ScribeMigration* migration) {
	if (db == NULL || migration == NULL || migration->filename == NULL || migration->filename[0] == '\0') {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	return scribeDbInsertIntText(db,
	                             SCRIBE_DB_TABLE_MIGRATIONS,
	                             SCRIBE_DB_COL_MIGRATIONS_VERSION,
	                             SCRIBE_DB_COL_MIGRATIONS_FILENAME,
	                             migration->version,
	                             migration->filename);
}

static enum ScribeError applySingleMigration(struct ScribeDb* db, const struct ScribeMigration* migration) {
	if (db == NULL || migration == NULL || migration->filepath == NULL || migration->filepath[0] == '\0') {}

	char* sql = NULL;
	enum ScribeError err = readFile(migration->filepath, &sql, NULL);
	if (err != SCRIBE_OK) {
		goto cleanup_sql;
	}

	err = scribeDbBegin(db);
	if (err != SCRIBE_OK) {
		goto cleanup_sql;
	}

	err = scribeDbExecute(db, sql);
	if (err != SCRIBE_OK) {
		goto rollback;
	}

	err = recordMigration(db, migration);
	if (err != SCRIBE_OK) {
		goto rollback;
	}

	err = scribeDbCommit(db);
	if (err != SCRIBE_OK) {
		goto rollback;
	}

	goto cleanup_sql;

rollback:
	(void)scribeDbRollback(db);

cleanup_sql:
	free(sql);
	return err;
}

static enum ScribeError getCurrentMigrationVersion(struct ScribeDb* db, int* outVersion) {
	if (db == NULL || outVersion == NULL) {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	*outVersion = -1;

	int tableExists = 0;
	enum ScribeError err = scribeDbTableExists(db, SCRIBE_DB_TABLE_MIGRATIONS, &tableExists);
	if (err != SCRIBE_OK) {
		return err;
	}

	if (!tableExists) {
		return SCRIBE_OK;
	}

	int version = -1;
	int hasVersion = 0;

	err = scribeDbQueryInt(db,
	                       "SELECT MAX(" SCRIBE_DB_COL_MIGRATIONS_VERSION ") FROM " SCRIBE_DB_TABLE_MIGRATIONS ";",
	                       &version,
	                       &hasVersion);
	if (err != SCRIBE_OK) {
		return err;
	}

	if (!hasVersion) {
		return SCRIBE_ERR_INVALID_MIGRATION_PLAN;
	}

	*outVersion = version;
	return SCRIBE_OK;
}

static enum ScribeError applyMigrationPlan(struct ScribeDb* db, const struct ScribeMigrationPlan* plan) {
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

enum ScribeError applyMigrations(struct ScribeDb* db, const char* migrationsDir) {
	if (db == NULL || migrationsDir == NULL || migrationsDir[0] == '\0') {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	struct ScribeMigrationPlan plan = {0};

	enum ScribeError err = createMigrationPlan(migrationsDir, &plan);
	if (err != SCRIBE_OK) {
		goto cleanup;
	}

	err = applyMigrationPlan(db, &plan);

cleanup:
	destroyMigrationPlan(&plan);
	return err;
}
