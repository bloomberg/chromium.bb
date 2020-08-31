#!/usr/bin/env python
# Copyright 2020 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""This script is used to download the clang update script. It runs as a
gclient hook.

It's equivalent to using curl to download the latest update script:

  $ curl --silent --create-dirs -o tools/clang/scripts/update.py \
      https://raw.githubusercontent.com/chromium/chromium/master/tools/clang/scripts/update.py

The purpose of "reinventing the wheel" with this script is just so developers
aren't required to have curl installed.
"""

import argparse
import os
import sys

try:
  from urllib2 import HTTPError, URLError, urlopen
except ImportError: # For Py3 compatibility
  from urllib.error import HTTPError, URLError
  from urllib.request import urlopen

SCRIPT_DOWNLOAD_URL = ('https://raw.githubusercontent.com/' +
                       'chromium/chromium/master/tools/clang/scripts/update.py')

def main():
  parser = argparse.ArgumentParser(
      description='Download clang update script from chromium master.')
  parser.add_argument('--output',
                      help='Path to script file to create/overwrite.')
  args = parser.parse_args()

  if not args.output:
    print('usage: download-clang-update-script.py ' +
          '--output=tools/clang/scripts/update.py');
    return 1

  script_contents = ''
  try:
    response = urlopen(SCRIPT_DOWNLOAD_URL)
    script_contents = response.read()
  except HTTPError as e:
    print e.code
    print e.read()
    return 1
  except URLError as e:
    print 'Download failed. Reason: ', e.reason
    return 1

  directory = os.path.dirname(args.output)
  if not os.path.exists(directory):
    os.makedirs(directory)

  script_file = open(args.output, 'w')
  script_file.write(script_contents)
  script_file.close()

  return 0

if __name__ == '__main__':
  sys.exit(main())
