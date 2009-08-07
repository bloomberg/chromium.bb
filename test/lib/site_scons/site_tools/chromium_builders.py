# TODO(sgk):  This is cut-and-paste from the Chromium project until
# the SCons generator can be made generic.  (It currently uses
# this functionality initially added for Chromium.)

# Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""
Stub tool module for adding, to a construction environment, Chromium-specific
wrappers around SCons builders.  This goes away once the configuration
of this tool module moves out GYP and into the Chromium configuration.
"""

def generate(env):
  pass

def exists(env):
  return True
