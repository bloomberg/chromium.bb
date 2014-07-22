# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import sys


GIT_HASH_PATTERN = re.compile(r'^[0-9a-fA-F]{40}$')


def GetOSName(platform_name=sys.platform):
  if platform_name == 'cygwin' or platform_name.startswith('win'):
    return 'win'
  elif platform_name.startswith('linux'):
    return 'unix'
  elif platform_name.startswith('darwin'):
    return 'mac'
  else:
    return platform_name


def IsGitHash(revision):
  return GIT_HASH_PATTERN.match(str(revision))
