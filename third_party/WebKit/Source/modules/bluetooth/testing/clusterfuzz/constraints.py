# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module to get random numbers, strings, etc.

   The values returned by the various functions can be replaced in
   templates to generate test cases.
"""

import random


def GetValidUUIDString():
    """Constructs a valid UUID string.

    Returns:
      A string representating a valid UUID according to:
      https://webbluetoothcg.github.io/web-bluetooth/#valid-uuid
    """

    return '\'{:08x}-{:04x}-{:04x}-{:04x}-{:012x}\''.format(
        random.getrandbits(32),
        random.getrandbits(16),
        random.getrandbits(16),
        random.getrandbits(16),
        random.getrandbits(48))


def GetRequestDeviceOptions():
    """Returns an object used by navigator.bluetooth.requestDevice."""
    # TODO(ortuno): Randomize the members, number of filters, services, etc.

    return '{filters: [{services: [ %s ]}]}' % GetValidUUIDString()
