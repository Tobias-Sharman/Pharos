CREATE TABLE works (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL CHECK (name <> ''),
    version TEXT NOT NULL CHECK (version <> ''),
    revision INTEGER NOT NULL CHECK (revision >= 0),
    architecture TEXT NOT NULL CHECK (architecture <> ''),
    hash TEXT NOT NULL UNIQUE CHECK (hash <> ''),
    path TEXT NOT NULL CHECK (path <> ''),
    size_bytes INTEGER NOT NULL CHECK (size_bytes >= 0),
    signature TEXT NOT NULL DEFAULT '',
    built_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    UNIQUE (name, version, revision, architecture)
);

CREATE TABLE dependencies (
    work_id INTEGER NOT NULL REFERENCES works (id),
    depends_on_work_id INTEGER NOT NULL REFERENCES works (id),
    kind TEXT NOT NULL CHECK (kind IN ('build', 'runtime')),
    PRIMARY KEY (work_id, depends_on_work_id),
    CHECK (work_id <> depends_on_work_id)
);
