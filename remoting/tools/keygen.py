# Copyright (c) 2010 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import platform
import sys

SCRIPT_PATH = os.path.dirname(sys.argv[0])
if SCRIPT_PATH == "":
  SCRIPT_PATH = os.getcwd()

if platform.system() == "Windows":
  KEYGEN_PATH = SCRIPT_PATH + '\..\Debug\chromoting_host_keygen.exe'
elif platform.system() == "Darwin": # Darwin == MacOSX
  KEYGEN_PATH = SCRIPT_PATH + '/../../xcodebuild/Debug/chromoting_host_keygen'
else:
  KEYGEN_PATH = SCRIPT_PATH + '/../../out/Debug/chromoting_host_keygen'

def generateRSAKeyPair():
  """Returns (priv, pub) keypair where priv is a new private key and
  pub is the corresponding public key.  Both keys are BASE64 encoded."""
  pipe = os.popen(KEYGEN_PATH)
  out = pipe.readlines()
  if len(out) != 2:
    raise Exception("chromoting_host_keygen failed.")
  return (out[0].strip(), out[1].strip())
