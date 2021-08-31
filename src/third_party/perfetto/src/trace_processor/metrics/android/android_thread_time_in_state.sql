--
-- Copyright 2020 The Android Open Source Project
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

SELECT RUN_METRIC('android/cpu_info.sql');
SELECT RUN_METRIC('android/process_metadata.sql');

DROP TABLE IF EXISTS android_thread_time_in_state_base;
CREATE TABLE android_thread_time_in_state_base AS
SELECT
  base.*,
  IFNULL(core_type_per_cpu.core_type, 'unknown') core_type
FROM (
  SELECT
    ts,
    utid,
    EXTRACT_ARG(counter.arg_set_id, 'time_in_state_cpu_id') AS
        time_in_state_cpu,
    EXTRACT_ARG(counter.arg_set_id, 'freq') AS freq,
    CAST(value AS INT) AS runtime_ms_counter
  FROM counter
  JOIN thread_counter_track ON (counter.track_id = thread_counter_track.id)
  WHERE thread_counter_track.name = 'time_in_state'
) base
LEFT JOIN core_type_per_cpu ON (cpu = time_in_state_cpu);

DROP VIEW IF EXISTS android_thread_time_in_state_raw;
CREATE VIEW android_thread_time_in_state_raw AS
SELECT
  utid,
  time_in_state_cpu,
  core_type,
  freq,
  MAX(runtime_ms_counter) - MIN(runtime_ms_counter) runtime_ms_diff
FROM android_thread_time_in_state_base
GROUP BY utid, time_in_state_cpu, core_type, freq;

DROP TABLE IF EXISTS android_thread_time_in_state_counters;
CREATE TABLE android_thread_time_in_state_counters AS
SELECT
  utid,
  raw.time_in_state_cpu,
  raw.core_type,
  SUM(runtime_ms_diff) AS runtime_ms,
  SUM(raw.freq * runtime_ms_diff / 1000000) AS mcycles,
  SUM(power * runtime_ms_diff / 3600000) AS power_profile_mah
FROM android_thread_time_in_state_raw AS raw
    LEFT OUTER JOIN cpu_cluster_power AS power USING(core_type, freq)
GROUP BY utid, raw.time_in_state_cpu, raw.core_type
HAVING runtime_ms > 0;

DROP VIEW IF EXISTS android_thread_time_in_state_thread_metrics;
CREATE VIEW android_thread_time_in_state_thread_metrics AS
SELECT
  utid,
  RepeatedField(AndroidThreadTimeInStateMetric_MetricsByCoreType(
    'time_in_state_cpu',  time_in_state_cpu,
    'core_type', core_type,
    'runtime_ms', runtime_ms,
    'mcycles', CAST(mcycles AS INT),
    'power_profile_mah', power_profile_mah
  )) metrics
FROM android_thread_time_in_state_counters
GROUP BY utid;

DROP VIEW IF EXISTS android_thread_time_in_state_threads;
CREATE VIEW android_thread_time_in_state_threads AS
SELECT
  upid,
  RepeatedField(AndroidThreadTimeInStateMetric_Thread(
    'name',
    thread.name,
    'main_thread',
    thread.is_main_thread,
    'metrics_by_core_type',
    android_thread_time_in_state_thread_metrics.metrics
  )) threads
FROM thread
JOIN android_thread_time_in_state_thread_metrics USING (utid)
GROUP BY upid;

DROP VIEW IF EXISTS android_thread_time_in_state_process_metrics;
CREATE VIEW android_thread_time_in_state_process_metrics AS
WITH process_counters AS (
  SELECT
    upid,
    time_in_state_cpu,
    core_type,
    SUM(runtime_ms) AS runtime_ms,
    SUM(mcycles) AS mcycles,
    SUM(power_profile_mah) AS power_profile_mah
  FROM android_thread_time_in_state_counters
  JOIN thread USING (utid)
  GROUP BY upid, time_in_state_cpu, core_type
)
SELECT
  upid,
  RepeatedField(AndroidThreadTimeInStateMetric_MetricsByCoreType(
    'time_in_state_cpu', time_in_state_cpu,
    'core_type', core_type,
    'runtime_ms', runtime_ms,
    'mcycles', CAST(mcycles AS INT),
    'power_profile_mah', power_profile_mah
 )) metrics
FROM process_counters
GROUP BY upid;

DROP VIEW IF EXISTS android_thread_time_in_state_output;
CREATE VIEW android_thread_time_in_state_output AS
SELECT AndroidThreadTimeInStateMetric(
  'processes', (
    SELECT
      RepeatedField(AndroidThreadTimeInStateMetric_Process(
        'metadata', metadata,
        'metrics_by_core_type',
            android_thread_time_in_state_process_metrics.metrics,
        'threads', android_thread_time_in_state_threads.threads
      ))
    FROM process
    JOIN process_metadata USING (upid)
    JOIN android_thread_time_in_state_process_metrics USING (upid)
    JOIN android_thread_time_in_state_threads USING (upid)
  )
);

-- Ensure we always get the previous clock tick for duration in
-- android_thread_time_in_state_event_raw.
DROP VIEW IF EXISTS android_thread_time_in_state_event_clock;
CREATE VIEW android_thread_time_in_state_event_clock AS
SELECT
  ts,
  LAG(ts) OVER (ORDER BY ts) AS lag_ts
FROM (
  SELECT DISTINCT ts from android_thread_time_in_state_base
);

DROP VIEW IF EXISTS android_thread_time_in_state_event_raw;
CREATE VIEW android_thread_time_in_state_event_raw AS
SELECT
  ts,
  ts - lag_ts AS dur,
  upid,
  core_type,
  utid,
  -- We need globally unique track names so add the utid even when we
  -- know the name. But when we don't, also use the tid because that's what
  -- the rest of the UI does.
  IFNULL(thread.name, 'Thread ' || thread.tid) || ' (' || thread.utid || ')'
      AS thread_track_name,
  freq,
  runtime_ms_counter - LAG(runtime_ms_counter)
      OVER (PARTITION BY core_type, utid, freq ORDER BY ts) AS runtime_ms
FROM android_thread_time_in_state_base
    -- Join to keep only utids which have non-zero runtime in the trace.
    JOIN android_thread_time_in_state_counters USING (utid, core_type)
    JOIN android_thread_time_in_state_event_clock USING(ts)
    JOIN thread using (utid);

DROP VIEW IF EXISTS android_thread_time_in_state_event_thread;
CREATE VIEW android_thread_time_in_state_event_thread AS
SELECT
  'counter' AS track_type,
  thread_track_name || ' (' || core_type || ' core)' as track_name,
  ts,
  dur,
  upid,
  sum(runtime_ms * freq) as ms_freq
FROM android_thread_time_in_state_event_raw
WHERE runtime_ms IS NOT NULL
  AND dur != 0
GROUP BY track_type, track_name, ts, dur, upid;

DROP VIEW IF EXISTS android_thread_time_in_state_event_global;
CREATE VIEW android_thread_time_in_state_event_global AS
SELECT
  'counter' AS track_type,
  'Total ' || core_type || ' core cycles / sec' as track_name,
  ts,
  dur,
  0 AS upid,
  SUM(runtime_ms * freq) AS ms_freq
FROM android_thread_time_in_state_event_raw
WHERE runtime_ms IS NOT NULL
GROUP BY ts, track_name;

DROP TABLE IF EXISTS android_thread_time_in_state_event;
CREATE TABLE android_thread_time_in_state_event AS
SELECT track_type, track_name, ts, dur, upid, ms_freq * 1000000 / dur AS value
FROM android_thread_time_in_state_event_thread
UNION ALL
SELECT track_type, track_name, ts, dur, upid, ms_freq * 1000000 / dur AS value
FROM android_thread_time_in_state_event_global
-- Biggest values at top of list in UI.
ORDER BY value DESC;
