#!/usr/bin/env python

# Copyright (c) 2012 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that actions get re-run when their arguments change.

This is a regression test for http://code.google.com/p/gyp/issues/detail?id=262
"""

import os

import TestGyp


def ReadFile(filename):
  fh = open(filename, "r")
  try:
    return fh.read()
  finally:
    fh.close()


def WriteFile(filename, data):
  fh = open(filename, "w")
  try:
    fh.write(data)
  finally:
    fh.close()


test = TestGyp.TestGyp()

test.run_gyp('arg-action.gyp', chdir='src')
test.build('arg-action.gyp', chdir='src')
test.built_file_must_match('dest-file.txt', 'ARG_VALUE_1', chdir='src')

gypi_file = os.path.join(test.workdir, 'src', 'arg-action.gypi')
data = ReadFile(gypi_file)
WriteFile(gypi_file, data.replace('ARG_VALUE_1', 'ARG_VALUE_2'))

test.run_gyp('arg-action.gyp', chdir='src')
test.build('arg-action.gyp', chdir='src')
test.built_file_must_match('dest-file.txt', 'ARG_VALUE_2', chdir='src')

test.pass_test()
