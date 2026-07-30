#include "test.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include <scribe/error.h>

#include "root.h"

struct RootPathJoinCase {
	const char* name;
	const char* rootPath;
	const char* childPath;
	const char* expectedPath;
};

struct RootPathInvalidCase {
	const char* name;
	const struct ScribeRoot* root;
	const char* childPath;
};

static void printRootPathJoinFailure(const struct RootPathJoinCase* testCase,
                                     enum ScribeError actualError,
                                     const char* actualPath) {
	(void)fprintf(stderr, "  FAIL: root path join case: %s\n", testCase->name);
	(void)fprintf(stderr,
	              "    input: rootPath=%s, childPath=%s\n",
	              nullableString(testCase->rootPath),
	              nullableString(testCase->childPath));
	(void)fprintf(stderr, "      expected: error=%d, path=%s\n", SCRIBE_OK, nullableString(testCase->expectedPath));
	(void)fprintf(stderr, "      actual:   error=%d, path=%s\n", actualError, nullableString(actualPath));
}

static int checkRootPathJoinCase(const struct RootPathJoinCase* testCase) {
	struct ScribeRoot root;
	char* actualPath = NULL;
	int failed = 0;

	enum ScribeError actualError = initScribeRoot(&root, testCase->rootPath);

	if (actualError == SCRIBE_OK) {
		actualError = makeScribeRootPath(&root, testCase->childPath, &actualPath);
	}

	if (actualError != SCRIBE_OK) {
		failed = 1;
	}

	if (!stringsEqual(actualPath, testCase->expectedPath)) {
		failed = 1;
	}

	if (failed != 0) {
		printRootPathJoinFailure(testCase, actualError, actualPath);
	}

	free(actualPath);
	return failed;
}

static int runRootPathJoinCases(void) {
	struct RootPathJoinCase cases[] = {
	    {
	        .name = "relative root",
	        .rootPath = "./root",
	        .childPath = "store",
	        .expectedPath = "./root/store",
	    },
	    {
	        .name = "trailing slash",
	        .rootPath = "./root/",
	        .childPath = "store",
	        .expectedPath = "./root/store",
	    },
	    {
	        .name = "repeated trailing slash",
	        .rootPath = "./root//",
	        .childPath = "var/tmp",
	        .expectedPath = "./root/var/tmp",
	    },
	    {
	        .name = "filesystem root",
	        .rootPath = "/",
	        .childPath = "store",
	        .expectedPath = "/store",
	    },
	};

	int failureCount = 0;

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
		failureCount += checkRootPathJoinCase(&cases[i]);
	}

	return failureCount;
}

static void printRootPathInvalidFailure(const struct RootPathInvalidCase* testCase,
                                        enum ScribeError actualError,
                                        const char* actualPath) {
	(void)fprintf(stderr, "  FAIL: root path invalid case: %s\n", testCase->name);
	(void)fprintf(stderr,
	              "    input: rootPath=%s, childPath=%s\n",
	              testCase->root == NULL ? "(null root)" : nullableString(testCase->root->path),
	              nullableString(testCase->childPath));
	(void)fprintf(stderr, "      expected: error=%d, path=(null)\n", SCRIBE_ERR_INVALID_ARGUMENT);
	(void)fprintf(stderr, "      actual:   error=%d, path=%s\n", actualError, nullableString(actualPath));
}

static int checkRootPathInvalidCase(const struct RootPathInvalidCase* testCase) {
	char* actualPath = NULL;
	int failed = 0;

	enum ScribeError actualError = makeScribeRootPath(testCase->root, testCase->childPath, &actualPath);

	if (actualError != SCRIBE_ERR_INVALID_ARGUMENT) {
		failed = 1;
	}

	if (actualPath != NULL) {
		failed = 1;
	}

	if (failed != 0) {
		printRootPathInvalidFailure(testCase, actualError, actualPath);
	}

	free(actualPath);
	return failed;
}

static int runRootPathInvalidCases(void) {
	struct ScribeRoot validRoot = {"./root"};
	struct ScribeRoot nullPathRoot = {NULL};
	struct ScribeRoot emptyPathRoot = {""};

	struct RootPathInvalidCase cases[] = {
	    {
	        .name = "null root",
	        .root = NULL,
	        .childPath = "store",
	    },
	    {
	        .name = "root null path",
	        .root = &nullPathRoot,
	        .childPath = "store",
	    },
	    {
	        .name = "empty root path",
	        .root = &emptyPathRoot,
	        .childPath = "store",
	    },
	    {
	        .name = "null child path",
	        .root = &validRoot,
	        .childPath = NULL,
	    },
	    {
	        .name = "empty child path",
	        .root = &validRoot,
	        .childPath = "",
	    },
	    {
	        .name = "absolute child path",
	        .root = &validRoot,
	        .childPath = "/store",
	    },
	};

	int failureCount = 0;

	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
		failureCount += checkRootPathInvalidCase(&cases[i]);
	}

	return failureCount;
}

static int runRootInitInvalidCases(void) {
	struct ScribeRoot root;
	int failureCount = 0;

	if (initScribeRoot(NULL, "./root") != SCRIBE_ERR_INVALID_ARGUMENT) {
		(void)fputs("  FAIL: root init invalid case: null root\n", stderr);
		++failureCount;
	}

	if (initScribeRoot(&root, NULL) != SCRIBE_ERR_INVALID_ARGUMENT) {
		(void)fputs("  FAIL: root init invalid case: null path\n", stderr);
		++failureCount;
	}

	if (initScribeRoot(&root, "") != SCRIBE_ERR_INVALID_ARGUMENT) {
		(void)fputs("  FAIL: root init invalid case: empty path\n", stderr);
		++failureCount;
	}

	return failureCount;
}

static int runRootPathNullOutPathCase(void) {
	struct ScribeRoot root = {"./root"};
	enum ScribeError actualError = makeScribeRootPath(&root, "store", NULL);

	if (actualError != SCRIBE_ERR_INVALID_ARGUMENT) {
		(void)fprintf(stderr,
		              "  FAIL: root path null out path: expected error=%d, actual error=%d\n",
		              SCRIBE_ERR_INVALID_ARGUMENT,
		              actualError);
		return 1;
	}

	return 0;
}

int runRootTests(void) {
	int failureCount = 0;

	failureCount += runRootPathJoinCases();
	failureCount += runRootPathInvalidCases();
	failureCount += runRootInitInvalidCases();
	failureCount += runRootPathNullOutPathCase();

	return failureCount;
}
