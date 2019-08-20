# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The build API entry point."""

from __future__ import print_function

import os
import sys

from chromite.api import api_config as api_config_lib
from chromite.api import controller
from chromite.api import router as router_lib
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.utils import matching


def GetParser():
  """Build the argument parser."""
  parser = commandline.ArgumentParser(description=__doc__)

  call_group = parser.add_argument_group(
      'API Call Options',
      'These options are used to execute an endpoint. When making a call every '
      'argument in this group is required.')
  call_group.add_argument(
      'service_method',
      nargs='?',
      help='The "chromite.api.Service/Method" that is being called.')
  call_group.add_argument(
      '--input-json',
      type='path',
      help='Path to the JSON serialized input argument protobuf message.')
  call_group.add_argument(
      '--output-json',
      type='path',
      help='The path to which the result protobuf message should be written.')

  ux_group = parser.add_argument_group('Developer Options',
                                       'Options to help developers.')
  # Lists the full chromite.api.Service/Method, has both names to match
  # whichever mental model people prefer.
  ux_group.add_argument(
      '--list-methods',
      '--list-services',
      action='store_true',
      dest='list_services',
      help='List the name of each registered "chromite.api.Service/Method".')

  # Run configuration options.
  test_group = parser.add_argument_group(
      'Testing Options',
      'These options are used to execute various tests against the API. These '
      'options are mutually exclusive. Calling code can use these options to '
      'validate inputs and test their handling of each return code case for '
      'each endpoint.')
  call_modifications = test_group.add_mutually_exclusive_group()
  call_modifications.add_argument(
      '--validate-only',
      action='store_true',
      default=False,
      help='When set, only runs the argument validation logic. Calls produce '
           'a return code of 0 iff the input proto comprises arguments that '
           'are a valid call to the endpoint, or 1 otherwise.')
  # See: api/faux.py for the mock call and error implementations.
  call_modifications.add_argument(
      '--mock-call',
      action='store_true',
      default=False,
      help='When set, returns a valid, mock response rather than running the '
           'endpoint. This allows API consumers to more easily test their '
           'implementations against the version of the API being called. '
           'This argument will always result in a return code of 0.')
  call_modifications.add_argument(
      '--mock-error',
      action='store_true',
      default=False,
      help='When set, return a valid, mock error response rather than running '
           'the endpoint. This allows API consumers to test their error '
           'handling semantics against the version of the API being called. '
           'This argument will always result in a return code of 2 iff the '
           'endpoint ever produces a return code of 2, otherwise will always'
           'produce a return code of 1.')
  call_modifications.add_argument(
      '--mock-invalid',
      action='store_true',
      default=False,
      help='When set, return a mock validation error response rather than '
           'running the endpoint. This allows API consumers to test their '
           'validation error handling semantics against the version of the API '
           'being called without having to understand how to construct an '
           'invalid request. '
           'This argument will always result in a return code of 1.')

  return parser


def _ParseArgs(argv, router):
  """Parse and validate arguments."""
  parser = GetParser()
  opts = parser.parse_args(argv)

  methods = router.ListMethods()

  if opts.list_services:
    # We just need to print the methods and we're done.
    for method in methods:
      print(method)
    sys.exit(0)

  # Positional service_method argument validation.
  if not opts.service_method:
    parser.error('Must pass "Service/Method".')

  parts = opts.service_method.split('/')
  if len(parts) != 2:
    parser.error(
        'Must pass the correct format: (e.g. chromite.api.SdkService/Create).'
        'Use --list-methods to see a full list.')

  if opts.service_method not in methods:
    # Unknown method, try to match against known methods and make a suggestion.
    # This is just for developer sanity, e.g. misspellings when testing.
    matched = matching.GetMostLikelyMatchedObject(
        methods, opts.service_method, matched_score_threshold=0.6)
    error = 'Unrecognized service name.'
    if matched:
      error += '\nDid you mean: \n%s' % '\n'.join(matched)
    parser.error(error)

  opts.service = parts[0]
  opts.method = parts[1]

  # --input-json and --output-json validation.
  if not opts.input_json or not opts.output_json:
    parser.error('--input-json and --output-json are both required.')

  if not os.path.exists(opts.input_json):
    parser.error('Input file does not exist.')

  # Build the config object from the options.
  opts.config = api_config_lib.ApiConfig(
      validate_only=opts.validate_only,
      mock_call=opts.mock_call,
      mock_error=opts.mock_error)

  opts.Freeze()
  return opts


def main(argv):
  router = router_lib.GetRouter()

  opts = _ParseArgs(argv, router)

  if opts.mock_invalid:
    # --mock-invalid handling. We print error messages, but no output is ever
    # set for validation errors, so we can handle it by just giving back the
    # correct return code here.
    return controller.RETURN_CODE_INVALID_INPUT

  try:
    return router.Route(opts.service, opts.method, opts.input_json,
                        opts.output_json, opts.config)
  except router_lib.Error as e:
    # Handle router_lib.Error derivatives nicely, but let anything else bubble
    # up.
    cros_build_lib.Die(e.message)
