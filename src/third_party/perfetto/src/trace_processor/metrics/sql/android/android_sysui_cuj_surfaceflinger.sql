-- Isolate the SurfaceFlinger process Id
DROP TABLE IF EXISTS android_sysui_cuj_sf_process;
CREATE TABLE android_sysui_cuj_sf_process AS
SELECT name, upid FROM process
WHERE process.name='/system/bin/surfaceflinger'
LIMIT 1;

DROP VIEW IF EXISTS android_sysui_cuj_sf_actual_frame_timeline_slice;
CREATE VIEW android_sysui_cuj_sf_actual_frame_timeline_slice AS
SELECT
  actual.*,
  actual.ts + actual.dur AS ts_end,
  CAST(actual.name AS integer) AS vsync
FROM actual_frame_timeline_slice actual JOIN android_sysui_cuj_sf_process USING (upid);

DROP TABLE IF EXISTS android_sysui_cuj_surfaceflinger_main_thread;
CREATE TABLE android_sysui_cuj_surfaceflinger_main_thread AS
  SELECT android_sysui_cuj_sf_process.name AS process_name, thread.utid
  FROM thread JOIN android_sysui_cuj_sf_process USING (upid)
  WHERE thread.is_main_thread;

DROP TABLE IF EXISTS android_sysui_cuj_sf_main_thread_track;
CREATE TABLE android_sysui_cuj_sf_main_thread_track AS
SELECT thread_track.id
FROM thread_track
JOIN android_sysui_cuj_surfaceflinger_main_thread thread USING (utid);

DROP VIEW IF EXISTS android_sysui_cuj_surfaceflinger_gpu_completion_thread;
CREATE VIEW android_sysui_cuj_surfaceflinger_gpu_completion_thread AS
  SELECT android_sysui_cuj_sf_process.name AS process_name, thread.utid
  FROM thread JOIN android_sysui_cuj_sf_process USING (upid)
  WHERE thread.name = 'GPU completion';

DROP VIEW IF EXISTS android_sysui_cuj_surfaceflinger_renderengine_thread;
CREATE VIEW android_sysui_cuj_surfaceflinger_renderengine_thread AS
  SELECT android_sysui_cuj_sf_process.name AS process_name, thread.utid
  FROM thread JOIN android_sysui_cuj_sf_process USING (upid)
  WHERE thread.name = 'RenderEngine';

DROP VIEW IF EXISTS android_sysui_cuj_surfaceflinger_gpu_completion_slices;
CREATE VIEW android_sysui_cuj_surfaceflinger_gpu_completion_slices AS
  SELECT
    process_name,
    thread.utid,
    slice.*,
    slice.ts + slice.dur AS ts_end,
    -- Extracts 1234 from 'waiting for GPU completion 1234'
    CAST(STR_SPLIT(slice.name, ' ', 4) AS INTEGER) AS idx
  FROM slice
  JOIN thread_track ON slice.track_id = thread_track.id
  JOIN android_sysui_cuj_surfaceflinger_gpu_completion_thread thread USING (utid)
  WHERE slice.name GLOB 'waiting for GPU completion *'
  AND dur > 0;

-- Find flows between actual frame slices from app process to surfaceflinger, allowing us to
-- correlate vsyncs.
DROP VIEW IF EXISTS android_sysui_cuj_surfaceflinger_app_flow_vsyncs;
CREATE VIEW android_sysui_cuj_surfaceflinger_app_flow_vsyncs AS
SELECT
  app_slice.name AS app_vsync,
  app_slice.id AS app_slice_id,
  cuj_process.name AS app_process,
  sf_slice.name AS sf_vsync,
  sf_slice.id AS sf_slice_id
FROM android_sysui_cuj_sf_actual_frame_timeline_slice sf_slice
JOIN directly_connected_flow(sf_slice.id) flow
JOIN actual_frame_timeline_slice app_slice ON slice_in = app_slice.id
JOIN android_sysui_cuj_last_cuj cuj_process ON app_slice.upid = cuj_process.upid
GROUP BY app_vsync, sf_vsync;

-- Filter to those SF frames which flow from app frames that are within the app vsync boundaries of
-- the CUJ
DROP TABLE IF EXISTS android_sysui_cuj_sf_frames_in_cuj;
CREATE TABLE android_sysui_cuj_sf_frames_in_cuj AS
SELECT
  sf_frame.ts,
  sf_frame.dur,
  sf_frame.jank_type,
  sf_frame.ts + sf_frame.dur AS ts_end,
  flows.sf_vsync,
  flows.app_vsync
-- This table contains only the frame timeline slices within the CUJ app vsync boundaries
FROM android_sysui_cuj_frame_timeline_events app_frames
-- Find the matching SF frame via flow
JOIN android_sysui_cuj_surfaceflinger_app_flow_vsyncs flows ON app_frames.vsync = flows.app_vsync
JOIN android_sysui_cuj_sf_actual_frame_timeline_slice sf_frame ON sf_frame.id = flows.sf_slice_id
GROUP BY flows.sf_vsync;

-- Take the min and max vsync to define the SurfaceFlinger boundaries
DROP TABLE IF EXISTS android_sysui_cuj_sf_vsync_boundaries;
CREATE TABLE android_sysui_cuj_sf_vsync_boundaries AS
SELECT MIN(sf_vsync) AS vsync_min, MAX(sf_vsync) AS vsync_max
FROM android_sysui_cuj_sf_frames_in_cuj;

-- Find just the commit slices, within the CUJ (by vsync)
DROP TABLE IF EXISTS android_sysui_cuj_surfaceflinger_commit_slices_in_cuj;
CREATE TABLE android_sysui_cuj_surfaceflinger_commit_slices_in_cuj AS
SELECT * FROM
  (SELECT
    -- Extract the vsync number from name like 'commit 235991 vsyncIn 15.992ms'
    CAST(STR_SPLIT(slice.name, ' ', 1) AS INTEGER) AS vsync,
    CAST(CAST(STR_SPLIT(slice.name, ' ', 3) AS NUMBER) * 1e6 + slice.ts AS INTEGER) AS expected_vsync_ts,
    slice.name,
    slice.ts,
    slice.dur,
    slice.ts + slice.dur AS ts_end
  FROM slice
  JOIN android_sysui_cuj_sf_main_thread_track main_track ON slice.track_id = main_track.id
  WHERE slice.dur > 0 AND slice.name GLOB 'commit *')
JOIN android_sysui_cuj_sf_vsync_boundaries cuj_boundaries
WHERE vsync >= cuj_boundaries.vsync_min AND vsync <= cuj_boundaries.vsync_max;

-- Find just the onMessageInvalidate slices, within the CUJ (by vsync)
DROP VIEW IF EXISTS android_sysui_cuj_surfaceflinger_on_message_invalidate_slices_in_cuj;
CREATE VIEW android_sysui_cuj_surfaceflinger_on_message_invalidate_slices_in_cuj AS
  WITH on_msg AS (SELECT
    -- Extract the vsync number from name like 'onMessageInvalidate 235991 vsyncIn 15.992ms'
    CAST(STR_SPLIT(slice.name, ' ', 1) AS INTEGER) AS vsync,
    CAST(CAST(STR_SPLIT(slice.name, ' ', 3) AS NUMBER) * 1e6 + slice.ts AS INTEGER) AS expected_vsync_ts,
    slice.ts,
    slice.ts + slice.dur AS ts_end,
    slice.dur
  FROM slice
  JOIN android_sysui_cuj_sf_main_thread_track main_track ON slice.track_id = main_track.id
  WHERE slice.name GLOB 'onMessageInvalidate *'
  )
SELECT
    on_msg.vsync,
    on_msg.ts,
    on_msg.ts_end,
    on_msg.dur,
    on_msg.expected_vsync_ts,
    lag(on_msg.ts_end) OVER (ORDER BY on_msg.ts_end ASC) AS ts_prev_frame_end,
    lead(on_msg.ts) OVER (ORDER BY on_msg.ts ASC) AS ts_next_frame_start
FROM on_msg
JOIN android_sysui_cuj_sf_vsync_boundaries cuj_boundaries
WHERE on_msg.vsync >= cuj_boundaries.vsync_min AND on_msg.vsync <= cuj_boundaries.vsync_max;

-- Find just the composite slices
DROP TABLE IF EXISTS android_sysui_cuj_surfaceflinger_composite_slices;
CREATE TABLE android_sysui_cuj_surfaceflinger_composite_slices AS
  SELECT
    slice.name,
    slice.ts,
    slice.dur,
    slice.ts + slice.dur AS ts_end
  FROM slice
  JOIN android_sysui_cuj_sf_main_thread_track main_track ON slice.track_id = main_track.id
  WHERE slice.dur > 0 AND slice.name = 'composite';

DROP VIEW IF EXISTS android_sysui_cuj_surfaceflinger_commit_composite_frames_in_cuj;
CREATE VIEW android_sysui_cuj_surfaceflinger_commit_composite_frames_in_cuj AS
  WITH commit_to_composite AS (
    SELECT
      commits.vsync,
      commits.ts AS commit_ts,
      commits.name AS name,
      commits.expected_vsync_ts,
      min(composite.ts) AS composite_ts
      FROM android_sysui_cuj_surfaceflinger_commit_slices_in_cuj commits
      JOIN android_sysui_cuj_surfaceflinger_composite_slices composite ON composite.ts > commits.ts_end
      GROUP BY commits.vsync
  )
  SELECT
    vsync,
    match.commit_ts AS ts,
    composite.ts_end AS ts_end,
    composite.ts_end - match.commit_ts AS dur,
    match.expected_vsync_ts,
    lag(composite.ts_end) OVER (ORDER BY composite.ts_end ASC) AS ts_prev_frame_end,
    lead(match.commit_ts) OVER (ORDER BY match.commit_ts ASC) AS ts_next_frame_start
  FROM commit_to_composite match
  JOIN android_sysui_cuj_surfaceflinger_composite_slices composite ON match.composite_ts = composite.ts;

-- All SF frames in the CUJ
DROP TABLE IF EXISTS android_sysui_cuj_surfaceflinger_main_thread_frames;
CREATE TABLE android_sysui_cuj_surfaceflinger_main_thread_frames AS
SELECT * FROM android_sysui_cuj_surfaceflinger_on_message_invalidate_slices_in_cuj
UNION ALL
SELECT * FROM android_sysui_cuj_surfaceflinger_commit_composite_frames_in_cuj;

-- Our timestamp boundaries are the earliest ts and latest ts_end of the main thread frames (e.g.
-- onMessageInvalidate or commit/composite pair) which flow from app frames within the CUJ
DROP TABLE IF EXISTS android_sysui_cuj_sf_ts_boundaries;
CREATE TABLE android_sysui_cuj_sf_ts_boundaries AS
SELECT ts, ts_end - ts AS dur, ts_end
FROM (SELECT MIN(ts) AS ts, MAX(ts_end) AS ts_end FROM android_sysui_cuj_surfaceflinger_main_thread_frames);

DROP TABLE IF EXISTS android_sysui_cuj_surfaceflinger_main_thread_slices_in_cuj;
CREATE TABLE android_sysui_cuj_surfaceflinger_main_thread_slices_in_cuj AS
  SELECT
    slice.*,
    slice.ts + slice.dur AS ts_end
  FROM slice
  JOIN android_sysui_cuj_sf_main_thread_track main_track ON slice.track_id = main_track.id
  JOIN android_sysui_cuj_sf_ts_boundaries cuj_boundaries
  ON slice.ts >= cuj_boundaries.ts AND slice.ts <= cuj_boundaries.ts_end
  WHERE slice.dur > 0;

-- Find SurfaceFlinger GPU completions that are within the CUJ
DROP TABLE IF EXISTS android_sysui_cuj_surfaceflinger_gpu_completion_slices_in_cuj;
CREATE TABLE android_sysui_cuj_surfaceflinger_gpu_completion_slices_in_cuj AS
SELECT slice.* FROM android_sysui_cuj_surfaceflinger_gpu_completion_slices slice
JOIN android_sysui_cuj_sf_ts_boundaries cuj_boundaries
ON slice.ts >= cuj_boundaries.ts AND slice.ts <= cuj_boundaries.ts_end;

DROP TABLE IF EXISTS android_sysui_cuj_surfaceflinger_renderengine_slices_in_cuj;
CREATE TABLE android_sysui_cuj_surfaceflinger_renderengine_slices_in_cuj AS
  SELECT
    process_name,
    thread.utid,
    slice.*,
    slice.ts + slice.dur AS ts_end
  FROM slice
  JOIN thread_track ON slice.track_id = thread_track.id
  JOIN android_sysui_cuj_surfaceflinger_renderengine_thread thread USING (utid)
  JOIN android_sysui_cuj_sf_ts_boundaries cuj_boundaries
  ON slice.ts >= cuj_boundaries.ts AND slice.ts <= cuj_boundaries.ts_end
  WHERE slice.dur > 0;

DROP VIEW IF EXISTS android_sysui_cuj_gcs_to_mt_match;
CREATE VIEW android_sysui_cuj_gcs_to_mt_match AS
-- Match Mainthread slice with the first GPU Completion that begins during it
SELECT
gcs.ts AS gcs_ts,
gcs.ts_end AS gcs_ts_end,
gcs.dur AS gcs_dur,
gcs.idx AS idx,
mtf.ts AS mts_ts,
mtf.vsync AS vsync
FROM android_sysui_cuj_surfaceflinger_gpu_completion_slices_in_cuj gcs
-- join with all previous render frames but take latest start time
JOIN android_sysui_cuj_surfaceflinger_main_thread_frames mtf ON gcs.ts > mtf.ts AND gcs.ts < mtf.ts + mtf.dur
GROUP BY gcs.ts, gcs.ts_end, gcs.dur, gcs.idx;

-- Those SurfaceFlinger Frames where we missed the deadline
-- To avoid overlap - which could result in counting janky slices more than once - we limit the
-- definition of each frame to:
--  * beginning when the shared timeline actual frame starts, or - if later -
--    when the previous main thread computation ended
--  * ending when the next main thread computation begins, but no later than the
--    shared timeline actual frame ends
DROP TABLE IF EXISTS android_sysui_cuj_sf_missed_frames;
CREATE TABLE android_sysui_cuj_sf_missed_frames AS
SELECT
  CAST(frame.name AS integer) AS frame_number,
  CAST(frame.name AS integer) AS vsync,
  max(mtf.ts_prev_frame_end, frame.ts) AS ts,
  min(mtf.ts_next_frame_start, frame.ts_end) AS ts_end,
  min(mtf.ts_next_frame_start, frame.ts_end) - max(mtf.ts_prev_frame_end, frame.ts) AS dur,
  -- Needed for compatibility with downstream scripts
  CAST(min(mtf.ts_next_frame_start, frame.ts_end) - max(mtf.ts_prev_frame_end, frame.ts) AS INTEGER) AS dur_frame,
  gcs.gcs_ts,
  gcs.gcs_ts_end,
  gcs.gcs_dur,
  CAST(1 AS INTEGER) AS app_missed
FROM android_sysui_cuj_sf_actual_frame_timeline_slice frame
JOIN android_sysui_cuj_surfaceflinger_main_thread_frames mtf ON frame.name = mtf.vsync
JOIN android_sysui_cuj_gcs_to_mt_match gcs ON gcs.vsync = frame.name
WHERE frame.jank_type != 'None';
