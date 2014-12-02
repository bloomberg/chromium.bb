# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import subprocess
import sys

(source, name, out,) = sys.argv[1:]

subprocess.check_call([
  "flex",
  "-Cfe",
  "-o", out + ".cpp",
  "--prefix=" + name,
  source,
])
