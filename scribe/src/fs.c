#include "fs.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "scribe/error.h"

enum ScribeError makeJoinedPath(const char* basePath, const char* childPath, char** outPath) {
	if (outPath == NULL) {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	*outPath = NULL;

	if (basePath == NULL || basePath[0] == '\0' || childPath == NULL || childPath[0] == '\0' || childPath[0] == '/') {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	size_t baseLength = strlen(basePath);
	size_t childLength = strlen(childPath);

	while (baseLength > 1 && basePath[baseLength - 1] == '/') {
		--baseLength;
	}

	size_t separatorLength = basePath[baseLength - 1] == '/' ? 0 : 1;

	if (baseLength > SIZE_MAX - separatorLength) {
		return SCRIBE_ERR_OUT_OF_MEMORY;
	}

	size_t prefixLength = baseLength + separatorLength;
	if (prefixLength > SIZE_MAX - childLength) {
		return SCRIBE_ERR_OUT_OF_MEMORY;
	}

	size_t pathLength = prefixLength + childLength;
	if (pathLength == SIZE_MAX) {
		return SCRIBE_ERR_OUT_OF_MEMORY;
	}

	size_t allocationSize = pathLength + 1; // For '\0'
	char* path = malloc(allocationSize);
	if (path == NULL) {
		return SCRIBE_ERR_OUT_OF_MEMORY;
	}

	memcpy(path, basePath, baseLength);

	if (separatorLength == 1) {
		path[baseLength] = '/';
		++baseLength;
	}

	memcpy(path + baseLength, childPath, childLength);
	path[pathLength] = '\0';

	*outPath = path;
	return SCRIBE_OK;
}

enum ScribeError makeDirectory(const char* path, mode_t mode) {
	if (path == NULL || path[0] == '\0') {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	if (mkdir(path, mode) == 0) {
		return SCRIBE_OK;
	}

	if (errno != EEXIST) {
		return SCRIBE_ERR_ROOT_CREATE_FAILED;
	}

	struct stat st;
	if (stat(path, &st) != 0) {
		return SCRIBE_ERR_ROOT_CREATE_FAILED;
	}

	if (!S_ISDIR(st.st_mode)) {
		return SCRIBE_ERR_ROOT_CREATE_FAILED;
	}

	return SCRIBE_OK;
}

enum ScribeError readFile(const char* path, char** outBuffer, size_t* outSize) {
	if (outBuffer == NULL) {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	*outBuffer = NULL;

	if (outSize != NULL) {
		*outSize = 0;
	}

	if (path == NULL || path[0] == '\0') {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	FILE* file = fopen(path, "rb");
	if (file == NULL) {
		return SCRIBE_ERR_IO;
	}

	enum ScribeError err = SCRIBE_OK;
	char* buffer = NULL;
	size_t fileSize = 0;

	if (fseek(file, 0, SEEK_END) != 0) {
		err = SCRIBE_ERR_IO;
		goto cleanup;
	}

	long fileSizeLong = ftell(file);
	if (fileSizeLong < 0) {
		err = SCRIBE_ERR_IO;
		goto cleanup;
	}

	fileSize = (size_t)fileSizeLong;
	if (fileSize == SIZE_MAX) {
		err = SCRIBE_ERR_OUT_OF_MEMORY;
		goto cleanup;
	}

	if (fseek(file, 0, SEEK_SET) != 0) {
		err = SCRIBE_ERR_IO;
		goto cleanup;
	}

	buffer = malloc(fileSize + 1);
	if (buffer == NULL) {
		err = SCRIBE_ERR_OUT_OF_MEMORY;
		goto cleanup;
	}

	size_t bytesRead = fread(buffer, 1, fileSize, file);
	if (bytesRead != fileSize) {
		err = SCRIBE_ERR_IO;
		goto cleanup;
	}

	buffer[fileSize] = '\0';

	if (fclose(file) != 0) {
		file = NULL;
		err = SCRIBE_ERR_IO;
		goto cleanup;
	}

	file = NULL;
	*outBuffer = buffer;
	buffer = NULL;

	if (outSize != NULL) {
		*outSize = fileSize;
	}

cleanup:
	free(buffer);

	if (file != NULL) {
		if (fclose(file) != 0 && err == SCRIBE_OK) {
			err = SCRIBE_ERR_IO;
		}
	}

	return err;
}
