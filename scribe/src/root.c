#include "root.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "fs.h"
#include "migrations.h"

static const char* const g_kScribeRootDirectories[] = {"library",
                                                       "library/generations",
                                                       "library/generations/system",
                                                       "library/generations/system/0",
                                                       "library/pins",
                                                       "library/pins/manual",
                                                       "library/profiles",
                                                       "library/tmp",
                                                       "library/works",
                                                       "var",
                                                       "var/builds",
                                                       "var/cache",
                                                       "var/cache/scribe",
                                                       "var/gcroots",
                                                       "var/lib",
                                                       "var/lib/scribe",
                                                       "var/lib/scribe/locks",
                                                       "var/lib/scribe/recipes",
                                                       "var/lib/scribe/state",
                                                       "var/lib/scribe/transactions",
                                                       "var/log",
                                                       "var/log/scribe",
                                                       "var/sources",
                                                       "var/spool",
                                                       "var/tmp"};

static const size_t g_kScribeRootDirectoryCount
    = sizeof(g_kScribeRootDirectories) / sizeof(g_kScribeRootDirectories[0]);

static const char* const g_kScribeSystemProfileCurrentLink = "library/profiles/system";
static const char* const g_kScribeSystemProfileInitialGenerationLinkTarget = "../generations/system/0";

enum ScribeError initScribeRoot(struct ScribeRoot* root, const char* path) {
	if (root == NULL || path == NULL || path[0] == '\0') {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	root->path = path;
	return SCRIBE_OK;
}

enum ScribeError makeScribeRootPath(const struct ScribeRoot* root, const char* childPath, char** outPath) {
	if (root == NULL) {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	return makeJoinedPath(root->path, childPath, outPath);
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

	err = makeDirectory(path, 0755);
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

	err = makeSymlink(targetPath, fullLinkPath);

	free(fullLinkPath);
	return err;
}

enum ScribeError createScribeRootLayout(const struct ScribeRoot* root, const char* migrationsDir) {
	if (root == NULL || root->path == NULL || root->path[0] == '\0') {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	if (migrationsDir == NULL || migrationsDir[0] == '\0') {
		return SCRIBE_ERR_INVALID_ARGUMENT;
	}

	enum ScribeError err = makeDirectory(root->path, 0755);
	if (err != SCRIBE_OK) {
		return err;
	}

	for (size_t i = 0; i < g_kScribeRootDirectoryCount; ++i) {
		err = makeChildDirectory(root, g_kScribeRootDirectories[i]);
		if (err != SCRIBE_OK) {
			return err;
		}
	}

	err = makeScribeRootSymlink(root,
	                            g_kScribeSystemProfileInitialGenerationLinkTarget,
	                            g_kScribeSystemProfileCurrentLink);
	if (err != SCRIBE_OK) {
		return err;
	}

	char* dbPath = NULL;
	err = makeScribeRootPath(root, "var/lib/scribe/ledger.sqlite", &dbPath);
	if (err != SCRIBE_OK) {
		return err;
	}

	err = applyMigrations(dbPath, migrationsDir);
	free(dbPath);
	return err;
}
