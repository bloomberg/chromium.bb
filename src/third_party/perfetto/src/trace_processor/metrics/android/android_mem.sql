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

-- Create all the views used to generate the Android Memory metrics proto.
SELECT RUN_METRIC('android/android_mem_lmk.sql');

-- Generate the process counter metrics.
SELECT RUN_METRIC('android/android_mem_proc_counters.sql',
                  'table_name',
                  'file_rss',
                  'counter_names',
                  'mem.rss.file');
SELECT RUN_METRIC('android/android_mem_proc_counters.sql',
                  'table_name',
                  'anon_rss',
                  'counter_names',
                  'mem.rss.anon');

CREATE VIEW process_metrics_view AS
SELECT
  AndroidMemoryMetric_ProcessMetrics(
    'process_name',
    anon_rss.name,
    'overall_counters',
    AndroidMemoryMetric_ProcessMemoryCounters(
      'anon_rss',
      AndroidMemoryMetric_Counter(
        'min',
        anon_rss.min,
        'max',
        anon_rss.max,
        'avg',
        anon_rss.avg
      )
    )
  ) as metric
FROM
  anon_rss;

CREATE VIEW android_mem_output AS
SELECT
  AndroidMemoryMetric(
    'system_metrics',
    AndroidMemoryMetric_SystemMetrics(
      'lmks',
      AndroidMemoryMetric_LowMemoryKills(
        'total_count',
        (SELECT COUNT(*) FROM lmk_by_score)
      )
    ),
    'process_metrics',
    (SELECT RepeatedField(metric) FROM process_metrics_view)
  );
