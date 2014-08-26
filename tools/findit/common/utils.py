# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import re
import sys

from http_client_local import HttpClientLocal


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


def GetHttpClient():
  # TODO(stgao): return implementation for appengine when running on appengine.
  return HttpClientLocal


def JoinLineNumbers(line_numbers, accepted_gap=1):
  """Join line numbers into line blocks.

  Args:
    line_numbers: a list of line number.
    accepted_gap: if two line numbers are within the give gap,
                  they would be combined together into a block.
                  Eg: for (1, 2, 3, 6, 7, 8, 12), if |accepted_gap| = 1, result
                  would be 1-3, 6-8, 12; if |accepted_gap| = 3, result would be
                  1-8, 12; if |accepted_gap| =4, result would be 1-12.
  """
  if not line_numbers:
    return ''

  line_numbers = map(int, line_numbers)
  line_numbers.sort()

  block = []
  start_line_number = line_numbers[0]
  last_line_number = start_line_number
  for current_line_number in line_numbers[1:]:
    if last_line_number + accepted_gap < current_line_number:
      if start_line_number == last_line_number:
        block.append('%d' % start_line_number)
      else:
        block.append('%d-%d' % (start_line_number, last_line_number))
      start_line_number = current_line_number
    last_line_number = current_line_number
  else:
    if start_line_number == last_line_number:
      block.append('%d' % start_line_number)
    else:
      block.append('%d-%d' % (start_line_number, last_line_number))

  return ', '.join(block)
