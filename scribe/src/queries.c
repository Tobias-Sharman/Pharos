#include "queries.h"

#include <stdlib.h>
#include <string.h>

enum ScribeError scribeMigrationsTableExists(sqlite3* db, const char* name, struct ScribeMigrationsTableExistsRow* out, int* outHasRow) {
	sqlite3_stmt* stmt = NULL;
	enum ScribeError result = SCRIBE_OK;

	int rc = sqlite3_prepare_v2(db, "SELECT name FROM sqlite_master WHERE type = 'table' AND name = ?;", -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		result = SCRIBE_ERR_SQL;
		goto cleanup;
	}

	sqlite3_bind_text(stmt, 1, name, -1, SQLITE_TRANSIENT);

	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {

		const char* text = (const char*)sqlite3_column_text(stmt, 0);
		if (text != NULL) {
			size_t textLen = strlen(text) + 1;
			out->name = malloc(textLen);
			if (out->name != NULL) {
				memcpy(out->name, text, textLen);
			}
		} else {
			out->name = NULL;
		}

		*outHasRow = 1;
	} else if (rc == SQLITE_DONE) {
		*outHasRow = 0;
	} else {
		result = SCRIBE_ERR_SQL;
		goto cleanup;
	}

cleanup:
	sqlite3_finalize(stmt);

	return result;
}

void scribeMigrationsTableExistsRowFree(struct ScribeMigrationsTableExistsRow* row) {
	free(row->name);
}

enum ScribeError scribeGetLatestMigration(sqlite3* db, struct ScribeGetLatestMigrationRow* out, int* outHasRow) {
	sqlite3_stmt* stmt = NULL;
	enum ScribeError result = SCRIBE_OK;

	int rc = sqlite3_prepare_v2(db, "SELECT version, status FROM scribe_migrations ORDER BY version DESC LIMIT 1;", -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		result = SCRIBE_ERR_SQL;
		goto cleanup;
	}

	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
		out->version = sqlite3_column_int(stmt, 0);

		const char* text = (const char*)sqlite3_column_text(stmt, 1);
		if (text != NULL) {
			size_t textLen = strlen(text) + 1;
			out->status = malloc(textLen);
			if (out->status != NULL) {
				memcpy(out->status, text, textLen);
			}
		} else {
			out->status = NULL;
		}

		*outHasRow = 1;
	} else if (rc == SQLITE_DONE) {
		*outHasRow = 0;
	} else {
		result = SCRIBE_ERR_SQL;
		goto cleanup;
	}

cleanup:
	sqlite3_finalize(stmt);

	return result;
}

void scribeGetLatestMigrationRowFree(struct ScribeGetLatestMigrationRow* row) {
	free(row->status);
}

enum ScribeError scribeInsertMigrationAttempt(sqlite3* db, int version, const char* filename, const char* status) {
	sqlite3_stmt* stmt = NULL;
	enum ScribeError result = SCRIBE_OK;

	int rc = sqlite3_prepare_v2(db, "INSERT INTO scribe_migrations (version, filename, status) VALUES (?, ?, ?);", -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		result = SCRIBE_ERR_SQL;
		goto cleanup;
	}

	sqlite3_bind_int(stmt, 1, version);
	sqlite3_bind_text(stmt, 2, filename, -1, SQLITE_TRANSIENT);
	sqlite3_bind_text(stmt, 3, status, -1, SQLITE_TRANSIENT);

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
		result = SCRIBE_ERR_SQL;
		goto cleanup;
	}

cleanup:
	sqlite3_finalize(stmt);

	return result;
}

enum ScribeError scribeMarkMigrationApplied(sqlite3* db, const char* status, int version) {
	sqlite3_stmt* stmt = NULL;
	enum ScribeError result = SCRIBE_OK;

	int rc = sqlite3_prepare_v2(db, "UPDATE scribe_migrations SET status = ? WHERE version = ?;", -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		result = SCRIBE_ERR_SQL;
		goto cleanup;
	}

	sqlite3_bind_text(stmt, 1, status, -1, SQLITE_TRANSIENT);
	sqlite3_bind_int(stmt, 2, version);

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
		result = SCRIBE_ERR_SQL;
		goto cleanup;
	}

cleanup:
	sqlite3_finalize(stmt);

	return result;
}
