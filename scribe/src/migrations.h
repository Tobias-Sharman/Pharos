#ifndef SCRIBE_MIGRATIONS_H
#define SCRIBE_MIGRATIONS_H

#include <scribe/error.h>

#include "db.h"

enum ScribeError applyMigrations(struct ScribeDb* db, const char* migrationsDir);

#endif // SCRIBE_MIGRATIONS_H
