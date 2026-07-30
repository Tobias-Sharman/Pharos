-- name: MigrationsTableExists :one
SELECT name FROM sqlite_master
WHERE type = 'table' AND name = ?;

-- name: GetLatestMigration :one
SELECT
    version,
    status
FROM scribe_migrations
ORDER BY version DESC LIMIT 1;

-- name: InsertMigrationAttempt :exec
INSERT INTO scribe_migrations (version, filename, status) VALUES (?, ?, ?);

-- name: MarkMigrationApplied :exec
UPDATE scribe_migrations SET status = ?
WHERE version = ?;
