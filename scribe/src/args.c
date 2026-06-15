#include <scribe/args.h>

#include <stddef.h>
#include <string.h>

#include <scribe/config.h>

static void initArgs(struct ScribeArgs* args) {
	args->commandArgc = 0;
	args->commandArgv = NULL;
	args->rootPath = SCRIBE_DEFAULT_ROOT;
	args->command = SCRIBE_COMMAND_UNKNOWN;
	args->errorText = NULL;
}

static enum ScribeError parseGlobalOptions(int argc, char** argv, struct ScribeArgs* out, int* argIndex) {
	while (*argIndex < argc && argv[*argIndex][0] == '-') {
		if (strcmp(argv[*argIndex], "--") == 0) {
			++(*argIndex);
			return SCRIBE_OK;
		}

		if (strcmp(argv[*argIndex], "--root") == 0) {
			if (argc <= *argIndex + 1) {
				out->errorText = argv[*argIndex];
				return SCRIBE_ERR_MISSING_OPTION_VALUE;
			}

			out->rootPath = argv[*argIndex + 1];
			(*argIndex) += 2;
			continue;
		}

		out->errorText = argv[*argIndex];
		return SCRIBE_ERR_UNKNOWN_OPTION;
	}

	return SCRIBE_OK;
}

static enum ScribeError parseCommand(const char* text, struct ScribeArgs* out) {
	if (strcmp(text, "init") == 0) {
		out->command = SCRIBE_COMMAND_INIT;
		return SCRIBE_OK;
	}

	out->errorText = text;
	return SCRIBE_ERR_UNKNOWN_COMMAND;
}

enum ScribeError parseScribeArgs(int argc, char** argv, struct ScribeArgs* out) {
	if (out == NULL || argv == NULL) {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	int argIndex = 1; // Account for inputs starting with scribe
	initArgs(out);

	enum ScribeError err = parseGlobalOptions(argc, argv, out, &argIndex);

	if (err != SCRIBE_OK) {
		return err;
	}

	if (argc <= argIndex) {
		return SCRIBE_ERR_MISSING_COMMAND;
	}

	out->commandArgc = argc - argIndex - 1; // -1 to account for the actual cmd
	out->commandArgv = argv + argIndex + 1; // +1 for same reason
	return parseCommand(argv[argIndex], out);
}
