#ifndef SCRIBE_ROOT_H
#define SCRIBE_ROOT_H

#include <scribe/error.h>

struct ScribeRoot {
	const char* path;
}; // I expect I will want to have more relevant information here so using a general stuct for now

enum ScribeError initScribeRoot(struct ScribeRoot* root, const char* path);

enum ScribeError makeScribeRootPath(const struct ScribeRoot* root, const char* childPath, char** outpath);

enum ScribeError createScribeRootLayout(const struct ScribeRoot* root);

#endif // SCRIBE_ROOT_H
