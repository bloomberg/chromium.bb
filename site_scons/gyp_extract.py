#!/usr/bin/python
# Copyright (c) 2011 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re


def LoadGypFile(gyp_filename):
  """Load the contents of a gyp file.

  Arguments:
    filename: filename of a .gyp file.
  Returns:
    Raw dict from .gyp file.
  """
  return eval(open(gyp_filename).read(), {}, {})

def IgnoreVariables(name):
  """E.g. map ppapi_cpp<(nacl_ppapi_library_suffix) to ppapi_cpp."""
  return re.sub('\<\([^)]*\)$', '', name)

def GypTargetSources(gyp_data, target_name, pattern):
  """Extract a sources from a target matching a given pattern.

  Arguments:
    gyp_data: dict previously load by LoadGypFile.
    target_name: target to extract from.
    pattern: re pattern that sources must match.
  Returns:
    A list of strings containing source filenames.
  """
  targets = [target for target in gyp_data['targets']
             if IgnoreVariables(target['target_name']) == target_name]
  # Only one target should have this name.
  assert len(targets) == 1
  desired_target = targets[0]
  # Extract source files that match.
  re_compiled = re.compile(pattern)
  return [source_file for source_file in desired_target['sources']
          if re_compiled.match(source_file)]
