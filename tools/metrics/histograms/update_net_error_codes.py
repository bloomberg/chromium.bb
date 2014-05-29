#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Updates NetErrorCodes enum in histograms.xml file with values read
 from net_error_list.h.

If the file was pretty-printed, the updated version is pretty-printed too.
"""

import os.path
import re
import sys

sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'common'))
from update_histogram_enum import UpdateHistogramFromDict

NET_ERROR_LIST_PATH = '../../../net/base/net_error_list.h'

def ReadNetErrorCodes(filename):
  """Reads in values from net_error_list.h, returning a dictionary mapping
  error code to error name.
  """
  # Read the file as a list of lines
  with open(filename) as f:
    content = f.readlines()

  ERROR_REGEX = re.compile(r'^NET_ERROR\(([\w]+), -([0-9]+)\)')

  # Parse out lines that are net errors.
  errors = {}
  for line in content:
    m = ERROR_REGEX.match(line)
    if m:
      errors[int(m.group(2))] = m.group(1)
  return errors

def main():
  if len(sys.argv) > 1:
    print >>sys.stderr, 'No arguments expected!'
    sys.stderr.write(__doc__)
    sys.exit(1)

  UpdateHistogramFromDict(
    'NetErrorCodes', ReadNetErrorCodes(NET_ERROR_LIST_PATH),
    NET_ERROR_LIST_PATH)

if __name__ == '__main__':
  main()
