CREATE TABLE scribe_migrations (
    version INTEGER PRIMARY KEY CHECK (version >= 0),
    filename TEXT NOT NULL CHECK (filename <> ''),
    status TEXT NOT NULL CHECK (status IN ('applied', 'failed')),
    applied_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now'))
);
