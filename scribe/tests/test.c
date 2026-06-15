#include "test.h"

#include <stdio.h>
#include <string.h>

int stringsEqual(const char* left, const char* right) {
	if (left == NULL && right == NULL) {
		return 1;
	}

	if (left == NULL || right == NULL) {
		return 0;
	}

	return strcmp(left, right) == 0;
}

const char* nullableString(const char* text) {
	if (text == NULL) {
		return "(null)";
	}

	return text;
}

static int runTestGroup(const char* name, int (*runTests)(void)) {
	int failureCount = runTests();

	if (failureCount != 0) {
		(void)fprintf(stderr, "FAIL: %s: %d failure(s)\n", name, failureCount);
		return failureCount;
	}

	(void)printf("PASS: %s\n", name);

	return 0;
}

int main(void) {
	int failureCount = 0;

	failureCount += runTestGroup("args", runArgsTests);
	failureCount += runTestGroup("root", runRootTests);

	if (failureCount != 0) {
		(void)fprintf(stderr, "FAIL: unit tests: %d failure(s)\n", failureCount);
		return 1;
	}

	(void)puts("PASS: unit tests");

	return 0;
}
