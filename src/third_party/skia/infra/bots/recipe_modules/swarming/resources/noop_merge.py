#!/usr/bin/env python
# Copyright 2017 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import shutil
import sys


def noop_merge(output_json, jsons_to_merge):
  """Use the first supplied JSON as the output JSON.

  Primarily intended for unsharded tasks.

  Args:
    output_json: A path to a JSON file to which the results should be written.
    jsons_to_merge: A list of paths to JSON files.
  """
  if len(jsons_to_merge) > 1:
    print >> sys.stderr, (
        'Multiple JSONs provided: %s' % ','.join(jsons_to_merge))
    return 1
  if jsons_to_merge:
    shutil.copyfile(jsons_to_merge[0], output_json)
  else:
    with open(output_json, 'w') as f:
      json.dump({}, f)
  return 0


def main(raw_args):
  parser = argparse.ArgumentParser()
  parser.add_argument('--build-properties', help=argparse.SUPPRESS)
  parser.add_argument('--summary-json', help=argparse.SUPPRESS)
  parser.add_argument('-o', '--output-json', required=True)
  parser.add_argument('jsons_to_merge', nargs='*')

  args = parser.parse_args(raw_args)

  return noop_merge(args.output_json, args.jsons_to_merge)


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
