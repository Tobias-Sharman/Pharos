CREATE TABLE transactions (
    id INTEGER PRIMARY KEY,
    transaction_type TEXT NOT NULL CHECK (
        transaction_type IN ('install', 'remove', 'upgrade', 'rollback', 'gc')
    ),
    status TEXT NOT NULL CHECK (status IN ('pending', 'applied', 'failed')),
    message TEXT NOT NULL DEFAULT '',
    details TEXT NOT NULL DEFAULT '',
    started_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    finished_at TEXT NOT NULL DEFAULT ''
);
