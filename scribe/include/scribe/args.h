#ifndef SCRIBE_ARGS_H
#define SCRIBE_ARGS_H

#include <scribe/error.h>

enum ScribeCommand {
	SCRIBE_COMMAND_UNKNOWN = 0,
	SCRIBE_COMMAND_INIT
};

struct ScribeArgs {
	int commandArgc;
	char** commandArgv;
	const char* rootPath;
	enum ScribeCommand command;
	const char* errorText;
};

enum ScribeError parseScribeArgs(int argc, char** argv, struct ScribeArgs* out);

#endif // SCRIBE_ARGS_H
