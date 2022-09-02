#!/usr/bin/env python

# Copyright (C) 2013 Bloomberg L.P. All rights reserved.
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

# This script is based on gyp_chromium, and contains code copied from it.  That
# file carries the following license:
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This script is a wrapper around gyp, which runs gyp using the most useful
# configurations for blpwtk2: 'msvsNinja', 'ninjaDebug', 'ninjaRelease'.
#
# In addition to the standard gyp arguments, you can pass the names of any of
# these configurations on the command line.  If no configurations are
# specified, then gyp will be invoked with all three configurations.
#
#                       msvsNinja         ninjaDebug        ninjaRelease
# -------------------------------------------------------------------------
# GYP_GENERATORS        msvs-ninja        ninja             ninja
# GYP_GENERATOR_FLAGS
#   msvs_version        *unset*           2013              2013
#   config              *unset*           Debug             Release
#
# Note that values set in GYP_GENERATOR_FLAGS from the shell's environment will
# override the defaults specified above.  Also, any -G flags passed on the
# command-line will override everything.
#
# Also note that, if GYP_GENERATORS environment is set, then this script just
# calls gyp once, without setting up the above configurations.  This is useful
# for doing project generation of a custom configuration.
#
# The path to the gyp file may be specified on the command-line.  If not
# specified, it will then be obtained from the 'BLPWTK2_GYP_FILE' environment
# variable, then the 'CHROMIUM_GYP_FILE' environment variable.  If neither is
# set, then 'src/blpwtk2/blpwtk2.gyp' will be used.
#
# This script also sets up the Windows SDK path to a patched version of the
# Windows 8.0 SDK (this is for Bloomberg L.P. developers).  Also do it for the
# DirectX SDK.

import os, sys, glob, shlex

script_dir = os.path.dirname(os.path.realpath(__file__))
chrome_src = os.path.abspath(os.path.join(script_dir, os.pardir))

sys.path.insert(0, os.path.join(chrome_src, 'tools', 'gyp', 'pylib'))
import gyp

# Assume this file is in a one-level-deep subdirectory of the source root.
SRC_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))

# Add paths so that pymod_do_main(...) can import files.
sys.path.insert(1, os.path.join(chrome_src, 'tools', 'generate_shim_headers'))
sys.path.insert(1, os.path.join(chrome_src, 'tools', 'grit'))
sys.path.insert(1, os.path.join(chrome_src, 'chrome', 'tools', 'build'))
sys.path.insert(1, os.path.join(chrome_src, 'native_client', 'build'))
sys.path.insert(1, os.path.join(chrome_src, 'native_client_sdk', 'src',
    'build_tools'))
sys.path.insert(1, os.path.join(chrome_src, 'third_party', 'WebKit',
    'Source', 'build', 'scripts'))
sys.path.insert(1, os.path.join(chrome_src, 'third_party', 'liblouis'))

# On Windows, Psyco shortens warm runs of build/gyp_chromium by about
# 20 seconds on a z600 machine with 12 GB of RAM, from 90 down to 70
# seconds.  Conversely, memory usage of build/gyp_chromium with Psyco
# maxes out at about 158 MB vs. 132 MB without it.
#
# Psyco uses native libraries, so we need to load a different
# installation depending on which OS we are running under. It has not
# been tested whether using Psyco on our Mac and Linux builds is worth
# it (the GYP running time is a lot shorter, so the JIT startup cost
# may not be worth it).
if sys.platform == 'win32':
  try:
    sys.path.insert(0, os.path.join(chrome_src, 'third_party', 'psyco_win32'))
    import psyco
  except:
    psyco = None
else:
  psyco = None


def GetSupplementalFiles():
  """Returns a list of the supplemental files that are included in all GYP
  sources."""
  return glob.glob(os.path.join(chrome_src, '*', 'supplement.gypi'))


def additional_include_files(supplemental_files, args=[]):
  """
  Returns a list of additional (.gypi) files to include, without duplicating
  ones that are already specified on the command line. The list of supplemental
  include files is passed in as an argument.
  """
  # Determine the include files specified on the command line.
  # This doesn't cover all the different option formats you can use,
  # but it's mainly intended to avoid duplicating flags on the automatic
  # makefile regeneration which only uses this format.
  specified_includes = set()
  for arg in args:
    if arg.startswith('-I') and len(arg) > 2:
      specified_includes.add(os.path.realpath(arg[2:]))

  result = []
  def AddInclude(path):
    if os.path.realpath(path) not in specified_includes:
      result.append(path)

  # Always include common.gypi.
  AddInclude(os.path.join(script_dir, 'common.gypi'))

  # Optionally add supplemental .gypi files if present.
  for supplement in supplemental_files:
    AddInclude(supplement)

  return result


def execGyp(args):
  print "Running gyp with this environment:"
  print "    GYP_GENERATORS = '" + os.environ['GYP_GENERATORS'] + "'"
  if 'GYP_MSVS_VERSION' in os.environ:
    print "    GYP_MSVS_VERSION = '" + os.environ['GYP_MSVS_VERSION'] + "'"
  if 'GYP_GENERATOR_FLAGS' in os.environ:
    print "    GYP_GENERATOR_FLAGS = '" + os.environ['GYP_GENERATOR_FLAGS'] + "'"
  if 'GYP_DEFINES' in os.environ:
    print "    GYP_DEFINES = '" + os.environ['GYP_DEFINES'] + "'"
  if len(args) > 0:
    print "    CmdlineArgs: " + " ".join(args)

  sys.stdout.flush()
  return gyp.main(args)

origGeneratorFlags = None

def saveOriginalEnv():
  global origGeneratorFlags
  if 'GYP_GENERATOR_FLAGS' in os.environ:
    origGeneratorFlags = os.environ['GYP_GENERATOR_FLAGS']
  else:
    origGeneratorFlags = None

def restoreOriginalEnv():
  if origGeneratorFlags != None:
    os.environ['GYP_GENERATOR_FLAGS'] = origGeneratorFlags
  elif 'GYP_GENERATOR_FLAGS' in os.environ:
    del os.environ['GYP_GENERATOR_FLAGS']

def applyVariableToEnvironment(env, var, val):
  if env in os.environ:
    envItems = os.environ[env].split(" ")
  else:
    envItems = []
  found = False
  for i in range(0, len(envItems)):
    iSplit = envItems[i].split("=")
    if iSplit[0] == var:
      # Don't change it
      found = True
      break
  if not found:
    envItems.append(var + "=" + val)
  os.environ[env] = " ".join(envItems)

def doMsvsNinja(args):
  os.environ['GYP_GENERATORS'] = 'msvs-ninja'
  return execGyp(args)

def doNinjaDebug(args):
  os.environ['GYP_GENERATORS'] = 'ninja'
  applyVariableToEnvironment('GYP_GENERATOR_FLAGS', 'msvs_version', '2013')
  applyVariableToEnvironment('GYP_GENERATOR_FLAGS', 'config', 'Debug')
  return execGyp(args)

def doNinjaRelease(args):
  os.environ['GYP_GENERATORS'] = 'ninja'
  applyVariableToEnvironment('GYP_GENERATOR_FLAGS', 'msvs_version', '2013')
  applyVariableToEnvironment('GYP_GENERATOR_FLAGS', 'config', 'Release')
  return execGyp(args)

def main(args):
  # This could give false positives since it doesn't actually do real option
  # parsing.  Oh well.
  gyp_file_specified = False
  generatorsToUse = []
  for arg in args:
    if arg == 'msvsNinja' or arg == 'ninjaDebug' or arg == 'ninjaRelease':
      generatorsToUse.append(arg)
    elif arg.endswith('.gyp'):
      gyp_file_specified = True

  # If none of them are specified, then do all of them.
  if not generatorsToUse:
    generatorsToUse.append('msvsNinja')
    generatorsToUse.append('ninjaDebug')
    generatorsToUse.append('ninjaRelease')

  args = filter(lambda x: x not in generatorsToUse, args)

  # If we didn't get a file, check an env var, and then fall back to
  # assuming 'src/blpwtk2/blpwtk2.gyp'.
  if not gyp_file_specified:
    gyp_file = os.environ.get('BLPWTK2_GYP_FILE') or \
        os.environ.get('CHROMIUM_GYP_FILE')
    if gyp_file:
      # Note that CHROMIUM_GYP_FILE values can't have backslashes as
      # path separators even on Windows due to the use of shlex.split().
      args.extend(shlex.split(gyp_file))
    else:
      args.append(os.path.join(chrome_src, 'blpwtk2', 'blpwtk2.gyp'))
      args.extend(['--root-target', 'blpwtk2_all'])

  supplemental_includes = GetSupplementalFiles()

  args.extend(
      ['-I' + i for i in additional_include_files(supplemental_includes, args)])

  args.append('--no-circular-check')

  # For Bloomberg developers, setup path to the patched Windows 8.0 SDK and the
  # DirectX SDK.  If you don't have BPCDEV_PATH setup, then you will get it
  # from the default installed location.
  bpcDevPath = os.environ.get('BPCDEV_PATH')

  # Add default definitions to GYP_DEFINES
  defaultDefines = 'blpwtk2=1'

  if 'GYP_DEFINES' in os.environ:
    os.environ['GYP_DEFINES'] += ' ' + defaultDefines
  else:
    os.environ['GYP_DEFINES'] = defaultDefines


  if 'GYP_GENERATORS' in os.environ:
    return execGyp(args)

  saveOriginalEnv()
  for generator in generatorsToUse:
    if 'msvsNinja' == generator:
      rc = doMsvsNinja(args)
    elif 'ninjaDebug' == generator:
      rc = doNinjaDebug(args)
    elif 'ninjaRelease' == generator:
      rc = doNinjaRelease(args)

    if 0 != rc:
        return rc
    restoreOriginalEnv()

  return 0

if __name__ == '__main__':
  # Use the Psyco JIT if available.
  if psyco:
    psyco.profile()
    print "Enabled Psyco JIT."

  sys.exit(main(sys.argv[1:]))

