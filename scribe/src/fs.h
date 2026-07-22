#ifndef SCRIBE_FS_H
#define SCRIBE_FS_H

#include <sys/types.h>

#include <scribe/error.h>

enum ScribeError makeJoinedPath(const char* basePath, const char* childPath, char** outPath);

enum ScribeError makeDirectory(const char* path, mode_t mode);

enum ScribeError readFile(const char* path, char** outBuffer, size_t* outSize);

#endif // SCRIBE_FS_H
