cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(Scribe
	VERSION 0.1.0
	DESCRIPTION "Package manager for PharOS"
	LANGUAGES C
)

add_executable(scribe
	src/args.c
	src/fs.c
	src/main.c
	src/migrations.c
	src/queries.c
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
	PUBLIC
		${CMAKE_CURRENT_SOURCE_DIR}/include
	PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/src
)

find_package(SQLite3 REQUIRED)

target_link_libraries(scribe
	PRIVATE
		SQLite3::SQLite3
)

###############################################################################

enable_testing()

# TODO: Make scribe library so no need to duplicate stuff
add_executable(test_scribe
	tests/test.c
	tests/test_args.c
	tests/test_root.c

	src/args.c
	src/fs.c
	src/migrations.c
	src/queries.c
	src/root.c
	src/runner.c
	src/commands/init.c
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

find_package(SQLite3 REQUIRED)

target_link_libraries(test_scribe
	PRIVATE
		SQLite3::SQLite3
)

add_test(NAME scribe_unit COMMAND test_scribe)
