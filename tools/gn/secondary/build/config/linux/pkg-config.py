# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import json
import subprocess
import sys
import re
from optparse import OptionParser

# This script runs pkg-config, optionally filtering out some results, and
# returns the result.
#
# The result will be [ <includes>, <cflags>, <libs>, <lib_dirs> ] where each
# member is itself a list of strings.
#
# You can filter out matches using "-v <regexp>" where all results from
# pkgconfig matching the given regular expression will be ignored. You can
# specify more than one regular expression my specifying "-v" more than once.

# If this is run on non-Linux platforms, just return nothing and indicate
# success. This allows us to "kind of emulate" a Linux build from other
# platforms.
if sys.platform.find("linux") == -1:
  print "[[],[],[]]"
  sys.exit(0)

parser = OptionParser()
parser.add_option('-v', action='append', dest='strip_out', type='string')
(options, args) = parser.parse_args()

# Make a list of regular expressions to strip out.
strip_out = []
if options.strip_out != None:
  for regexp in options.strip_out:
    strip_out.append(re.compile(regexp))

try:
  flag_string = subprocess.check_output(["pkg-config", "--cflags", "--libs"] +
      args)
  # For now just split on spaces to get the args out. This will break if
  # pkgconfig returns quoted things with spaces in them, but that doesn't seem
  # to happen in practice.
  all_flags = flag_string.strip().split(' ')
except:
  print "Could not run pkg-config."
  sys.exit(1)

includes = []
cflags = []
libs = []
lib_dirs = []

def MatchesAnyRegexp(flag, list_of_regexps):
  for regexp in list_of_regexps:
    if regexp.search(flag) != None:
      return True
  return False

for flag in all_flags[:]:
  if len(flag) == 0 or MatchesAnyRegexp(flag, strip_out):
    continue;

  if flag[:2] == '-l':
    libs.append(flag[2:])
  if flag[:2] == '-L':
    lib_dirs.append(flag[2:])
  elif flag[:2] == '-I':
    includes.append(flag[2:])
  else:
    cflags.append(flag)

# Output a GN array, the first one is the cflags, the second are the libs. The
# JSON formatter prints GN compatible lists when everything is a list of
# strings.
print json.dumps([includes, cflags, libs, lib_dirs])
