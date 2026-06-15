#include <scribe/runner.h>

#include <stdio.h>
#include <stdlib.h>

#include <scribe/args.h>
#include <scribe/error.h>

#include "commands.h"

static void printUsage(FILE* stream) {
	(void)fputs("\n", stream);
	(void)fputs("usage: scribe [options] <command> [command-options]\n", stream);
	(void)fputs("\n", stream);
	(void)fputs("options:\n", stream);
	(void)fputs("    --root <path>    use a specific Scribe root\n", stream);
	(void)fputs("\n", stream);
	(void)fputs("commands:\n", stream);
	(void)fputs("    init             initialise a Scribe root\n", stream);
}

static int reportParseError(enum ScribeError err, const struct ScribeArgs* args) {
	switch (err) {
		case SCRIBE_ERR_INVALID_ARGUMENT:
			(void)fputs("scribe: internal argument parsing error\n", stderr);
			return EXIT_FAILURE;
		case SCRIBE_ERR_MISSING_OPTION_VALUE:
			(void)fputs("scribe: missing value for option\n", stderr);
			printUsage(stderr);
			return EXIT_FAILURE;
		case SCRIBE_ERR_UNKNOWN_OPTION:
			(void)fprintf(stderr, "scribe: unknown option '%s'\n", args->errorText);
			printUsage(stderr);
			return EXIT_FAILURE;
		case SCRIBE_ERR_MISSING_COMMAND:
			(void)fputs("scribe: no command specifed\n", stderr);
			printUsage(stderr);
			return EXIT_FAILURE;
		case SCRIBE_ERR_UNKNOWN_COMMAND:
			(void)fprintf(stderr, "scribe: unknown command '%s'\n", args->errorText);
			printUsage(stderr);
			return EXIT_FAILURE;
		default:
			(void)fputs("scribe: internal unhandled parser error\n", stderr);
			return EXIT_FAILURE;
	}
}

static enum ScribeError dispatchCommand(const struct ScribeArgs* args) {
	switch (args->command) {
		case SCRIBE_COMMAND_UNKNOWN:
			(void)fputs("scribe: internal command dispatch error", stderr);
			return SCRIBE_ERR_UNKNOWN_COMMAND;
		case SCRIBE_COMMAND_INIT:
			return runInitCommand(args);
	}
}

int runScribe(int argc, char** argv) {
	struct ScribeArgs args;
	enum ScribeError err = parseScribeArgs(argc, argv, &args);
	if (err != SCRIBE_OK) {
		reportParseError(err, &args);
	}

	err = dispatchCommand(&args);
	if (err != SCRIBE_OK) {
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
