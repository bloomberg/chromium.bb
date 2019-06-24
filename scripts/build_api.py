# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The build API entry point."""

from __future__ import print_function

import os

from chromite.api import router as router_lib
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.utils import matching


def GetParser():
  """Build the argument parser."""
  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument('service_method',
                      help='The "chromite.api.Service/Method" that is being '
                           'called.')

  parser.add_argument(
      '--input-json', type='path', required=True,
      help='Path to the JSON serialized input argument protobuf message.')
  parser.add_argument(
      '--output-json', type='path', required=True,
      help='The path to which the result protobuf message should be written.')

  return parser


def _ParseArgs(argv, router):
  """Parse and validate arguments."""
  parser = GetParser()
  opts = parser.parse_args(argv)

  methods = router.ListMethods()
  if opts.service_method not in methods:
    # Unknown method, try to match against known methods and make a suggestion.
    # This is just for developer sanity, e.g. misspellings when testing.
    matched = matching.GetMostLikelyMatchedObject(methods, opts.service_method,
                                                  matched_score_threshold=0.6)
    error = 'Unrecognized service name.'
    if matched:
      error += '\nDid you mean: \n%s' % '\n'.join(matched)
    parser.error(error)

  parts = opts.service_method.split('/')

  if len(parts) != 2:
    parser.error('Must pass "Service/Method".')

  opts.service = parts[0]
  opts.method = parts[1]

  if not os.path.exists(opts.input_json):
    parser.error('Input file does not exist.')

  opts.Freeze()
  return opts


def main(argv):
  router = router_lib.GetRouter()

  opts = _ParseArgs(argv, router)

  try:
    return router.Route(opts.service, opts.method, opts.input_json,
                        opts.output_json)
  except router_lib.Error as e:
    # Handle router_lib.Error derivatives nicely, but let anything else bubble
    # up.
    cros_build_lib.Die(e.message)
