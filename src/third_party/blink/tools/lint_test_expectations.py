#!/usr/bin/env vpython
# Copyright 2018 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys

from blinkpy.common import version_check  # pylint: disable=unused-import
from blinkpy.web_tests.lint_test_expectations import main


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:], sys.stderr))
