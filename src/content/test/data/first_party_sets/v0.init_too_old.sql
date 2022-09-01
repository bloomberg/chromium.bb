PRAGMA foreign_keys=OFF;

BEGIN TRANSACTION;

CREATE TABLE sites_to_clear (
    site TEXT NOT NULL PRIMARY KEY,
    marked_at_run INTEGER NOT NULL
) WITHOUT ROWID;

CREATE INDEX idx_marked_at_run_sites ON sites_to_clear (marked_at_run);

CREATE TABLE IF NOT EXISTS profiles_cleared (
   profile TEXT PRIMARY KEY NOT NULL,
   cleared_at_run Integer NOT NULL
) WITHOUT ROWID;

CREATE INDEX idx_cleared_at_run_profiles ON profiles_cleared (cleared_at_run);

CREATE TABLE meta(key LONGVARCHAR NOT NULL UNIQUE PRIMARY KEY, value LONGVARCHAR);

INSERT INTO meta VALUES('version','0');
INSERT INTO meta VALUES('last_compatible_version','1');
INSERT INTO meta VALUES('run_count','1');

INSERT INTO sites_to_clear VALUES('https://example.test', 1);
INSERT INTO profiles_cleared VALUES('p', 1);

COMMIT;