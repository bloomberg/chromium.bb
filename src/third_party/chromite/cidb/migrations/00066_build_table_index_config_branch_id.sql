CREATE INDEX build_table_config_branch_id_index ON buildTable (
  build_config,
  branch,
  id DESC);

INSERT INTO schemaVersionTable (schemaVersion, scriptName) VALUES
  (66, '00066_build_table_index_config_branch_id.sql');
