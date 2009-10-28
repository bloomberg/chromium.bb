#!/usr/bin/python2.6.2
# Copyright 2009, Google Inc.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of Google Inc. nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
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


"""Defines common O3D test runner constants. This file determines paths to
other O3D components relatively, so it must be placed in the right location.

"""

import os
import sys

import util

join = os.path.join

# Make sure OS is supported.
if not util.IsWindows() and not util.IsMac() and not util.IsLinux():
  print 'Only Windows, Mac, and Linux are supported.'
  sys.exit(1)

# This path should be root/o3d/tests.
TEST_PATH = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# This path should be root/o3d.
O3D_PATH = os.path.dirname(TEST_PATH)
# This path should be root, i.e., the checkout location.
BASE_PATH = os.path.dirname(O3D_PATH)


HOME = os.path.expanduser('~')
if HOME == '~':
  print 'Cannot find user home directory.'
  sys.exit(1)

if util.IsWindows():
  PYTHON = r'C:\Python24\python.exe'
else:
  PYTHON = 'python'

# Note: this path may or may not exist.
RESULTS_PATH = join(TEST_PATH, 'results')

# Build directories.
if util.IsWindows():
  BUILD_PATH = join(O3D_PATH, 'build')
elif util.IsMac():
  BUILD_PATH = join(BASE_PATH, 'xcodebuild')
else:
  BUILD_PATH = join(BASE_PATH, 'sconsbuild')
  
if os.path.exists(join(BUILD_PATH, 'Debug')):
  PRODUCT_DIR_PATH = join(BUILD_PATH, 'Debug')
elif os.path.exists(join(BUILD_PATH, 'Release')):
  PRODUCT_DIR_PATH = join(BUILD_PATH, 'Release')
else:
  print 'Cannot find Debug or Release folder in ' + BUILD_PATH
  sys.exit(1)
  
# Plugin locations.
INSTALL_PATHS = []
if util.IsWindows():
  INSTALL_PATHS += [join(HOME, 'Application Data', 'Mozilla',
                         'plugins', 'npo3dautoplugin.dll')]
  INSTALL_PATHS += [join(HOME, 'Application Data', 'Google', 'O3D',
                         'o3d_host.dll')]
elif util.IsMac():
  INSTALL_PATHS += ['/Library/Internet Plug-Ins/O3D.plugin']
else:
  INSTALL_PATHS += [join(HOME, '.mozilla', 'plugins',
                         'libnpo3dautoplugin.so')]
