# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import sys

_SCRIPT_PATH = os.path.dirname(sys.argv[0])
if _SCRIPT_PATH == "":
  _SCRIPT_PATH = os.getcwd()

_EXE_PATHS_TO_TRY = [
  '.',
  '..\\..\\build\\Debug',
  '..\\..\\build\\Release',
  '..\\Debug',
  '..\\Release',
  '../../xcodebuild/Debug',
  '../../xcodebuild/Release',
  '../../out/Debug',
  '../../out/Release']


def locate_executable(exe_name):
  for path in _EXE_PATHS_TO_TRY:
    exe_path = os.path.join(_SCRIPT_PATH, path, exe_name)
    if os.path.exists(exe_path):
      return exe_path
    exe_path = exe_path + ".exe"
    if os.path.exists(exe_path):
      return exe_path

  raise Exception("Could not locate executable '%s'" % exe_name)


def generateRSAKeyPair():
  """Returns (priv, pub) keypair where priv is a new private key and
  pub is the corresponding public key.  Both keys are BASE64 encoded."""
  keygen_path = locate_executable("remoting_host_keygen")
  pipe = os.popen(keygen_path)
  out = pipe.readlines()
  if len(out) != 2:
    raise Exception("remoting_host_keygen failed.")
  return (out[0].strip(), out[1].strip())
