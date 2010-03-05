#!/usr/bin/env python
# Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper around webkitpy/layout_tests/run-chromium-webkit-tests.py"""
import os
import sys

sys.path.append(os.path.join(os.path.dirname(os.path.abspath(sys.argv[0])),
                             "webkitpy", "layout_tests"))
import run_chromium_webkit_tests

if __name__ == '__main__':
    options, args = run_chromium_webkit_tests.parse_args()
    run_chromium_webkit_tests.main(options, args)
