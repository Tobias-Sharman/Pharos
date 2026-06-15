#include "test.h"

#include <stddef.h>
#include <stdio.h>

#include <scribe/args.h>
#include <scribe/config.h>
#include <scribe/error.h>

struct ArgsParseCase {
	const char* name;

	int argc;
	char** argv;

	enum ScribeError expectedError;
	enum ScribeCommand expectedCommand;

	const char* expectedRootPath;
	const char* expectedErrorText;

	int expectedCommandArgc;
	const char* expectedFirstCommandArg;
};

static void printArgv(int argc, char** argv) {
	for (int i = 0; i < argc; ++i) {
		if (i != 0) {
			(void)fputc(' ', stderr);
		}

		(void)fputs(argv[i], stderr);
	}
}

static void printArgsParseFailure(const struct ArgsParseCase* testCase,
                                  enum ScribeError actualError,
                                  const struct ScribeArgs* actualArgs) {
	(void)fprintf(stderr, "  FAIL: args parse case: %s\n", testCase->name);

	(void)fputs("    input:    ", stderr);
	printArgv(testCase->argc, testCase->argv);
	(void)fputc('\n', stderr);

	(void)fprintf(
	    stderr,
	    "      expected: error=%d, command=%d, rootPath=%s, errorText=%s, commandArgc=%d, firstCommandArg=%s\n",
	    testCase->expectedError,
	    testCase->expectedCommand,
	    nullableString(testCase->expectedRootPath),
	    nullableString(testCase->expectedErrorText),
	    testCase->expectedCommandArgc,
	    nullableString(testCase->expectedFirstCommandArg));

	(void)fprintf(
	    stderr,
	    "      actual:   error=%d, command=%d, rootPath=%s, errorText=%s, commandArgc=%d, firstCommandArg=%s\n",
	    actualError,
	    actualArgs->command,
	    nullableString(actualArgs->rootPath),
	    nullableString(actualArgs->errorText),
	    actualArgs->commandArgc,
	    actualArgs->commandArgc > 0 ? nullableString(actualArgs->commandArgv[0]) : "(null)");
}

static int checkArgsParseCase(const struct ArgsParseCase* testCase) {
	struct ScribeArgs args;

	enum ScribeError actualError = parseScribeArgs(testCase->argc, testCase->argv, &args);

	const char* actualFirstCommandArg = NULL;
	if (args.commandArgc > 0 && args.commandArgv != NULL) {
		actualFirstCommandArg = args.commandArgv[0];
	}

	int failed = 0;

	if (actualError != testCase->expectedError) {
		failed = 1;
	}

	if (args.command != testCase->expectedCommand) {
		failed = 1;
	}

	if (!stringsEqual(args.rootPath, testCase->expectedRootPath)) {
		failed = 1;
	}

	if (!stringsEqual(args.errorText, testCase->expectedErrorText)) {
		failed = 1;
	}

	if (args.commandArgc != testCase->expectedCommandArgc) {
		failed = 1;
	}

	if (!stringsEqual(actualFirstCommandArg, testCase->expectedFirstCommandArg)) {
		failed = 1;
	}

	if (failed != 0) {
		printArgsParseFailure(testCase, actualError, &args);
		return 1;
	}

	return 0;
}

static int runArgsParseCases(void) {
	struct ArgsParseCase cases[] = {
	    {
	        .name = "init command",
	        .argc = 2,
	        .argv = (char*[]){"scribe", "init"},
	        .expectedError = SCRIBE_OK,
	        .expectedCommand = SCRIBE_COMMAND_INIT,
	        .expectedRootPath = SCRIBE_DEFAULT_ROOT,
	        .expectedErrorText = NULL,
	        .expectedCommandArgc = 0,
	        .expectedFirstCommandArg = NULL,
	    },
	    {
	        .name = "root option",
	        .argc = 4,
	        .argv = (char*[]){"scribe", "--root", "./test-root", "init"},
	        .expectedError = SCRIBE_OK,
	        .expectedCommand = SCRIBE_COMMAND_INIT,
	        .expectedRootPath = "./test-root",
	        .expectedErrorText = NULL,
	        .expectedCommandArgc = 0,
	        .expectedFirstCommandArg = NULL,
	    },
	    {
	        .name = "double dash stops global options",
	        .argc = 3,
	        .argv = (char*[]){"scribe", "--", "init"},
	        .expectedError = SCRIBE_OK,
	        .expectedCommand = SCRIBE_COMMAND_INIT,
	        .expectedRootPath = SCRIBE_DEFAULT_ROOT,
	        .expectedErrorText = NULL,
	        .expectedCommandArgc = 0,
	        .expectedFirstCommandArg = NULL,
	    },
	    {
	        .name = "double dash treats option as command",
	        .argc = 4,
	        .argv = (char*[]){"scribe", "--", "--root", "init"},
	        .expectedError = SCRIBE_ERR_UNKNOWN_COMMAND,
	        .expectedCommand = SCRIBE_COMMAND_UNKNOWN,
	        .expectedRootPath = SCRIBE_DEFAULT_ROOT,
	        .expectedErrorText = "--root",
	        .expectedCommandArgc = 1,
	        .expectedFirstCommandArg = "init",
	    },
	    {
	        .name = "missing command",
	        .argc = 1,
	        .argv = (char*[]){"scribe"},
	        .expectedError = SCRIBE_ERR_MISSING_COMMAND,
	        .expectedCommand = SCRIBE_COMMAND_UNKNOWN,
	        .expectedRootPath = SCRIBE_DEFAULT_ROOT,
	        .expectedErrorText = NULL,
	        .expectedCommandArgc = 0,
	        .expectedFirstCommandArg = NULL,
	    },
	    {
	        .name = "unknown command",
	        .argc = 2,
	        .argv = (char*[]){"scribe", "unknown"},
	        .expectedError = SCRIBE_ERR_UNKNOWN_COMMAND,
	        .expectedCommand = SCRIBE_COMMAND_UNKNOWN,
	        .expectedRootPath = SCRIBE_DEFAULT_ROOT,
	        .expectedErrorText = "unknown",
	        .expectedCommandArgc = 0,
	        .expectedFirstCommandArg = NULL,
	    },
	    {
	        .name = "missing root value",
	        .argc = 2,
	        .argv = (char*[]){"scribe", "--root"},
	        .expectedError = SCRIBE_ERR_MISSING_OPTION_VALUE,
	        .expectedCommand = SCRIBE_COMMAND_UNKNOWN,
	        .expectedRootPath = SCRIBE_DEFAULT_ROOT,
	        .expectedErrorText = "--root",
	        .expectedCommandArgc = 0,
	        .expectedFirstCommandArg = NULL,
	    },
	    {
	        .name = "unknown option",
	        .argc = 3,
	        .argv = (char*[]){"scribe", "--bad-option", "init"},
	        .expectedError = SCRIBE_ERR_UNKNOWN_OPTION,
	        .expectedCommand = SCRIBE_COMMAND_UNKNOWN,
	        .expectedRootPath = SCRIBE_DEFAULT_ROOT,
	        .expectedErrorText = "--bad-option",
	        .expectedCommandArgc = 0,
	        .expectedFirstCommandArg = NULL,
	    },
	    {
	        .name = "command args preserved",
	        .argc = 3,
	        .argv = (char*[]){"scribe", "init", "extra"},
	        .expectedError = SCRIBE_OK,
	        .expectedCommand = SCRIBE_COMMAND_INIT,
	        .expectedRootPath = SCRIBE_DEFAULT_ROOT,
	        .expectedErrorText = NULL,
	        .expectedCommandArgc = 1,
	        .expectedFirstCommandArg = "extra",
	    },
	};

	int failureCount = 0;

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
		failureCount += checkArgsParseCase(&cases[i]);
	}

	return failureCount;
}

static int runArgsInvalidInputCases(void) {
	char* argv[] = {"scribe", "init"};
	struct ScribeArgs args;

	int failureCount = 0;

	if (parseScribeArgs(2, NULL, &args) != SCRIBE_ERR_INVALID_ARGUMENT) {
		(void)fputs("  FAIL: args invalid input: null argv\n", stderr);
		++failureCount;
	}

	if (parseScribeArgs(2, argv, NULL) != SCRIBE_ERR_INVALID_ARGUMENT) {
		(void)fputs("  FAIL: args invalid input: null out args\n", stderr);
		++failureCount;
	}

	return failureCount;
}

int runArgsTests(void) {
	int failureCount = 0;

	failureCount += runArgsParseCases();
	failureCount += runArgsInvalidInputCases();

	return failureCount;
}
