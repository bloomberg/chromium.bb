# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from webkitpy.common.memoized import memoized
from webkitpy.common.path_finder import PathFinder


@memoized
def absolute_chromium_wpt_dir(host):
    finder = PathFinder(host.filesystem)
    return finder.path_from_layout_tests('external', 'wpt')


@memoized
def absolute_chromium_dir(host):
    finder = PathFinder(host.filesystem)
    return finder.chromium_base()
