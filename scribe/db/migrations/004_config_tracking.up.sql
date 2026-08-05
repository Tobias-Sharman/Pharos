CREATE TABLE files (
    id INTEGER PRIMARY KEY,
    work_id INTEGER NOT NULL REFERENCES works (id),
    path TEXT NOT NULL CHECK (path <> ''),
    type TEXT NOT NULL CHECK (
        type IN ('binary', 'library', 'config', 'doc', 'data', 'other')
    ),
    size_bytes INTEGER NOT NULL CHECK (size_bytes >= 0),
    content_hash TEXT NOT NULL CHECK (content_hash <> ''),
    UNIQUE (work_id, path)
);

CREATE TABLE config_state (
    path TEXT PRIMARY KEY,
    base_hash TEXT NOT NULL CHECK (base_hash <> ''),
    package_hash TEXT NOT NULL CHECK (package_hash <> ''),
    disk_hash TEXT NOT NULL CHECK (disk_hash <> '')
);
