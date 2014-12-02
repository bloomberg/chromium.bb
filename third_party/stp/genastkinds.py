# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import os.path
import subprocess
import sys

(script, source, out,) = sys.argv[1:]

subprocess.check_call([
  "perl", script,
  "--file", source,
])
os.rename("ASTKind.cpp", os.path.join(out, "src/lib/AST/ASTKind.cpp"))
os.rename("ASTKind.h", os.path.join(out, "src/include/stp/AST/ASTKind.h"))
