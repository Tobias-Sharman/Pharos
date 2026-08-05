CREATE TABLE gc_roots (
    id INTEGER PRIMARY KEY,
    work_id INTEGER NOT NULL REFERENCES works (id),
    reason TEXT NOT NULL CHECK (reason <> ''),
    created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now'))
);
