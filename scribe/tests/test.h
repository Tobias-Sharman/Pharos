#ifndef SCRIBE_TEST_H
#define SCRIBE_TEST_H

int stringsEqual(const char* left, const char* right);
const char* nullableString(const char* text);

int runArgsTests(void);
int runRootTests(void);

#endif
