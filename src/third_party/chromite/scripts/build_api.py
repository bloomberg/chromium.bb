# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The build API entry point."""

from __future__ import print_function

import os
import sys

from chromite.api import api_config as api_config_lib
from chromite.api import router as router_lib
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.utils import matching


def GetParser():
  """Build the argument parser."""
  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument('service_method', nargs='?',
                      help='The "chromite.api.Service/Method" that is being '
                           'called.')
  parser.add_argument(
      '--input-json', type='path',
      help='Path to the JSON serialized input argument protobuf message.')
  parser.add_argument(
      '--output-json', type='path',
      help='The path to which the result protobuf message should be written.')
  parser.add_argument(
      '--validate-only', action='store_true', default=False,
      help='When set, only runs the argument validation logic. Calls produce'
           'a return code of 0 iff the arguments comprise a valid call to the'
           'endpoint, or 1 otherwise.')
  parser.add_argument(
      '--list-services', action='store_true',
      help='List the names of the registered services.')

  return parser


def _ParseArgs(argv, router):
  """Parse and validate arguments."""
  parser = GetParser()
  opts = parser.parse_args(argv)

  methods = router.ListMethods()

  if opts.list_services:
    for method in methods:
      print(method)
    sys.exit(0)

  if not opts.service_method:
    parser.error('Must pass "Service/Method".')

  parts = opts.service_method.split('/')
  if len(parts) != 2:
    parser.error(
        'Must pass the correct format: (i.e. chromite.api.SdkService/Create)')

  if not opts.input_json or not opts.output_json:
    parser.error('--input-json and --output-json are both required.')

  if opts.service_method not in methods:
    # Unknown method, try to match against known methods and make a suggestion.
    # This is just for developer sanity, e.g. misspellings when testing.
    matched = matching.GetMostLikelyMatchedObject(methods, opts.service_method,
                                                  matched_score_threshold=0.6)
    error = 'Unrecognized service name.'
    if matched:
      error += '\nDid you mean: \n%s' % '\n'.join(matched)
    parser.error(error)

  opts.service = parts[0]
  opts.method = parts[1]

  if not os.path.exists(opts.input_json):
    parser.error('Input file does not exist.')

  opts.config = api_config_lib.ApiConfig(validate_only=opts.validate_only)

  opts.Freeze()
  return opts


def main(argv):
  router = router_lib.GetRouter()

  opts = _ParseArgs(argv, router)

  try:
    return router.Route(opts.service, opts.method, opts.input_json,
                        opts.output_json, opts.config)
  except router_lib.Error as e:
    # Handle router_lib.Error derivatives nicely, but let anything else bubble
    # up.
    cros_build_lib.Die(e.message)
