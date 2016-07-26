# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module to get random numbers, strings, etc.

   The values returned by the various functions can be replaced in
   templates to generate test cases.
"""

import random
import sys
import uuid

# This script needs the utils.py and fuzzy_types.py modules in order
# to work. This files are copied by the setup.py script and not checked-in
# next to this code, so we need to disable the style warning.
# pylint: disable=F0401
from resources import utils
from resources import fuzzy_types

import gatt_aliases


# List of services that are included in our fake adapters.
FAKE_SERVICES = [
    'generic_access',
    'glucose',
    'heart_rate',
    'battery_service',
    'human_interface_device',
    '000000a0-97e5-4cd7-b9f1-f5a427670c59',  # Error UUID
    'device_information',
    '611c954a-263b-4f4a-aab6-01ddb953f985',  # Blacklisted
    '01d7d889-7451-419f-aeb8-d65e7b9277af',  # Request Disconnection
]

# List of available fake adapters.
FAKE_ADAPTERS = [
    'BaseAdapter',
    'NotPresentAdapter',
    'NotPoweredAdapter',
    'EmptyAdapter',
    'FailStartDiscoveryAdapter',
    'GlucoseHeartRateAdapter',
    'UnicodeDeviceAdapter',
    'MissingServiceHeartRateAdapter',
    'MissingCharacteristicHeartRateAdapter',
    'HeartRateAdapter',
    'TwoHeartRateServicesAdapter',
    'DisconnectingHeartRateAdapter',
    'BlacklistTestAdapter',
    'FailingConnectionsAdapter',
    'FailingGATTOperationsAdapter',
    'DelayedServicesDiscoveryAdapter',
]


def _ToJsStr(s):
    return u'\'{}\''.format(s)


def GetFakeAdapter():
    return _ToJsStr(random.choice(FAKE_ADAPTERS))


def _GetFuzzedJsString(s):
    """Returns a fuzzed string based on |s|.

    Args:
      s: The base string to fuzz.
    Returns:
      A single line string surrounded by quotes.
    """
    while True:
        fuzzed_string = fuzzy_types.FuzzyString(s)
        try:
            fuzzed_string = fuzzed_string.decode('utf8')
        except UnicodeDecodeError:
            print 'Can\'t decode fuzzed string. Trying again.'
        else:
            fuzzed_string = '\\n'.join(fuzzed_string.split())
            fuzzed_string = fuzzed_string.replace('\'', r'\'')
            return _ToJsStr(fuzzed_string)


def GetServiceUUIDFromFakes():
    """Returns a random service string from the list of fake services."""
    return _ToJsStr(random.choice(FAKE_SERVICES))


def GetValidServiceAlias():
    """Returns a valid service alias from the list of services aliases."""
    return _ToJsStr(random.choice(gatt_aliases.SERVICES))


def GetRandomUUID():
    """Returns a random UUID, a random number or a fuzzed uuid or alias."""
    choice = random.choice(['uuid', 'number', 'fuzzed string'])
    if choice == 'uuid':
        return _ToJsStr(uuid.uuid4())
    elif choice == 'number':
        return utils.UniformExpoInteger(0, sys.maxsize.bit_length() + 1)
    elif choice == 'fuzzed string':
        choice2 = random.choice(['uuid', 'alias'])
        if choice2 == 'uuid':
            random_uuid = str(uuid.uuid4())
            return _GetFuzzedJsString(random_uuid)
        elif choice2 == 'alias':
            alias = random.choice(gatt_aliases.SERVICES)
            return _GetFuzzedJsString(alias)


def GetServiceUUID():
    """Generates a random Service UUID from a set of functions.

    See GetServiceUUIDFromFakes(), GetValidServiceAlias() and GetRandomUUID()
    for the different values this function can return.

    This function weights GetServiceUUIDFromFakes() more heavily to increase the
    probability of generating test pages that can interact with the fake
    adapters.

    Returns:
      A string or a number that can be used as a Service UUID by the Web
      Bluetooth API.
    """
    roll = random.random()
    if roll < 0.8:
        return GetServiceUUIDFromFakes()
    elif roll < 0.9:
        return GetValidServiceAlias()
    else:
        return GetRandomUUID()


def GetRequestDeviceOptions():
    """Returns an object used by navigator.bluetooth.requestDevice."""
    # TODO(ortuno): Randomize the members, number of filters, services, etc.

    return '{filters: [{services: [ %s ]}]}' % GetServiceUUID()
