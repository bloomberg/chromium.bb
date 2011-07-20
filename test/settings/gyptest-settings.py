#!/usr/bin/env python

# Copyright (c) 2011 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Smoke-tests 'settings' blocks.
"""

import TestGyp

test = TestGyp.TestGyp()
test.run_gyp('settings.gyp')
test.build('test.gyp', test.ALL)
test.pass_test()
