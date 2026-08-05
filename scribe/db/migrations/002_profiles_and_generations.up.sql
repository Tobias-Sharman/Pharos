CREATE TABLE profiles (
    id INTEGER PRIMARY KEY,
    name TEXT NOT NULL UNIQUE CHECK (name <> ''),
    path TEXT NOT NULL CHECK (path <> ''),
    profile_type TEXT NOT NULL CHECK (
        profile_type IN ('system', 'rescue', 'user')
    )
);

CREATE TABLE generations (
    id INTEGER PRIMARY KEY,
    profile_id INTEGER NOT NULL REFERENCES profiles (id),
    number INTEGER NOT NULL CHECK (number >= 0),
    is_active TEXT NOT NULL CHECK (is_active IN ('yes', 'no')) DEFAULT 'no',
    created_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    UNIQUE (profile_id, number)
);

CREATE UNIQUE INDEX idx_one_active_generation_per_profile
ON generations (profile_id)
WHERE is_active = 'yes';

CREATE TABLE generation_works (
    generation_id INTEGER NOT NULL REFERENCES generations (id),
    work_id INTEGER NOT NULL REFERENCES works (id),
    install_reason TEXT NOT NULL CHECK (
        install_reason IN ('explicit', 'dependency')
    ),
    PRIMARY KEY (generation_id, work_id)
);

CREATE TABLE pins (
    id INTEGER PRIMARY KEY,
    generation_id INTEGER NOT NULL REFERENCES generations (id),
    reason TEXT NOT NULL CHECK (reason IN ('manual', 'rescue')),
    pinned_at TEXT NOT NULL DEFAULT (strftime('%Y-%m-%dT%H:%M:%SZ', 'now')),
    UNIQUE (generation_id, reason)
);
