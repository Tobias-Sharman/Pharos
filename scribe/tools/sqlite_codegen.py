import os
import sqlite3
from collections.abc import Iterator
from dataclasses import dataclass
from pathlib import Path

import sqlglot
from sqlglot import exp

VALID_MODES = {"one", "many", "exec", "execrows"}

SQLITE_TO_C_TYPE = {
    "INTEGER": "int",
    "TEXT": "char*",
    "REAL": "double",
}


@dataclass(frozen=True)
class MigrationFile:
    version: int
    filename: str
    filepath: Path


@dataclass(frozen=True)
class ColumnInfo:
    name: str
    declared_type: str
    not_null: bool
    is_primary_key: bool


@dataclass(frozen=True)
class ParsedQuery:
    name: str
    mode: str
    sql: str
    source_path: Path


@dataclass(frozen=True)
class QueryTarget:
    table: str
    result_columns: list[str]
    parameter_columns: list[str]


@dataclass(frozen=True)
class EmittedFunction:
    struct_def: str | None
    prototypes: list[str]
    definition: str


def discover_migrations(migration_path: Path) -> list[MigrationFile]:
    with os.scandir(migration_path) as contents:
        up_only = [entry.name for entry in contents if entry.name.endswith(".up.sql") and entry.is_file()]

    migrations = [
        MigrationFile(
            version=int(name.partition("_")[0]),
            filename=name,
            filepath=migration_path / name,
        )
        for name in up_only
    ]

    sorted_migrations = sorted(migrations, key=lambda m: m.version)

    for i in range(len(sorted_migrations) - 1):
        if sorted_migrations[i].version == sorted_migrations[i + 1].version:
            raise ValueError(
                f"duplicate migration version {sorted_migrations[i].version}: "
                f"{sorted_migrations[i].filename} and {sorted_migrations[i + 1].filename}"
            )

    return sorted_migrations


def discover_query_files(queries_dir: Path) -> list[Path]:
    with os.scandir(queries_dir) as contents:
        return sorted(Path(entry.path) for entry in contents if entry.name.endswith(".sql") and entry.is_file())


def check_duplicate_query_names(queries: list[ParsedQuery]) -> None:
    seen: dict[str, Path] = {}
    for query in queries:
        if query.name in seen:
            raise ValueError(
                f"duplicate query name {query.name!r}: defined in both {seen[query.name]} and {query.source_path}"
            )
        seen[query.name] = query.source_path


def build_schema(migrations: list[MigrationFile]) -> sqlite3.Connection:
    connection = sqlite3.connect(":memory:")

    for migration in migrations:
        sql = migration.filepath.read_text()
        try:
            connection.executescript(sql)
        except sqlite3.OperationalError as error:
            raise ValueError(f"migration {migration.filename} failed: {error}") from error

    return connection


def get_table_columns(connection: sqlite3.Connection, table_name: str) -> list[ColumnInfo]:
    cursor = connection.execute(f"PRAGMA table_info({table_name})")
    return [
        ColumnInfo(
            name=row[1],
            declared_type=row[2],
            not_null=bool(row[3]),
            is_primary_key=bool(row[5]),
        )
        for row in cursor.fetchall()
    ]


def parse_query_file(path: Path) -> list[ParsedQuery]:
    lines = path.read_text().splitlines()
    queries: list[ParsedQuery] = []

    i = 0
    while i < len(lines):
        stripped = lines[i].strip()

        if not stripped.startswith("-- name:"):
            i += 1
            continue

        header = stripped.removeprefix("-- name:").strip()
        name, _, mode = header.partition(":")
        name = name.strip()
        mode = mode.strip()
        i += 1

        if not name:
            raise ValueError(f"{path}: query header missing a name: {stripped!r}")

        if mode not in VALID_MODES:
            raise ValueError(f"query {name!r} in {path}: invalid mode {mode!r}, expected one of {VALID_MODES}")

        sql_lines: list[str] = []
        while True:
            if i >= len(lines):
                raise ValueError(f"query {name!r} in {path}: missing terminating ';'")

            line = lines[i]
            if line.strip().startswith("-- name:"):
                raise ValueError(f"query {name!r} in {path}: missing terminating ';' before next query")

            sql_lines.append(line)
            i += 1

            if line.strip().endswith(";"):
                break

        sql = " ".join(" ".join(sql_lines).split())
        if not sql:
            raise ValueError(f"query {name!r} in {path}: empty query body")

        queries.append(ParsedQuery(name=name, mode=mode, sql=sql, source_path=path))

    return queries


def walk_where_conditions(node: exp.Expression | None) -> Iterator[exp.Expression]:  # pyright: ignore[reportPrivateImportUsage]
    if node is None:
        return
    if isinstance(node, exp.Paren):
        yield from walk_where_conditions(node.this)
        return
    if isinstance(node, (exp.And, exp.Or)):
        yield from walk_where_conditions(node.this)
        yield from walk_where_conditions(node.expression)
        return
    if isinstance(
        node,
        (
            exp.EQ,
            exp.NEQ,
            exp.GT,
            exp.GTE,
            exp.LT,
            exp.LTE,
            exp.Like,
            exp.Between,
            exp.In,
        ),
    ):
        yield node
        return

    yield from walk_where_conditions(node.args.get("this"))
    yield from walk_where_conditions(node.args.get("expression"))


def extract_where_columns(tree: exp.Expression) -> list[str]:  # pyright: ignore[reportPrivateImportUsage]
    where = tree.args.get("where")
    if where is None:
        return []

    columns: list[str] = []

    for node in walk_where_conditions(where.this):
        if not isinstance(node.this, exp.Column):
            continue

        if isinstance(node, exp.Between):
            low = node.args.get("low")
            high = node.args.get("high")
            if isinstance(low, exp.Placeholder):
                columns.append(node.this.name)
            if isinstance(high, exp.Placeholder):
                columns.append(node.this.name)

        elif isinstance(node, exp.In):
            placeholder_count = sum(1 for value in node.expressions if isinstance(value, exp.Placeholder))
            columns.extend([node.this.name] * placeholder_count)

        elif isinstance(node.expression, exp.Placeholder):
            columns.append(node.this.name)

    return columns


def extract_target(sql: str) -> QueryTarget:
    tree = sqlglot.parse_one(sql, dialect="sqlite")

    table_node = tree.find(exp.Table)
    if table_node is None:
        raise ValueError(f"no table found in query: {sql}")
    table = table_node.name

    result_columns: list[str] = []
    parameter_columns: list[str] = []

    if isinstance(tree, exp.Select):
        result_columns = [projection.name for projection in tree.expressions if isinstance(projection, exp.Column)]
        parameter_columns = extract_where_columns(tree)

    elif isinstance(tree, exp.Insert):
        schema = tree.this
        if isinstance(schema, exp.Schema):
            parameter_columns = [column.name for column in schema.expressions]

    elif isinstance(tree, exp.Update):
        parameter_columns = [
            eq.this.name for eq in tree.expressions if isinstance(eq, exp.EQ) and isinstance(eq.this, exp.Column)
        ]
        parameter_columns += extract_where_columns(tree)

    elif isinstance(tree, exp.Delete):
        parameter_columns = extract_where_columns(tree)

    else:
        raise ValueError(f"unsupported statement type: {type(tree).__name__}")

    return QueryTarget(table=table, result_columns=result_columns, parameter_columns=parameter_columns)


def find_column(columns: list[ColumnInfo], name: str) -> ColumnInfo:
    for column in columns:
        if column.name == name:
            return column
    raise ValueError(f"column not found: {name}")


def resolve_column_type(column: ColumnInfo) -> str:
    c_type = SQLITE_TO_C_TYPE.get(column.declared_type.upper())
    if c_type is None:
        raise ValueError(f"unsupported column type: {column.declared_type} on {column.name}")
    return c_type


def get_string_columns(columns: list[str], table_columns: list[ColumnInfo]) -> list[str]:
    return [name for name in columns if resolve_column_type(find_column(table_columns, name)) == "char*"]


def emit_bind_call(index: int, c_type: str, param_name: str) -> str:
    match c_type:
        case "char*":
            return f"\tsqlite3_bind_text(stmt, {index}, {param_name}, -1, SQLITE_TRANSIENT);\n"
        case "int":
            return f"\tsqlite3_bind_int(stmt, {index}, {param_name});\n"
        case "double":
            return f"\tsqlite3_bind_double(stmt, {index}, {param_name});\n"
        case _:
            raise ValueError(f"unsupported column type: {c_type}")


def build_params_and_binds(target: QueryTarget, table_columns: list[ColumnInfo]) -> tuple[list[str], list[str]]:
    params: list[str] = []
    bind_lines: list[str] = []
    occurrence_counts: dict[str, int] = {}

    for index, column_name in enumerate(target.parameter_columns, start=1):
        column = find_column(table_columns, column_name)
        c_type = resolve_column_type(column)
        param_type = "const char*" if c_type == "char*" else c_type

        occurrence_counts[column_name] = occurrence_counts.get(column_name, 0) + 1
        occurrence = occurrence_counts[column_name]
        param_name = column_name if occurrence == 1 else f"{column_name}{occurrence}"

        params.append(f"{param_type} {param_name}")
        bind_lines.append(emit_bind_call(index, c_type, param_name))
    return params, bind_lines


def emit_read_column(index: int, c_type: str, field_name: str, target_prefix: str) -> str:
    match c_type:
        case "char*":
            return (
                f"\n"
                f"\t\tconst char* text = (const char*)sqlite3_column_text(stmt, {index});\n"
                f"\t\tif (text != NULL) {{\n"
                f"\t\t\tsize_t textLen = strlen(text) + 1;\n"
                f"\t\t\t{target_prefix}{field_name} = malloc(textLen);\n"
                f"\t\t\tif ({target_prefix}{field_name} != NULL) {{\n"
                f"\t\t\t\tmemcpy({target_prefix}{field_name}, text, textLen);\n"
                f"\t\t\t}}\n"
                f"\t\t}} else {{\n"
                f"\t\t\t{target_prefix}{field_name} = NULL;\n"
                f"\t\t}}\n"
            )
        case "int":
            return f"\t\t{target_prefix}{field_name} = sqlite3_column_int(stmt, {index});"
        case "double":
            return f"\t\t{target_prefix}{field_name} = sqlite3_column_double(stmt, {index});"
        case _:
            raise ValueError(f"unsupported column type: {c_type}")


def get_or_create_struct_name(
    query: ParsedQuery, columns: list[str], table_columns: list[ColumnInfo], struct_registry: dict[tuple[str, ...], str]
) -> tuple[str, str | None]:
    key = tuple(columns)
    if key in struct_registry:
        return struct_registry[key], None

    struct_name = f"Scribe{query.name}Row"
    struct_registry[key] = struct_name

    fields = [f"\t{resolve_column_type(find_column(table_columns, name))} {name};" for name in columns]
    definition = f"struct {struct_name} {{\n" + "\n".join(fields) + "\n};"
    return struct_name, definition


def emit_prepare_block(sql: str) -> str:
    return (
        f'\tint rc = sqlite3_prepare_v2(db, "{sql}", -1, &stmt, NULL);\n'
        "\tif (rc != SQLITE_OK) {\n"
        "\t\tresult = SCRIBE_ERR_SQL;\n"
        "\t\tgoto cleanup;\n"
        "\t}"
    )


def emit_exec(query: ParsedQuery, target: QueryTarget, table_columns: list[ColumnInfo]) -> EmittedFunction:
    function_name = f"scribe{query.name}"
    params, bind_lines = build_params_and_binds(target, table_columns)

    if query.mode == "execrows":
        params.append("int64_t* outRowsAffected")

    all_params = ", ".join(["sqlite3* db"] + params)
    signature = f"enum ScribeError {function_name}({all_params})"
    bind_block = "".join(bind_lines)
    if len(bind_block) != 0:
        bind_block = bind_block + "\n"
    prepare_block = emit_prepare_block(query.sql)

    rows_affected_block = "\n\t*outRowsAffected = sqlite3_changes64(db);\n" if query.mode == "execrows" else "\n"

    definition = (
        f"{signature} {{\n"
        "\tsqlite3_stmt* stmt = NULL;\n"
        "\tenum ScribeError result = SCRIBE_OK;\n"
        "\n"
        f"{prepare_block}\n"
        "\n"
        f"{bind_block}"
        "\trc = sqlite3_step(stmt);\n"
        "\tif (rc != SQLITE_DONE) {\n"
        "\t\tresult = SCRIBE_ERR_SQL;\n"
        "\t\tgoto cleanup;\n"
        "\t}\n"
        f"{rows_affected_block}"
        "cleanup:\n"
        "\tsqlite3_finalize(stmt);\n\n"
        "\treturn result;\n"
        "}"
    )

    return EmittedFunction(struct_def=None, prototypes=[f"{signature};"], definition=definition)


def emit_one(
    query: ParsedQuery,
    target: QueryTarget,
    table_columns: list[ColumnInfo],
    struct_registry: dict[tuple[str, ...], str],
) -> EmittedFunction:
    function_name = f"scribe{query.name}"
    struct_name, struct_def = get_or_create_struct_name(query, target.result_columns, table_columns, struct_registry)
    params, bind_lines = build_params_and_binds(target, table_columns)

    all_params = ", ".join(["sqlite3* db"] + params + [f"struct {struct_name}* out", "int* outHasRow"])
    signature = f"enum ScribeError {function_name}({all_params})"
    bind_block = "".join(bind_lines)
    if len(bind_block) != 0:
        bind_block = bind_block + "\n"
    prepare_block = emit_prepare_block(query.sql)

    read_lines = [
        emit_read_column(index, resolve_column_type(find_column(table_columns, name)), name, "out->")
        for index, name in enumerate(target.result_columns)
    ]
    read_block = "\n".join(read_lines)

    definition = (
        f"{signature} {{\n"
        "\tsqlite3_stmt* stmt = NULL;\n"
        "\tenum ScribeError result = SCRIBE_OK;\n"
        "\n"
        f"{prepare_block}\n"
        "\n"
        f"{bind_block}"
        "\trc = sqlite3_step(stmt);\n"
        "\tif (rc == SQLITE_ROW) {\n"
        f"{read_block}\n"
        "\t\t*outHasRow = 1;\n"
        "\t} else if (rc == SQLITE_DONE) {\n"
        "\t\t*outHasRow = 0;\n"
        "\t} else {\n"
        "\t\tresult = SCRIBE_ERR_SQL;\n"
        "\t\tgoto cleanup;\n"
        "\t}\n"
        "\n"
        "cleanup:\n"
        "\tsqlite3_finalize(stmt);\n\n"
        "\treturn result;\n"
        "}"
    )

    prototypes = [f"{signature};"]
    string_columns = get_string_columns(target.result_columns, table_columns)

    if string_columns:
        free_signature = f"void {function_name}RowFree(struct {struct_name}* row)"
        free_lines = [f"\tfree(row->{name});" for name in string_columns]
        free_definition = f"{free_signature} {{\n" + "\n".join(free_lines) + "\n}"
        definition = f"{definition}\n\n{free_definition}"
        prototypes.append(f"{free_signature};")

    return EmittedFunction(struct_def=struct_def, prototypes=prototypes, definition=definition)


def emit_many(
    query: ParsedQuery,
    target: QueryTarget,
    table_columns: list[ColumnInfo],
    struct_registry: dict[tuple[str, ...], str],
) -> EmittedFunction:
    function_name = f"scribe{query.name}"
    struct_name, struct_def = get_or_create_struct_name(query, target.result_columns, table_columns, struct_registry)
    params, bind_lines = build_params_and_binds(target, table_columns)

    all_params = ", ".join(["sqlite3* db"] + params + [f"struct {struct_name}** outItems", "size_t* outCount"])
    signature = f"enum ScribeError {function_name}({all_params})"
    bind_block = "".join(bind_lines)
    if len(bind_block) != 0:
        bind_block = bind_block + "\n"
    prepare_block = emit_prepare_block(query.sql)

    read_lines = [
        emit_read_column(index, resolve_column_type(find_column(table_columns, name)), name, "items[count].")
        for index, name in enumerate(target.result_columns)
    ]
    read_block = "\n".join(read_lines)

    string_columns = get_string_columns(target.result_columns, table_columns)
    free_lines = [f"\t\tfree(items[i].{name});" for name in string_columns]
    free_block = "\n".join(free_lines) if free_lines else "\t\t(void)items;"
    free_signature = f"void {function_name}Free(struct {struct_name}* items, size_t count)"

    cleanup_free_lines = [f"\t\t\tfree(items[i].{name});" for name in string_columns]
    cleanup_free_block = "\n".join(cleanup_free_lines) if cleanup_free_lines else "\t\t\t(void)items;"

    definition = (
        f"{signature} {{\n"
        "\tsqlite3_stmt* stmt = NULL;\n"
        f"\tstruct {struct_name}* items = NULL;\n"
        "\tsize_t count = 0;\n"
        "\tenum ScribeError result = SCRIBE_OK;\n"
        "\n"
        f"{prepare_block}\n"
        "\n"
        f"{bind_block}"
        "\twhile ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {\n"
        f"\t\tstruct {struct_name}* resized = realloc(items, (count + 1) * sizeof(struct {struct_name}));\n"  # TODO: Not good for lots of returns, fine for my pupose but in general bad
        "\t\tif (resized == NULL) {\n"
        "\t\t\tresult = SCRIBE_ERR_OUT_OF_MEMORY;\n"
        "\t\t\tgoto cleanup;\n"
        "\t\t}\n"
        "\t\titems = resized;\n"
        "\n"
        f"{read_block}\n"
        "\t\tcount++;\n"
        "\t}\n"
        "\n"
        "\tif (rc != SQLITE_DONE) {\n"
        "\t\tresult = SCRIBE_ERR_SQL;\n"
        "\t\tgoto cleanup;\n"
        "\t}\n"
        "\n"
        "\t*outItems = items;\n"
        "\t*outCount = count;\n"
        "\n"
        "cleanup:\n"
        "\tsqlite3_finalize(stmt);\n"
        "\tif (result != SCRIBE_OK) {\n"
        "\t\tfor (size_t i = 0; i < count; i++) {\n"
        f"{cleanup_free_block}\n"
        "\t\t}\n"
        "\t\tfree(items);\n"
        "\t}\n"
        "\treturn result;\n"
        "}\n"
        "\n"
        f"{free_signature} {{\n"
        "\tfor (size_t i = 0; i < count; i++) {\n"
        f"{free_block}\n"
        "\t}\n"
        "\tfree(items);\n"
        "}"
    )

    return EmittedFunction(
        struct_def=struct_def, prototypes=[f"{signature};", f"{free_signature};"], definition=definition
    )


def emit_function(
    query: ParsedQuery,
    target: QueryTarget,
    table_columns: list[ColumnInfo],
    struct_registry: dict[tuple[str, ...], str],
) -> EmittedFunction:
    if query.mode in ("exec", "execrows"):
        return emit_exec(query, target, table_columns)
    elif query.mode == "one":
        return emit_one(query, target, table_columns, struct_registry)
    elif query.mode == "many":
        return emit_many(query, target, table_columns, struct_registry)
    else:
        raise ValueError(f"unhandled mode: {query.mode}")


def emit_header(emitted_functions: list[EmittedFunction], header_guard: str) -> str:
    struct_defs = [ef.struct_def for ef in emitted_functions if ef.struct_def is not None]
    prototype_groups = ["\n".join(ef.prototypes) for ef in emitted_functions]

    parts = [
        f"#ifndef {header_guard}",
        f"#define {header_guard}",
        "",
        "#include <sqlite3.h>",
        "#include <stddef.h>",
        "#include <stdint.h>",
        "",
        "#include <scribe/error.h>",
        "",
    ]
    parts.extend(struct_defs)
    if struct_defs:
        parts.append("")
    parts.append("\n\n".join(prototype_groups))
    parts.append("")
    parts.append(f"#endif // {header_guard}")
    return "\n".join(parts)


def emit_source(emitted_functions: list[EmittedFunction], header_file_name: str) -> str:
    parts = [
        f'#include "{header_file_name}"',
        "",
        "#include <stdlib.h>",
        "#include <string.h>",
        "",
    ]
    parts.append("\n\n".join(ef.definition for ef in emitted_functions))
    return "\n".join(parts)


def main() -> None:
    script_dir = Path(__file__).resolve().parent
    project_dir = script_dir.parent
    migration_path = project_dir / "db" / "migrations"
    queries_dir = project_dir / "db" / "queries"
    output_dir = project_dir / "src"
    output_dir.mkdir(exist_ok=True)

    migrations = discover_migrations(migration_path)
    print(f"Discovered {len(migrations)} migration(s):")
    for migration in migrations:
        print(f"  {migration.version}: {migration.filename}")

    connection = build_schema(migrations)

    query_files = discover_query_files(queries_dir)
    print(f"\nDiscovered {len(query_files)} query file(s):")
    for query_file in query_files:
        print(f"  {query_file.name}")

    queries: list[ParsedQuery] = []
    for query_file in query_files:
        queries.extend(parse_query_file(query_file))

    check_duplicate_query_names(queries)

    print(f"\nParsed {len(queries)} quer{'y' if len(queries) == 1 else 'ies'}:")

    struct_registry: dict[tuple[str, ...], str] = {}
    emitted_functions: list[EmittedFunction] = []

    for query in queries:
        print(f"  {query.name} ({query.mode})")
        target = extract_target(query.sql)
        table_columns = get_table_columns(connection, target.table)
        emitted_functions.append(emit_function(query, target, table_columns, struct_registry))

    header_name = "queries.h"
    header_guard = "SCRIBE_QUERIES_H"

    header_path = output_dir / header_name
    source_path = output_dir / "queries.c"

    header_path.write_text(emit_header(emitted_functions, header_guard) + "\n")
    source_path.write_text(emit_source(emitted_functions, header_name) + "\n")

    print(f"\nWrote {header_path}")
    print(f"Wrote {source_path}")


if __name__ == "__main__":
    main()
