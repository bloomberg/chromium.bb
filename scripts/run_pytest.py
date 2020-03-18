# -*- coding: utf-8 -*-
# Copyright 2020 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Wrapper to execute pytest inside the chromite virtualenv."""

from __future__ import print_function

import sys

import pytest  # pylint: disable=import-error


def main(argv):
  sys.exit(pytest.main(argv))
