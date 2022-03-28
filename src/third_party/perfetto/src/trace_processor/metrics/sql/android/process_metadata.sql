--
-- Copyright 2019 The Android Open Source Project
--
-- Licensed under the Apache License, Version 2.0 (the "License");
-- you may not use this file except in compliance with the License.
-- You may obtain a copy of the License at
--
--     https://www.apache.org/licenses/LICENSE-2.0
--
-- Unless required by applicable law or agreed to in writing, software
-- distributed under the License is distributed on an "AS IS" BASIS,
-- WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
-- See the License for the specific language governing permissions and
-- limitations under the License.
--

DROP TABLE IF EXISTS uid_package_count;

CREATE TABLE uid_package_count AS
SELECT uid, COUNT(1) AS cnt
FROM package_list
GROUP BY 1;

DROP TABLE IF EXISTS process_metadata_table;

CREATE TABLE process_metadata_table AS
SELECT
  process.upid,
  -- workaround for b/169226092: the bug has been fixed it Android T, but
  -- we support ingesting traces from older Android versions.
  CASE
      -- cmdline gets rewritten after fork, if these are still there we must
      -- have seen a racy capture.
    WHEN length(process.name) = 15 AND (
      process.cmdline in ('zygote', 'zygote64', '<pre-initialized>')
      OR process.cmdline GLOB '*' || process.name)
    THEN process.cmdline
    ELSE process.name
  END AS process_name,
  process.android_appid AS uid,
  CASE WHEN uid_package_count.cnt > 1 THEN TRUE ELSE NULL END AS shared_uid,
  plist.package_name,
  plist.version_code,
  plist.debuggable
FROM process
LEFT JOIN uid_package_count ON process.android_appid = uid_package_count.uid
LEFT JOIN package_list plist
ON (
  process.android_appid = plist.uid
  AND uid_package_count.uid = plist.uid
  AND (
    -- unique match
    uid_package_count.cnt = 1
    -- or process name starts with the package name
    OR process.name GLOB plist.package_name || '*')
  );

DROP VIEW IF EXISTS process_metadata;

CREATE VIEW process_metadata AS
WITH upid_packages AS (
  SELECT
  upid,
  RepeatedField(AndroidProcessMetadata_Package(
    'package_name', package_list.package_name,
    'apk_version_code', package_list.version_code,
    'debuggable', package_list.debuggable
  )) packages_for_uid
  FROM process
  JOIN package_list ON process.android_appid = package_list.uid
  GROUP BY upid
)
SELECT
  upid,
  NULL_IF_EMPTY(AndroidProcessMetadata(
    'name', process_name,
    'uid', uid,
    'package', NULL_IF_EMPTY(AndroidProcessMetadata_Package(
      'package_name', package_name,
      'apk_version_code', version_code,
      'debuggable', debuggable
    )),
    'packages_for_uid', packages_for_uid
  )) AS metadata
FROM process_metadata_table
LEFT JOIN upid_packages USING (upid);
