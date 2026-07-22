cmake_minimum_required(VERSION 3.13)

project(Scribe
	VERSION 0.1.0
	DESCRIPTION "Package manager for PharOS"
	LANGUAGES C
)

add_executable(scribe
	src/main.c
	src/args.c
	src/migrations.c
	src/db.c
	src/fs.c
	src/root.c
	src/runner.c
	src/commands/init.c
)

target_compile_features(scribe
	PRIVATE
		c_std_11
)

set_target_properties(scribe
	PROPERTIES
		C_EXTENSIONS OFF
)

target_include_directories(scribe
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/include
)

###############################################################################

enable_testing()

add_executable(test_scribe
	tests/test.c
	tests/test_args.c
	tests/test_root.c
	src/args.c
	src/root.c
)

target_compile_features(test_scribe
	PRIVATE
		c_std_11
)

set_target_properties(test_scribe
	PROPERTIES
		C_EXTENSIONS OFF
)

target_include_directories(test_scribe
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${CMAKE_CURRENT_SOURCE_DIR}/src
		${CMAKE_CURRENT_SOURCE_DIR}/tests
)

add_test(NAME scribe_unit COMMAND test_scribe)
