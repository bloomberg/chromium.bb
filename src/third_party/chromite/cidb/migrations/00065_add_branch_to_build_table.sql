ALTER TABLE buildTable
  ADD COLUMN branch varchar(255) DEFAULT NULL;

INSERT INTO schemaVersionTable (schemaVersion, scriptName) VALUES
  (65, '00065_add_branch_to_build_table.sql');
