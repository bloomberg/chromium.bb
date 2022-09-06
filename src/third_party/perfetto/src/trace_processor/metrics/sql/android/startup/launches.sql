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

-- The start of the launching event corresponds to the end of the AM handling
-- the startActivity intent, whereas the end corresponds to the first frame drawn.
-- Only successful app launches have a launching event.
DROP TABLE IF EXISTS launching_events;
CREATE TABLE launching_events AS
SELECT
  ts,
  dur,
  ts + dur AS ts_end,
  STR_SPLIT(s.name, ": ", 1) AS package_name
FROM slice s
JOIN process_track t ON s.track_id = t.id
JOIN process USING(upid)
WHERE s.name GLOB 'launching: *'
AND (process.name IS NULL OR process.name = 'system_server');

SELECT CREATE_FUNCTION(
  'SLICE_COUNT(slice_glob STRING)',
  'INT',
  'SELECT COUNT(1) FROM slice WHERE name GLOB $slice_glob'
);

-- All activity launches in the trace, keyed by ID.
-- Populated by different scripts depending on the platform version / contents.
-- See android/startup/launches*.sql
DROP TABLE IF EXISTS launches;
CREATE TABLE launches(
  id INTEGER PRIMARY KEY,
  ts BIG INT,
  ts_end BIG INT,
  dur BIG INT,
  package STRING
);

-- Note: on Q, we didn't have Android fingerprints but we *did*
-- have ActivityMetricsLogger events so we will use this approach
-- if we see any such events.
SELECT CASE
  WHEN SLICE_COUNT('launchingActivity#*:*') > 0
    THEN RUN_METRIC('android/startup/launches_minsdk33.sql')
  WHEN SLICE_COUNT('MetricsLogger:*') > 0
    THEN RUN_METRIC('android/startup/launches_minsdk29.sql')
  ELSE RUN_METRIC('android/startup/launches_maxsdk28.sql')
END;

-- Maps a launch to the corresponding set of processes that handled the
-- activity start. The vast majority of cases should be a single process.
-- However it is possible that the process dies during the activity launch
-- and is respawned.
DROP TABLE IF EXISTS launch_processes;
CREATE TABLE launch_processes(launch_id INT, upid BIG INT, launch_type STRING);

SELECT CREATE_FUNCTION(
  'STARTUP_SLICE_COUNT(start_ts LONG, end_ts LONG, utid INT, name STRING)',
  'INT',
  '
    SELECT COUNT(1)
    FROM thread_track t
    JOIN slice s ON s.track_id = t.id
    WHERE
      t.utid = $utid AND
      s.ts >= $start_ts AND
      s.ts < $end_ts AND
      s.name = $name
  '
);

INSERT INTO launch_processes(launch_id, upid, launch_type)
SELECT
  l.id AS launch_id,
  p.upid,
  CASE
    WHEN STARTUP_SLICE_COUNT(l.ts, l.ts_end, t.utid, 'bindApplication') > 0
      THEN 'cold'
    WHEN STARTUP_SLICE_COUNT(l.ts, l.ts_end, t.utid, 'activityStart') > 0
      THEN 'warm'
    WHEN STARTUP_SLICE_COUNT(l.ts, l.ts_end, t.utid, 'activityResume') > 0
      THEN 'hot'
    ELSE NULL
  END AS launch_type
FROM launches l
LEFT JOIN package_list ON (l.package = package_list.package_name)
JOIN process p ON (l.package = p.name OR p.uid = package_list.uid)
JOIN thread t ON (p.upid = t.upid AND t.is_main_thread)
WHERE launch_type IS NOT NULL;
