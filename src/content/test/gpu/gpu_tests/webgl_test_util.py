# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os

from gpu_tests import path_util

conformance_relcomps = (
  'third_party', 'webgl', 'src', 'sdk', 'tests')

extensions_relcomps = (
    'content', 'test', 'data', 'gpu')

conformance_relpath = os.path.join(*conformance_relcomps)
conformance_path = os.path.join(path_util.GetChromiumSrcDir(),
                                conformance_relpath)
extensions_relpath = os.path.join(*extensions_relcomps)

# These URL prefixes are needed because having more than one static
# server dir is causing the base server directory to be moved up the
# directory hierarchy.
url_prefixes_to_trim = [
  '/'.join(conformance_relcomps) + '/',
  '/'.join(extensions_relcomps) + '/'
]
