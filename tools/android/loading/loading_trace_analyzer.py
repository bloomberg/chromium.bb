#! /usr/bin/env python
# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import argparse
import json
import re
import sys

from loading_trace import LoadingTrace
import request_track


def _ArgumentParser():
  """Builds a command line argument's parser.
  """
  parser = argparse.ArgumentParser()
  subparsers = parser.add_subparsers(dest='subcommand', help='subcommand line')

  # requests listing subcommand.
  requests_parser = subparsers.add_parser('requests',
      help='Lists all request from the loading trace.')
  requests_parser.add_argument('loading_trace', type=file,
      help='Input loading trace to see the cache usage from.')
  requests_parser.add_argument('--output',
      type=argparse.FileType(),
      default=sys.stdout,
      help='Output destination path if different from stdout.')
  requests_parser.add_argument('--output-format', type=str, default='{url}',
      help='Output line format (Default to "{url}")')
  requests_parser.add_argument('--where',
      dest='where_statement', type=str,
      nargs=2, metavar=('FORMAT', 'REGEX'), default=[],
      help='Where statement to filter such as: --where "{protocol}" "https?"')
  return parser


def _RequestsSubcommand(args):
  """`loading_trace_analyzer.py requests` Command line tool entry point.

  Example:
    Lists all request with timing:
      ... requests --output-format "{timing} {url}"

    Lists  HTTP/HTTPS requests that have used the cache:
      ... requests --where "{protocol} {from_disk_cache}" "https?\S* True"
  """
  where_format = None
  where_statement = None
  if args.where_statement:
    where_format = args.where_statement[0]
    try:
      where_statement = re.compile(args.where_statement[1])
    except re.error as e:
      sys.stderr.write("Invalid where statement REGEX: {}\n{}\n".format(
          args.where_statement[1], str(e)))
      return 1

  loading_trace = LoadingTrace.FromJsonDict(json.load(args.loading_trace))
  for request_event in loading_trace.request_track.GetEvents():
    request_event_json = request_event.ToJsonDict()

    if where_statement != None:
      where_in = where_format.format(**request_event_json)
      if not where_statement.match(where_in):
        continue

    args.output.write(args.output_format.format(**request_event_json) + '\n')
  return 0


def main(command_line_args):
  """Command line tool entry point.
  """
  args = _ArgumentParser().parse_args(command_line_args)
  if args.subcommand == 'requests':
    return _RequestsSubcommand(args)
  assert False


if __name__ == '__main__':
  sys.exit(main(sys.argv[1:]))
