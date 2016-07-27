# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module to fuzz parameters of a template."""

import constraints
from fuzzer_helpers import FillInParameter


def FuzzParameters(test_file_data):
    """Fuzzes the data in the string provided.

    For now this function only replaces the TRANSFORM_SERVICE_UUID parameter
    with a valid UUID string.

    Args:
      test_file_data: String that contains parameters to be replaced.

    Returns:
      A string containing the value of test_file_data but with all its
      parameters replaced.
    """

    test_file_data = FillInParameter('TRANSFORM_BASIC_BASE',
                                     constraints.GetBasicBase,
                                     test_file_data)

    test_file_data = FillInParameter('TRANSFORM_DEVICE_DISCOVERY_BASE',
                                     constraints.GetDeviceDiscoveryBase,
                                     test_file_data)

    test_file_data = FillInParameter('TRANSFORM_CONNECTABLE_BASE',
                                     constraints.GetConnectableBase,
                                     test_file_data)

    test_file_data = FillInParameter('TRANSFORM_SERVICE_RETRIEVED_BASE',
                                     constraints.GetServiceRetrievedBase,
                                     test_file_data)

    test_file_data = FillInParameter('TRANSFORM_REQUEST_DEVICE_OPTIONS',
                                     constraints.GetRequestDeviceOptions,
                                     test_file_data)

    return test_file_data
