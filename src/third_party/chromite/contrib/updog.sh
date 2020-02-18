#!/bin/bash
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# A helper to query internal tables for a specific package name.

package_name="$1"

dremel \
  --min_completion_ratio 1 \
  --output csv \
  << SQLtoHERE
SELECT
  package_name,
  ARRAY_CONCAT(board_name) AS boards
FROM
  (SELECT
    ARRAY_TO_STRING(
      [ns[SAFE_OFFSET(4)],
       ns[SAFE_OFFSET(5)],
       ns[SAFE_OFFSET(6)],
       ns[SAFE_OFFSET(7)],
       ns[SAFE_OFFSET(8)],
       ns[SAFE_OFFSET(9)]],
      '.') AS package_name,
        ARRAY_AGG(DISTINCT board_name) AS board_name
  FROM
  (SELECT
    create_image_request.build_target.name AS board_name,
    SPLIT(event.name, '.') AS ns
  FROM
    chromeos_ci_eng.analysis_event_log.last3days,
    UNNEST(create_image_result.events) AS event
  WHERE
    create_image_request.build_target.name IS NOT NULL)
  GROUP BY 1
  ORDER BY 1)
WHERE
  package_name LIKE '%${package_name}%';
SQLtoHERE

