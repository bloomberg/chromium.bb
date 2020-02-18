# -*- coding: utf-8 -*-
# Copyright 2019 The Chromium OS Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Payload API Service."""

from __future__ import print_function

from chromite.api import controller
from chromite.lib import cros_build_lib
from chromite.api import faux
from chromite.api import validate
from chromite.service import payload


_VALID_IMAGE_PAIRS = (('src_signed_image', 'tgt_signed_image'),
                      ('src_unsigned_image', 'tgt_unsigned_image'),
                      ('full_update', 'tgt_unsigned_image'),
                      ('full_update', 'tgt_signed_image'))


# We have more fields we might validate however, they're either
# 'oneof' or allowed to be the empty value by design. If @validate
# gets more complex in the future we can add more here.
@faux.all_empty
@validate.require('bucket')
def GeneratePayload(input_proto, output_proto, config):
  """Generate a update payload ('do paygen').

  Args:
    input_proto (PayloadGenerationRequest): Input proto.
    output_proto (PayloadGenerationResult): Output proto.
    config (api.config.ApiConfig): The API call config.

  Returns:
    A controller return code (e.g. controller.RETURN_CODE_SUCCESS).
  """

  # Resolve the tgt image oneof.
  tgt_name = input_proto.WhichOneof('tgt_image_oneof')
  try:
    tgt_image = getattr(input_proto, tgt_name)
  except AttributeError:
    cros_build_lib.Die('%s is not a known tgt image type' % (tgt_name,))

  # Resolve the src image oneof.
  src_name = input_proto.WhichOneof('src_image_oneof')

  # If the source image is 'full_update' we lack a source entirely.
  if src_name == 'full_update':
    src_image = None
  # Otherwise we have an image.
  else:
    try:
      src_image = getattr(input_proto, src_name)
    except AttributeError:
      cros_build_lib.Die('%s is not a known src image type' % (src_name,))

  # Ensure they are compatible oneofs.
  if (src_name, tgt_name) not in _VALID_IMAGE_PAIRS:
    cros_build_lib.Die('%s and %s are not valid image pairs' %
                       (src_image, tgt_image))

  # Find the value of bucket or default to 'chromeos-releases'.
  destination_bucket = input_proto.bucket or 'chromeos-releases'

  # There's a potential that some paygen_lib library might raise here, but since
  # we're still involved in config we'll keep it before the validate_only.
  payload_config = payload.PayloadConfig(
      tgt_image,
      src_image,
      destination_bucket,
      input_proto.verify,
      input_proto.keyset)

  # If configured for validation only we're done here.
  if config.validate_only:
    return controller.RETURN_CODE_VALID_INPUT

  # Do payload generation.
  paygen_ok = payload_config.GeneratePayload()
  _SetGeneratePayloadOutputProto(output_proto, paygen_ok)

  if paygen_ok:
    return controller.RETURN_CODE_SUCCESS
  else:
    return controller.RETURN_CODE_COMPLETED_UNSUCCESSFULLY


def _SetGeneratePayloadOutputProto(output_proto, generate_payload_ok):
  """Set the output proto with the results from the service class.

  Args:
    output_proto (PayloadGenerationResult_pb2): The output proto.
    generate_payload_ok (bool): value to set output_proto.success.
  """
  output_proto.success = generate_payload_ok
