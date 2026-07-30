#ifndef SCRIBE_MIGRATIONS_H
#define SCRIBE_MIGRATIONS_H

#include <scribe/error.h>

enum ScribeError applyMigrations(const char* dbPath, const char* migrationsDir);

#endif // SCRIBE_MIGRATIONS_H
