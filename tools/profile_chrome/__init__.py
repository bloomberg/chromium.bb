# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys


_SRC_DIR = os.path.abspath(os.path.join(
    os.path.dirname(__file__), '..', '..'))
sys.path.append(os.path.join(_SRC_DIR, 'third_party', 'catapult', 'devil'))
sys.path.append(os.path.join(_SRC_DIR, 'build', 'android'))
