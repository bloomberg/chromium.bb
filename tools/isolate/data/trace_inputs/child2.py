# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import time

print 'child2'
# Introduce a race condition with the parent so the parent may have a chance
# to exit before the child. Will be random.
time.sleep(.01)
# Do not do anything with it.
open('test_file.txt')
