#!/usr/bin/env python
# Copyright %d The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Usage:
#
#   cd third_party/closure_compiler
#   tools/create_externs_gyp.py > externs/compiled_resources.gyp

from datetime import date
import os


_EXTERNS_TEMPLATE = """
# Copyright %d The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

########################################################
#    NOTE: THIS FILE IS GENERATED. DO NOT EDIT IT!     #
# Instead, run create_externs_gyp.py to regenerate it. #
########################################################
{
  'targets': [
    %s,
  ],
}""".lstrip()


_TARGET_TEMPLATE = """
    {
      'target_name': '%s',
      'includes': ['../externs_js.gypi'],
    }"""


def CreateExternsGyp():
  externs_dir = os.path.join(os.path.dirname(__file__), "..", "externs")
  externs_files = [f for f in os.listdir(externs_dir) if f.endswith('.js')]
  externs_files.sort()
  targets = [_TARGET_TEMPLATE % f[:-3] for f in externs_files]
  return _EXTERNS_TEMPLATE % (date.today().year, ",".join(targets).strip())


if __name__ == "__main__":
  print CreateExternsGyp()
