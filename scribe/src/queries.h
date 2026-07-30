#ifndef SCRIBE_QUERIES_H
#define SCRIBE_QUERIES_H

#include <sqlite3.h>
#include <stddef.h>
#include <stdint.h>

#include <scribe/error.h>

struct ScribeMigrationsTableExistsRow {
	char* name;
};
struct ScribeGetLatestMigrationRow {
	int version;
	char* status;
};

enum ScribeError scribeMigrationsTableExists(sqlite3* db, const char* name, struct ScribeMigrationsTableExistsRow* out, int* outHasRow);
void scribeMigrationsTableExistsRowFree(struct ScribeMigrationsTableExistsRow* row);

enum ScribeError scribeGetLatestMigration(sqlite3* db, struct ScribeGetLatestMigrationRow* out, int* outHasRow);
void scribeGetLatestMigrationRowFree(struct ScribeGetLatestMigrationRow* row);

enum ScribeError scribeInsertMigrationAttempt(sqlite3* db, int version, const char* filename, const char* status);

enum ScribeError scribeMarkMigrationApplied(sqlite3* db, const char* status, int version);

#endif // SCRIBE_QUERIES_H
