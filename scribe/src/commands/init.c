#include "commands.h"

#include <stdio.h>

#include "root.h"

static const char* const g_kScribeDefaultMigrationsDir = "db/migrations"; // TODO: Change to something like usr/share

static void printInitError(const enum ScribeError kErr, const struct ScribeArgs* args) {
	switch (kErr) {
		case SCRIBE_ERR_INVALID_ARGUMENT:
			(void)fputs("scribe: invalid root path\n", stderr);
			break;
		case SCRIBE_ERR_OUT_OF_MEMORY:
			(void)fputs("scribe: ran into memory issue when trying to create root layout\n", stderr);
			break;
		case SCRIBE_ERR_ROOT_CREATE_FAILED:
			(void)fprintf(stderr, "scribe: failed to create root layout at '%s'\n", args->rootPath);
			break;
		case SCRIBE_ERR_PREVIOUS_MIGRATION_FAILED:
			(void)fputs("scribe: a previous migration attempt failed and left the database in an unknown state, thus "
			            "refusing to continue automatically\n",
			            stderr);
			break;
		case SCRIBE_ERR_DUPLICATE_MIGRATION_VERSION:
			(void)fputs("scribe: two migration files share the same version number\n", stderr);
			break;
		case SCRIBE_ERR_SQL:
			(void)fputs("scribe: a migration failed to apply\n", stderr);
			break;
		default:
			(void)fprintf(stderr, "scribe: failed to initialise root at '%s'\n", args->rootPath);
			break;
	}
}

enum ScribeError runInitCommand(const struct ScribeArgs* args) {
	if (args == NULL) {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	if (args->commandArgc != 0) {
		(void)fputs("scribe: init does not accept arguments yet\n", stderr);
		return SCRIBE_ERR_UNEXPECTED_ARGUMENT;
	}

	struct ScribeRoot root;
	enum ScribeError err = initScribeRoot(&root, args->rootPath);
	if (err != SCRIBE_OK) {
		printInitError(err, args);
		return err;
	}

	err = createScribeRootLayout(&root, g_kScribeDefaultMigrationsDir);
	if (err != SCRIBE_OK) {
		printInitError(err, args);
		return err;
	}

	(void)fprintf(stdout, "scribe: initialised root layout at '%s'\n", args->rootPath);
	return SCRIBE_OK;
}
