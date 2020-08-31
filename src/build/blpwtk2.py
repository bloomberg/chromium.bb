#!/usr/bin/env python

# Copyright (C) 2017 Bloomberg L.P. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# This script is based on deport_tools/gn.py, and contains code copied from it.
# That file carries the following license:
# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is a wrapper around gn for generating blpwtk2 build tree
#
# Usage:
# blpwtk2.py                                -- Generate out/shared_debug
#                                                       out/shared_release
#                                                       out/static_release
#                                                       out/static_release_md
#
# blpwtk2.py shared                         -- Generate out/shared_debug
#                                                       out/shared_release
#
# blpwtk2.py static                         -- Generate out/static_release
#                                                       out/static_release_md
#
# blpwtk2.py debug                          -- Generate out/shared_debug
#
# blpwtk2.py release                        -- Generate out/shared_release
#                                                       out/static_release
#                                                       out/static_release_md
#
# blpwtk2.py static_crt                     -- Generate out/shared_debug
#                                                       out/shared_release
#                                                       out/static_release
#
# blpwtk2.py dynamic_crt                    -- Generate out/shared_debug
#                                                       out/shared_release
#                                                       out/static_release_md
#
# blpwtk2.py shared release                 -- Generate out/shared_release
#
# blpwtk2.py static release                 -- Generate out/static_release
#                                                       out/static_release_md
#
# blpwtk2.py static release static_crt      -- Generate out/static_release
#
# blpwtk2.py static release dynamic_crt     -- Generate out/static_release_md
#

import os
import sys
import subprocess

# Update the version of content shell here
content_version = '84.0.4147.140'

if sys.platform == 'win32':
  try:
    sys.path.insert(0, os.path.join(chrome_src, 'third_party', 'psyco_win32'))
    import psyco
  except:
    psyco = None
else:
  psyco = None

def verifyGN():
  try:
    subprocess.check_output(["which", "gn"], stderr=subprocess.STDOUT)
    return 0
  except subprocess.CalledProcessError:
    return 1

def applyVariableToEnvironment(env, var, val):
  if env in os.environ:
    envItems = os.environ[env].split(" ")
  else:
    envItems = []
  for i in range(0, len(envItems)):
    iSplit = envItems[i].split("=")
    if iSplit[0] == var:
      # Found it, remove current, replace with new value
      del envItems[i]
      break
  envItems.append(var + "=" + val)
  os.environ[env] = " ".join(envItems)


def createBuildCmd(gn_cmds, gn_mode, gn_type, bb_version, crt_mode, arch_type):
  if gn_type == 'debug':
    applyVariableToEnvironment('GN_DEFINES', 'is_debug', 'true')
    applyVariableToEnvironment('GN_DEFINES', 'is_official_build', 'false')

    if gn_mode == 'shared':
      applyVariableToEnvironment('GN_DEFINES', 'is_component_build', 'true')
    else:
      applyVariableToEnvironment('GN_DEFINES', 'is_component_build', 'false')

  else:
    applyVariableToEnvironment('GN_DEFINES', 'is_debug', 'false')

    if gn_mode == 'shared':
      applyVariableToEnvironment('GN_DEFINES', 'is_component_build', 'true')
      applyVariableToEnvironment('GN_DEFINES', 'is_official_build', 'false')
    else:
      applyVariableToEnvironment('GN_DEFINES', 'is_component_build', 'false')
      applyVariableToEnvironment('GN_DEFINES', 'is_official_build', 'true')

  if crt_mode == 'static_crt':
    applyVariableToEnvironment('GN_DEFINES', 'use_dynamic_crt', 'false')
  else:
    applyVariableToEnvironment('GN_DEFINES', 'use_dynamic_crt', 'true')

  gn_cmd = ''
  version = ''
  if bb_version:
    version = ' bb_version=\\\"' + bb_version + '\\\"'

  suffix = ''
  if arch_type == 'x64':
    suffix = '64'

  if crt_mode == 'static_crt':
    gn_cmd = ' gen out/' + gn_mode + '_' + gn_type + suffix + ' --args="' \
             + os.environ['GN_DEFINES'] + version + '"'
  else:
    gn_cmd = ' gen out/' + gn_mode + '_' + gn_type + suffix + '_md --args="' \
             + os.environ['GN_DEFINES'] + version + '"'

  gn_cmds.append(gn_cmd)

def parseArgs(argv):
  gn_shared = []
  gn_static = []
  gn_mode = None
  gn_type = None
  bb_version = None
  crt_mode = None

  # Generate 32-bit binaries and libraries by default
  arch_type = 'x86'

  if argv:
    for i in xrange(0, len(argv)):
      arg = argv[i]
      if arg == 'shared' or arg == 'static':
        gn_mode = arg
      elif arg == 'debug' or arg == 'release':
        gn_type = arg
      elif arg == 'dynamic_crt' or arg == 'static_crt':
        crt_mode = arg
      elif arg == 'x86' or arg == 'x64':
        arch_type = arg
      elif arg == '--bb_version':
        with open('../devkit_version.txt', 'r') as f:
          bb_version = f.readline()

  # Select target architecture
  applyVariableToEnvironment('GN_DEFINES', 'target_cpu', '\\\"' + arch_type + '\\\"')

  # Disable NativeClient, print preview, browser extensiion, paint preview
  # and VR support
  applyVariableToEnvironment('GN_DEFINES', 'enable_nacl', 'false')
  applyVariableToEnvironment('GN_DEFINES', 'enable_print_preview', 'false')
  applyVariableToEnvironment('GN_DEFINES', 'enable_extensions', 'false')
  applyVariableToEnvironment('GN_DEFINES', 'enable_paint_preview', 'false')
  applyVariableToEnvironment('GN_DEFINES', 'enable_vr', 'false')

  # Disable safe browsing mode
  applyVariableToEnvironment('GN_DEFINES', 'safe_browsing_mode', '0')

  # Enable proprietary codecs
  applyVariableToEnvironment('GN_DEFINES', 'proprietary_codecs', 'true')
  applyVariableToEnvironment('GN_DEFINES', 'ffmpeg_branding', '\\\"Chrome\\\"')

  # Apply the content shell version
  applyVariableToEnvironment('GN_DEFINES', 'content_shell_version', '\\\"' + content_version + '\\\"')

  # Disable COM_INIT_CHECK_HOOK_ENABLED in com_init_check_hook.h
  applyVariableToEnvironment('GN_DEFINES', 'com_init_check_hook_disabled', 'true')

  # Disable use of allocator shim.  This doesn't work well when linking with
  # dynamic CRT.
  applyVariableToEnvironment('GN_DEFINES', 'use_allocator_shim', 'false')

  # Disable mDNS support.
  applyVariableToEnvironment('GN_DEFINES', 'enable_mdns', 'false')

  if gn_type == 'debug' or not gn_type:
    if gn_mode == 'shared' or not gn_mode:
      createBuildCmd(gn_shared, 'shared', 'debug', bb_version, 'static_crt', arch_type)

  if gn_type == 'release' or not gn_type:
    if gn_mode == 'shared' or not gn_mode:
      createBuildCmd(gn_shared, 'shared', 'release', bb_version, 'static_crt', arch_type)
    if gn_mode == 'static' or not gn_mode:
      if crt_mode == 'static_crt' or not crt_mode:
        createBuildCmd(gn_static, 'static', 'release', bb_version, 'static_crt', arch_type)
      if crt_mode == 'dynamic_crt' or not crt_mode:
        createBuildCmd(gn_static, 'static', 'release', bb_version, 'dynamic_crt', arch_type)

  return gn_shared, gn_static, gn_type

def generateBuildTree(cmds):
  for cmd in cmds:
    os.system("gn" + cmd)

def main(argv):
  if verifyGN():
    print "Please install depot_tools."
    return 1

  gn_shared, gn_static, gn_type = parseArgs(argv)

  if len(gn_shared)  > 0:
    if gn_type:
      print "Generating GN shared %s build tree" % gn_type
    else:
      print "Generating GN shared debug and release build trees"
  sys.stdout.flush()

  generateBuildTree(gn_shared)

  if len(gn_static)  > 0:
    if gn_type:
      print "Generating GN static %s build tree" % gn_type
    else:
      print "Generating GN static release build trees"
  sys.stdout.flush()
  generateBuildTree(gn_static)

  return 0

if __name__ == '__main__':
  # Use the Psyco JIT if available.
  if psyco:
    psyco.profile()
    print "Enabled Psyco JIT."

  sys.exit(main(sys.argv[1:]))
