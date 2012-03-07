# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import os.path
import sys

# We need to get json_minify from the third_party directory.
# This is similar to what is done in chrome/common/extensions/docs/build.py
third_party_path = os.path.join(os.path.dirname(os.path.realpath(__file__)),
                                os.pardir, os.pardir, 'third_party/')
if third_party_path not in sys.path:
  sys.path.insert(0, third_party_path)
import json_minify as minify

def Load(filename):
  with open(filename, 'r') as handle:
    return json.loads(minify.json_minify(handle.read()))
