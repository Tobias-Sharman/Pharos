#include "db.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <scribe/config.h>

enum ScribeError scribeDbOpen(const char* path, struct ScribeDb* outDb) {
	if (path == NULL || path[0] == '\0' || outDb == NULL) {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	outDb->handle = NULL;
	sqlite3* handle = NULL;

	int rc = sqlite3_open(path, &handle);
	if (rc != SQLITE_OK) {
		if (handle != NULL) {
			sqlite3_close(handle);
		}

		return SCRIBE_ERR_IO;
	}

	rc = sqlite3_busy_timeout(handle, SCRIBE_DB_BUSY_TIMEOUT);
	if (rc != SQLITE_OK) {
		sqlite3_close(handle);
		return SCRIBE_ERR_SQL;
	}

	outDb->handle = handle;
	return SCRIBE_OK;
}

enum ScribeError scribeDbClose(struct ScribeDb* db) {
	if (db == NULL) {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	if (db->handle == NULL) {
		return SCRIBE_OK;
	}

	int rc = sqlite3_close(db->handle);
	if (rc != SQLITE_OK) {
		return SCRIBE_ERR_SQL;
	}

	db->handle = NULL;
	return SCRIBE_OK;
}

enum ScribeError scribeDbExecute(struct ScribeDb* db, const char* sql) {
	if (db == NULL || db->handle == NULL || sql == NULL || sql[0] == '\0') {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	char* sqliteErr = NULL;

	int rc = sqlite3_exec(db->handle, sql, NULL, NULL, &sqliteErr);
	if (rc != SQLITE_OK) {
		sqlite3_free(sqliteErr);
		return SCRIBE_ERR_SQL;
	}

	return SCRIBE_OK;
}

enum ScribeError scribeDbBegin(struct ScribeDb* db) {
	return scribeDbExecute(db, "BEGIN IMMEDIATE;");
}

enum ScribeError scribeDbCommit(struct ScribeDb* db) {
	return scribeDbExecute(db, "COMMIT;");
}

enum ScribeError scribeDbRollback(struct ScribeDb* db) {
	return scribeDbExecute(db, "ROLLBACK;");
}

enum ScribeError scribeDbTableExists(struct ScribeDb* db, const char* tableName, int* outExists) {
	if (db == NULL || db->handle == NULL || tableName == NULL || tableName[0] == '\0' || outExists == NULL) {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	*outExists = 0;
	const char* sql = "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ? LIMIT 1";
	sqlite3_stmt* stmt = NULL;

	int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		return SCRIBE_ERR_SQL;
	}

	enum ScribeError err = SCRIBE_OK;

	rc = sqlite3_bind_text(stmt, 1, tableName, -1, SQLITE_TRANSIENT);
	if (rc != SQLITE_OK) {
		err = SCRIBE_ERR_SQL;
		goto cleanup;
	}

	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
		*outExists = 1;
	} else if (rc != SQLITE_DONE) {
		err = SCRIBE_ERR_SQL;
		goto cleanup;
	}

cleanup:
	rc = sqlite3_finalize(stmt);
	if (rc != SQLITE_OK && err == SCRIBE_OK) {
		return SCRIBE_ERR_SQL;
	}

	return err;
}

enum ScribeError scribeDbQueryInt(struct ScribeDb* db, const char* sql, int* outValue, int* outHasValue) {
	if (db == NULL || db->handle == NULL || sql == NULL || sql[0] == '\0' || outValue == NULL || outHasValue == NULL) {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	*outValue = 0;
	*outHasValue = 0;

	sqlite3_stmt* stmt = NULL;
	int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
	if (rc != SQLITE_OK) {
		return SCRIBE_ERR_SQL;
	}

	enum ScribeError err = SCRIBE_OK;

	rc = sqlite3_step(stmt);
	if (rc == SQLITE_ROW) {
		if (sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
			*outValue = sqlite3_column_int(stmt, 0);
			*outHasValue = 1;
		}
	} else if (rc != SQLITE_DONE) {
		err = SCRIBE_ERR_SQL;
		goto cleanup;
	}

cleanup:
	rc = sqlite3_finalize(stmt);
	if (rc != SQLITE_OK && err == SCRIBE_OK) {
		err = SCRIBE_ERR_SQL;
	}

	return err;
}

enum ScribeError scribeDbInsertIntText(struct ScribeDb* db,
                                       const char* tableName,
                                       const char* intColumnName,
                                       const char* textColumnName,
                                       int intValue,
                                       const char* textValue) {
	if (db == NULL || db->handle == NULL || tableName == NULL || tableName[0] == '\0' || intColumnName == NULL
	    || intColumnName[0] == '\0' || textColumnName == NULL || textColumnName[0] == '\0' || textValue == NULL
	    || textValue[0] == '\0') {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	const char* sqlFormat = "INSERT INTO %s(%s, %s) VALUES (?, ?);";

	int sqlSize = snprintf(NULL, 0, sqlFormat, tableName, intColumnName, textColumnName);
	if (sqlSize < 0) {
		return SCRIBE_ERR_IO;
	}

	size_t bufferSize = (size_t)sqlSize + 1;

	char* sql = malloc(bufferSize);
	if (sql == NULL) {
		return SCRIBE_ERR_OUT_OF_MEMORY;
	}

	snprintf(sql, bufferSize, sqlFormat, tableName, intColumnName, textColumnName);

	sqlite3_stmt* stmt = NULL;
	int rc = sqlite3_prepare_v2(db->handle, sql, -1, &stmt, NULL);
	free(sql);

	if (rc != SQLITE_OK) {
		return SCRIBE_ERR_SQL;
	}

	enum ScribeError err = SCRIBE_OK;

	rc = sqlite3_bind_int(stmt, 1, intValue);
	if (rc != SQLITE_OK) {
		err = SCRIBE_ERR_SQL;
		goto cleanup;
	}

	rc = sqlite3_bind_text(stmt, 2, textValue, -1, SQLITE_TRANSIENT);
	if (rc != SQLITE_OK) {
		err = SCRIBE_ERR_SQL;
		goto cleanup;
	}

	rc = sqlite3_step(stmt);
	if (rc != SQLITE_DONE) {
		err = SCRIBE_ERR_SQL;
	}

cleanup:
	rc = sqlite3_finalize(stmt);
	if (rc != SQLITE_OK && err == SCRIBE_OK) {
		return SCRIBE_ERR_SQL;
	}

	return err;
}
