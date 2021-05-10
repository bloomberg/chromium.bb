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
DROP VIEW IF EXISTS same_frame;
CREATE VIEW same_frame AS
SELECT COUNT(name) AS total_duplicate_frames
FROM counters
WHERE name='SAME_FRAME'
AND value=1;

DROP VIEW IF EXISTS duplicate_frames_logged;
CREATE VIEW duplicate_frames_logged AS
SELECT CASE WHEN COUNT(name) > 0 THEN 1 ELSE 0 END AS logs_found
FROM counters
WHERE name='SAME_FRAME' AND value=0;

DROP VIEW IF EXISTS dpu_underrun;
CREATE VIEW dpu_underrun AS
SELECT COUNT(name) AS total_dpu_underrun_count
FROM counters
WHERE name='DPU_UNDERRUN'
AND value=1;

DROP VIEW IF EXISTS display_metrics_output;
CREATE VIEW display_metrics_output AS
SELECT AndroidDisplayMetrics(
    'total_duplicate_frames', (SELECT total_duplicate_frames
                            FROM same_frame),
    'duplicate_frames_logged', (SELECT logs_found
                            FROM duplicate_frames_logged),
    'total_dpu_underrun_count', (SELECT total_dpu_underrun_count
                            FROM dpu_underrun)
);
