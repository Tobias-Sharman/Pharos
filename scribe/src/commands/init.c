#include "../commands.h"

#include <stdio.h>

#include "../root.h"

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

	err = createScribeRootLayout(&root);
	if (err != SCRIBE_OK) {
		printInitError(err, args);
		return err;
	}

	(void)fprintf(stdout, "scribe: initialised root layout at '%s'\n", args->rootPath);
	return SCRIBE_OK;
}
