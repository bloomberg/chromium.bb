# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module to generate a test file with random calls to the Web Bluetooth API."""

import random
from fuzzer_helpers import FillInParameter

# Contains strings that represent calls to the Web Bluetooth API. These
# strings can be sequentially placed together to generate sequences of
# calls to the API. These strings are separated by line and placed so
# that indentation can be performed more easily.
TOKENS = [
    [
        '  requestDeviceWithKeyDown(TRANSFORM_REQUEST_DEVICE_OPTIONS);',
    ],
    [
        '  return requestDeviceWithKeyDown(TRANSFORM_REQUEST_DEVICE_OPTIONS);',
        '})',
        '.then(device => {',
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
    result = ''
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
