# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re

def executable_condition(entry):
  return (entry.executable == 'x' and re.match(
      '\S+(\.(so|dll|dylib|bundle)|chrome)((\.\d+)+\w*(\.\d+){0,3})?',
      entry.name))
