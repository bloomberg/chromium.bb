# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


def CheckOptimized(input_api, output_api):
  file_filter = lambda f: f.LocalPath().endswith('.svg')
  svgs = input_api.AffectedFiles(file_filter=file_filter, include_deletes=False)

  if not svgs:
    return []

  import svgo

  unoptimized = []
  for f in svgs:
    output = svgo.Run(input_api.os_path, ['-o', '-', f.AbsoluteLocalPath()])
    if output.strip() != '\n'.join(f.NewContents()).strip():
      unoptimized.append(f.LocalPath())

  if unoptimized:
    instructions = 'Run tools/resources/svgo.py on these files to optimize:'
    msg = '\n  '.join([instructions] + unoptimized)
    return [output_api.PresubmitNotifyResult(msg)]

  return []
