#include "root.h"

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static const char* const g_kScribeRootDirectories[] = {"store",
                                                       "var",
                                                       "var/profiles",
                                                       "var/profiles/system",
                                                       "var/profiles/system/generations",
                                                       "var/profiles/system/generations/0",
                                                       "var/gcroots",
                                                       "var/sources",
                                                       "var/builds",
                                                       "var/logs",
                                                       "var/tmp"};

static const size_t g_kScribeRootDirectoryCount
    = sizeof(g_kScribeRootDirectories) / sizeof(g_kScribeRootDirectories[0]);

static const char* const g_kScribeSystemProfileCurrentLink = "var/profiles/system/current";
static const char* const g_kScribeSystemProfileInitialGenerationLinkTarget = "generations/0";

enum ScribeError initScribeRoot(struct ScribeRoot* root, const char* path) {
	if (root == NULL || path == NULL || path[0] == '\0') {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	root->path = path;
	return SCRIBE_OK;
}

enum ScribeError makeScribeRootPath(const struct ScribeRoot* root, const char* childPath, char** outpath) {
	if (outpath == NULL) {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	*outpath = NULL;

	if (root == NULL || root->path == NULL || root->path[0] == '\0' || childPath == NULL || childPath[0] == '\0'
	    || childPath[0] == '/') {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	size_t rootLength = strlen(root->path);
	size_t childLength = strlen(childPath);

	while (rootLength > 1 && root->path[rootLength - 1] == '/') {
		--rootLength;
	}

	size_t separatorLength = 1;
	if (rootLength == 1 && root->path[0] == '/') {
		separatorLength = 0;
	}

	if (rootLength > SIZE_MAX - separatorLength) {
		return SCRIBE_ERR_OUT_OF_MEMORY;
	}

	size_t prefixLength = rootLength + separatorLength;
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

	memcpy(path, root->path, allocationSize);

	if (separatorLength == 1) {
		path[rootLength] = '/';
		rootLength++;
	}

	memcpy(path + rootLength, childPath, childLength);
	path[pathLength] = '\0';

	*outpath = path;
	return SCRIBE_OK;
}

// TODO: Add support for nested directory path (-p flag), do full composite path as ought to be interesting
static enum ScribeError makeDirectory(const char* path) {
	if (path == NULL) {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	// TODO: Adjust permissions
	if (mkdir(path, 0755) == 0) {
		return SCRIBE_OK;
	}

	if (errno == EEXIST) {
		return SCRIBE_OK;
	}

	return SCRIBE_ERR_ROOT_CREATE_FAILED;
}

static enum ScribeError makeChildDirectory(const struct ScribeRoot* root, const char* childPath) {
	if (root == NULL || childPath == NULL) {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	char* path = NULL;

	enum ScribeError err = makeScribeRootPath(root, childPath, &path);
	if (err != SCRIBE_OK) {
		return err;
	}

	err = makeDirectory(path);
	free(path);

	return err;
}

static enum ScribeError makeScribeRootSymlink(const struct ScribeRoot* root,
                                              const char* targetPath,
                                              const char* linkPath) {
	if (root == NULL || targetPath == NULL || linkPath == NULL) {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	char* fullLinkPath = NULL;

	enum ScribeError err = makeScribeRootPath(root, linkPath, &fullLinkPath);
	if (err != SCRIBE_OK) {
		return err;
	}

	if (symlink(targetPath, fullLinkPath) == 0) {
		err = SCRIBE_OK;
		goto cleanup;
	}

	if (errno == EEXIST) {
		err = SCRIBE_OK;
		goto cleanup;
	}

	err = SCRIBE_ERR_ROOT_CREATE_FAILED;

cleanup:
	free(fullLinkPath);
	return err;
}

enum ScribeError createScribeRootLayout(const struct ScribeRoot* root) {
	if (root == NULL || root->path == NULL || root->path[0] == '\0') {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	enum ScribeError err = makeDirectory(root->path);
	if (err != SCRIBE_OK) {
		return err;
	}

	for (size_t i = 0; i < g_kScribeRootDirectoryCount; ++i) {
		err = makeChildDirectory(root, g_kScribeRootDirectories[i]);
		if (err != SCRIBE_OK) {
			return err;
		}
	}

	return makeScribeRootSymlink(root,
	                             g_kScribeSystemProfileInitialGenerationLinkTarget,
	                             g_kScribeSystemProfileCurrentLink);
}
