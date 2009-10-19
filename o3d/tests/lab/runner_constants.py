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


"""Defines common O3D test runner constants.

"""

import os
import sys

import util

join = os.path.join

if util.IsWindows():
  AUTO_PATH = r'C:\auto'
  PYTHON = r'C:\Python24\python.exe'
  if util.IsXP():
    HOME_PATH = r'C:\Documents and Settings\testing'
  else:
    HOME_PATH = r'C:\Users\testing'

elif util.IsMac():
  AUTO_PATH = '/Users/testing/auto'
  PYTHON = 'python'
  HOME_PATH = '/Users/testing'

elif util.IsLinux():
  AUTO_PATH = '/home/testing/auto'
  PYTHON = 'python'
  HOME_PATH = '/home/testing'

else:
  print 'Only Windows, Mac, and Linux are supported.'
  sys.exit(1)

O3D_PATH = join(AUTO_PATH, 'o3d')
SCRIPTS_PATH = join(AUTO_PATH, 'scripts')
RESULTS_PATH = join(AUTO_PATH, 'results')
SOFTWARE_PATH = join(AUTO_PATH, 'software')

# Build directories.
if util.IsWindows():
  BUILD_PATH = join(O3D_PATH, 'o3d', 'build')
elif util.IsMac():
  BUILD_PATH = join(O3D_PATH, 'xcodebuild')
else:
  BUILD_PATH = join(O3D_PATH, 'sconsbuild')
  
if os.path.exists(join(BUILD_PATH, 'Debug')):
  PRODUCT_DIR_PATH = join(BUILD_PATH, 'Debug')
else:
  PRODUCT_DIR_PATH = join(BUILD_PATH, 'Release')
  
# Plugin locations.
INSTALL_PATHS = []
if util.IsWindows():
  INSTALL_PATHS += [join(HOME_PATH, 'Application Data', 'Mozilla',
                         'plugins', 'npo3dautoplugin.dll')]
  INSTALL_PATHS += [join(HOME_PATH, 'Application Data', 'Google', 'O3D',
                         'o3d_host.dll')]
elif util.IsMac():
  INSTALL_PATHS += ['/Library/Internet Plug-Ins/O3D.plugin']
else:
  INSTALL_PATHS += [join(HOME_PATH, '.mozilla', 'plugins',
                         'libnpo3dautoplugin.so')]
