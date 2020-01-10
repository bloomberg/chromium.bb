# -*- coding: utf-8 -*-
# Copyright 2018 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""The Build API entry point."""

from __future__ import print_function

import os

from google.protobuf import json_format

from chromite.api import api_config as api_config_lib
from chromite.api import controller
from chromite.api import router as router_lib
from chromite.api.gen.chromite.api import build_api_config_pb2
from chromite.lib import commandline
from chromite.lib import cros_build_lib
from chromite.lib import cros_logging as logging
from chromite.lib import osutils
from chromite.lib import tee
from chromite.utils import matching


def GetParser():
  """Build the argument parser."""
  parser = commandline.ArgumentParser(description=__doc__)

  parser.add_argument(
      'service_method',
      help='The "chromite.api.Service/Method" that is being called.')
  parser.add_argument(
      '--input-json',
      type='path',
      help='Path to the JSON serialized input argument protobuf message.')
  parser.add_argument(
      '--output-json',
      type='path',
      help='The path to which the result protobuf message should be written.')
  parser.add_argument(
      '--config-json',
      type='path',
      help='The path to the Build API call configs.')
  # TODO(crbug.com/1040978): Remove after usages removed.
  parser.add_argument(
      '--tee-log',
      type='path',
      help='The path to which stdout and stderr should be teed to.')

  return parser


def _ParseArgs(argv, router):
  """Parse and validate arguments."""
  parser = GetParser()
  opts, unknown = parser.parse_known_args(
      argv, namespace=commandline.ArgumentNamespace())
  parser.DoPostParseSetup(opts, unknown)

  if unknown:
    logging.warning('Unknown args ignored: %s', ' '.join(unknown))

  methods = router.ListMethods()

  # Positional service_method argument validation.
  parts = opts.service_method.split('/')
  if len(parts) != 2:
    parser.error('Invalid service/method specification format. It should be '
                 'something like chromite.api.SdkService/Create.')

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

  config_msg = build_api_config_pb2.BuildApiConfig()
  if opts.config_json:
    try:
      json_format.Parse(osutils.ReadFile(opts.config_json), config_msg,
                        ignore_unknown_fields=True)
    except IOError as e:
      parser.error(e)

  opts.config = api_config_lib.build_config_from_proto(config_msg)

  opts.Freeze()
  return opts


def main(argv):
  with cros_build_lib.ContextManagerStack() as stack:

    router = router_lib.GetRouter()
    opts = _ParseArgs(argv, router)

    if opts.tee_log:
      stack.Add(tee.Tee, opts.tee_log)
      logging.info('Teeing stdout and stderr to %s', opts.tee_log)
    if opts.config.log_path:
      stack.Add(tee.Tee, opts.config.log_path)
      logging.info('Teeing stdout and stderr to %s', opts.config.log_path)
    tee_log_env_value = os.environ.get('BUILD_API_TEE_LOG_FILE')
    if tee_log_env_value:
      stack.Add(tee.Tee, tee_log_env_value)
      logging.info('Teeing stdout and stderr to env path %s', tee_log_env_value)

    if opts.config.mock_invalid:
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
      cros_build_lib.Die(e)
