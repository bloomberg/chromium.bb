#!/bin/bash
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A helper script to query internal tables for build-time performance metrics
# for a specific build id.

build_id="$1"

# Make sure we have prod access so we don't fail without getting any work done.
prodcertstatus 1>/dev/null 2>/dev/null || prodaccess

# Run the query.
(f1-sql -quiet=1 -csv_output=1 \
  | tr -d '"') << SQLtoHERE
SELECT
  board,
  pkg_phase,
  seconds
FROM (
  SELECT
    install_packages_request.sysroot.build_target.name AS board,
    ARRAY_TO_STRING(
      ARRAY(SELECT x FROM UNNEST(SPLIT(event.name, '.')) AS x WHERE x IS NOT NULL),
      '.') pkg_phase,
    CAST(ROUND(event.duration_milliseconds / 1000.0) AS INT64) AS seconds
  FROM
    chromeos_ci_eng.analysis_event_log.\`all\`,
    UNNEST(install_packages_response.events) AS event
  JOIN
    chromeos_ci_eng.buildbucket_builds AS bb
  ON
    build_id = bb.id
  WHERE
    array_length(install_packages_response.events) > 0 AND
    build_id=${build_id}
  ORDER BY seconds DESC
)
WHERE
  pkg_phase NOT LIKE '%.sysroot.%';
SQLtoHERE

