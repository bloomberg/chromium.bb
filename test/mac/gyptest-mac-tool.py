#!/usr/bin/env python

# Copyright 2016 Google Inc. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Verifies that mac_tool.py commands works correctly.
"""

import TestGyp

import contextlib
import errno
import os
import subprocess
import sys


@contextlib.contextmanager
def CreateAndMountImage(image, mountpoint, size):
  if not os.path.isabs(mountpoint):
    mountpoint = os.path.join(os.getcwd(), mountpoint)
  TestGyp.MakeDirs(mountpoint)
  subprocess.check_call([
      'hdiutil', 'create', image, '-volname', image,
      '-size', str(size), '-fs', 'HFS+', '-type', 'UDIF'],
      stdout=subprocess.PIPE)
  subprocess.check_call([
      'hdiutil', 'attach', image + '.dmg', '-mountpoint', mountpoint],
      stdout=subprocess.PIPE)
  try:
    yield object()
  finally:
    subprocess.check_call(
        ['hdiutil', 'detach', mountpoint],
        stdout=subprocess.PIPE)


def CopyBundleResource(test, mac_tool_path, source, dest):
  process = subprocess.Popen([
      mac_tool_path, 'copy-bundle-resource', source, dest, 'True'],
      stdout=subprocess.PIPE, stderr=subprocess.PIPE)
  process.communicate()
  if not process.returncode:
    test.fail_test()


def FindMacTool():
  for path in os.environ['PYTHONPATH'].split(os.pathsep):
    mac_tool_path = os.path.join(path, '../../pylib/gyp/mac_tool.py')
    if os.path.isfile(mac_tool_path):
      return mac_tool_path
  raise Exception("mac_tool.py: file not found")


def TestCopyBundleResource(test, mac_tool_path):
  CopyBundleResource(test, mac_tool_path, __file__, os.path.basename(__file__))
  CopyBundleResource(test, mac_tool_path, __file__, os.path.basename(__file__))

  with CreateAndMountImage('disk', 'disk', '10m'):
    dest = os.path.join('disk', os.path.basename(__file__))
    CopyBundleResource(test, mac_tool_path, __file__, dest)
    CopyBundleResource(test, mac_tool_path, __file__, dest)


if sys.platform == 'darwin':
  test = TestGyp.TestGyp(formats=['ninja'])
  TestCopyBundleResource(test, FindMacTool())

  test.pass_test()
