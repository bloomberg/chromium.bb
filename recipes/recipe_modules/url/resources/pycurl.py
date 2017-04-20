#!/usr/bin/env python
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# NOTE: This was imported from Chromium's "tools/build" at revision:
# 65976b6e2a612439681dc42830e90dbcdf550f40

import argparse
import json
import logging
import os
import sys
import time

# Add vendored "requests" to path.
SCRIPT_PATH = os.path.abspath(
    os.path.join(os.path.dirname(__file__), 'third_party', 'requests_2_10_0'))
sys.path.insert(0, SCRIPT_PATH)
import requests


def main():
  parser = argparse.ArgumentParser(
      description='Get a url and print its document.',
      prog='./runit.py pycurl.py')
  parser.add_argument('url', help='the url to fetch')
  parser.add_argument('--outfile', help='write output to this file')
  parser.add_argument('--attempts', type=int, default=1,
      help='The number of attempts make (with exponential backoff) before '
           'failing.')
  parser.add_argument('--headers-json', type=str,
      help='A json file containing any headers to include with the request.')
  args = parser.parse_args()

  headers = None
  if args.headers_json:
    with open(args.headers_json, 'r') as json_file:
      headers = json.load(json_file)

  if args.attempts < 1:
    args.attempts = 1

  logging.info('Connecting...')
  retry_delay_seconds = 2
  for i in xrange(args.attempts):
    r = requests.get(args.url, headers=headers)
    if r.status_code == requests.codes.ok:
      break
    logging.error('(%d/%d) Request returned status code: %d',
        i+1, args.attempts, r.status_code)
    if (i+1) >= args.attempts:
      r.raise_for_status()
    logging.info('Sleeping %d seconds, then retrying.', retry_delay_seconds)
    time.sleep(retry_delay_seconds)
    retry_delay_seconds *= 2

  if args.outfile:
    total = 0.0
    logging.info('Downloading...')
    with open(args.outfile, 'wb') as f:
      for chunk in r.iter_content(1024*1024):
        f.write(chunk)
        total += len(chunk)
        logging.info('Downloaded %.1f MB so far', total / 1024 / 1024)
  else:
    print r.text


if __name__ == '__main__':
  logging.basicConfig()
  logging.getLogger().setLevel(logging.INFO)
  sys.exit(main())
