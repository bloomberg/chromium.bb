# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module to generate a test file with random calls to the Web Bluetooth API."""

import random
from fuzzer_helpers import FillInParameter

# Contains the different types of base tokens used when generating a test case.
BASE_TOKENS = [
    'TRANSFORM_BASIC_BASE',
    'TRANSFORM_DEVICE_DISCOVERY_BASE',
    'TRANSFORM_CONNECTABLE_BASE',
    'TRANSFORM_SERVICE_RETRIEVED_BASE',
]

# Contains strings that represent calls to the Web Bluetooth API. These
# strings can be sequentially placed together to generate sequences of
# calls to the API. These strings are separated by line and placed so
# that indentation can be performed more easily.
TOKENS = [
    [
        '})',
        '.catch(e => console.log(e.name + \': \' + e.message))',
        '.then(() => {',
    ],
    # Request Device Tokens
    [
        '  requestDeviceWithKeyDown(TRANSFORM_REQUEST_DEVICE_OPTIONS);'
    ],
    [
        '  return requestDeviceWithKeyDown(TRANSFORM_REQUEST_DEVICE_OPTIONS);',
        '})',
        '.then(device => {',
    ],
    # Connect Tokens
    [
        '  device.gatt.connect();',
    ],
    [
        '  return device.gatt.connect();',
        '})'
        '.then(gatt => {',
    ],
    [
        '  gatt.connect();',
    ],
    [
        '  return gatt.connect();',
        '})'
        '.then(gatt => {',
    ],
    # GetPrimaryService(s) Tokens
    [
        '  device.gatt.getPrimaryServices();',
    ],
    [
        '  gatt.getPrimaryServices();',
    ],
    [
        '  return device.gatt.getPrimaryServices();',
        '})'
        '.then(services => {',
    ],
    [
        '  return gatt.getPrimaryServices();',
        '})'
        '.then(services => {',
    ],
]

INDENT = '    '
BREAK = '\n'
END_TOKEN = '});'

# Maximum number of tokens that will be inserted in the generated
# test case.
MAX_NUM_OF_TOKENS = 100


def _GenerateSequenceOfRandomTokens():
    """Generates a sequence of calls to the Web Bluetooth API.

    Uses the arrays of strings in TOKENS and randomly picks a number between
    [1, 100] to generate a random sequence of calls to the Web Bluetooth API,
    calls to reload the page, and calls to perform  garbage collection.

    Returns:
      A string containing a sequence of calls to the Web Bluetooth API.
    """
    result = random.choice(BASE_TOKENS)

    for _ in xrange(random.randint(1, MAX_NUM_OF_TOKENS)):
        # Get random token.
        token = random.choice(TOKENS)

        # Indent and break line.
        for line in token:
            result += INDENT + line + BREAK

    result += INDENT + END_TOKEN

    return result


def GenerateTestFile(template_file_data):
    """Inserts a sequence of calls to the Web Bluetooth API into a template.

    Args:
      template_file_data: A template containing the 'TRANSFORM_RANDOM_TOKENS'
          string.
    Returns:
      A string consisting of template_file_data with the string
        'TRANSFORM_RANDOM_TOKENS' replaced with a sequence of calls to the Web
        Bluetooth API and calls to reload the page and perform garbage
        collection.
    """

    return FillInParameter('TRANSFORM_RANDOM_TOKENS',
                           _GenerateSequenceOfRandomTokens,
                           template_file_data)
