#ifndef SCRIBE_DB_H
#define SCRIBE_DB_H

#include <sqlite3.h>

#include <scribe/error.h>

struct ScribeDb {
	sqlite3* handle;
};

enum ScribeError scribeDbOpen(const char* path, struct ScribeDb* outDb);

enum ScribeError scribeDbClose(struct ScribeDb* db);

enum ScribeError scribeDbExecute(struct ScribeDb* db, const char* sql);

enum ScribeError scribeDbBegin(struct ScribeDb* db);

enum ScribeError scribeDbCommit(struct ScribeDb* db);

enum ScribeError scribeDbRollback(struct ScribeDb* db);

// outExists -> 1 => exists, 0 => doesn't
enum ScribeError scribeDbTableExists(struct ScribeDb* db, const char* tableName, int* outExists);

// hasValue -> 1 => has value, 0 => doesn't
enum ScribeError scribeDbQueryInt(struct ScribeDb* db, const char* sql, int* outValue, int* outHasValue);

enum ScribeError scribeDbInsertIntText(struct ScribeDb* db,
                                       const char* tableName,
                                       const char* intColumnName,
                                       const char* textColumnName,
                                       int intValue,
                                       const char* textValue);

#endif // SCRIBE_DB_H
