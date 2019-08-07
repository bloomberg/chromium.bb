#!/usr/bin/env python
# Copyright 2019 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

THIS_DIR = os.path.abspath(os.path.dirname(__file__))
SRC_DIR = os.path.abspath(os.path.join(THIS_DIR, *((os.pardir,) * 4)))
TYP_DIR = os.path.join(SRC_DIR, 'third_party', 'catapult', 'third_party', 'typ')

if not TYP_DIR in sys.path:
    sys.path.insert(0, TYP_DIR)

import typ


# TODO(rsesek): This is a hack to run the unittests with
# use_new_mac_bundle_structure set to both True and False. Go back to just
# using typ.main() after https://crbug.com/958976 is fixed.
class Runner(typ.Runner):

    def __init__(self, *args, **kwargs):
        self._run_count = 0
        super(Runner, self).__init__(*args, **kwargs)

    def _check_win_multiprocessing(self):
        # Do not use multiprocessing.
        return False

    def _run_tests(self, result_set, test_set, all_tests):
        if self._run_count == 0:
            os.environ['USE_NEW_BUNDLE_STRUCTURE'] = '0'
        elif self._run_count == 1:
            os.environ['USE_NEW_BUNDLE_STRUCTURE'] = '1'
        self._run_count += 1

        return super(Runner, self)._run_tests(result_set, test_set, all_tests)


sys.exit(
    Runner(host=typ.Host()).main(
        top_level_dir=os.path.dirname(THIS_DIR),
        suffixes=['*_test.py'],
        repeat=2))
