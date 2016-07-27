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
import wbt_fakes


# Strings that are used to generate the beginning of a test. The replacement
# fields are replaced by Get*Base() functions below to generate valid test
# cases.
BASIC_BASE = \
    '  return setBluetoothFakeAdapter({fake_adapter_name})\n'\
    '    .then(() => {{\n'

DEVICE_DISCOVERY_BASE = BASIC_BASE + \
    '      return requestDeviceWithKeyDown({{\n'\
    '        filters: [{{services: [{service_uuid}]}}]}});\n'\
    '    }})\n'\
    '    .then(device => {{\n'

CONNECTABLE_BASE = DEVICE_DISCOVERY_BASE + \
    '      return device.gatt.connect();\n'\
    '    }})\n'\
    '    .then(gatt => {{\n'

SERVICE_RETRIEVED_BASE = CONNECTABLE_BASE + \
    '      return gatt.getPrimaryService({service_uuid});\n'\
    '    }})\n'\
    '    .then(service => {{\n'


def _ToJsStr(s):
    return u'\'{}\''.format(s)


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


def GetAdvertisedServiceUUIDFromFakes():
    """Returns a random service string from the list of fake services."""
    return _ToJsStr(random.choice(wbt_fakes.ADVERTISED_SERVICES))


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


def GetAdvertisedServiceUUID():
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
        return GetAdvertisedServiceUUIDFromFakes()
    elif roll < 0.9:
        return GetValidServiceAlias()
    else:
        return GetRandomUUID()


def GetRequestDeviceOptions():
    """Returns an object used by navigator.bluetooth.requestDevice."""
    # TODO(ortuno): Randomize the members, number of filters, services, etc.

    return '{filters: [{services: [ %s ]}]}' % GetAdvertisedServiceUUID()


def GetBasicBase():
    """Returns a string that sets a random fake adapter."""
    adapter = _ToJsStr(random.choice(wbt_fakes.ALL_ADAPTERS))
    return BASIC_BASE.format(fake_adapter_name=adapter)


def GetDeviceDiscoveryBase():
    """Generates a string that contains all steps to find a device."""
    adapter, services = random.choice(wbt_fakes.ADAPTERS_WITH_DEVICES)
    return DEVICE_DISCOVERY_BASE.format(
        fake_adapter_name=_ToJsStr(adapter),
        service_uuid=_ToJsStr(random.choice(services)))


def GetConnectableBase():
    """Generates a string that contains all steps to connect to a device.

    Returns: A string that:
      1. Sets an adapter to a fake adapter with a connectable device.
      2. Looks for the connectable device.
      3. Connects to it.
    """
    adapter, services = random.choice(wbt_fakes.ADAPTERS_WITH_DEVICES)
    return DEVICE_DISCOVERY_BASE.format(
        fake_adapter_name=_ToJsStr(adapter),
        service_uuid=_ToJsStr(random.choice(services)))


def GetServiceRetrievedBase():
    """Returns a string that contains all steps to retrieve a service.

    Returns: A string that:
      1. Sets an adapter to a fake adapter with a connectable device with
         services.
      2. Use one of the device's services to look for that device.
      3. Connects to it.
      4. Retrieve the device's service used in 2.
    """
    adapter, services = random.choice(wbt_fakes.ADAPTERS_WITH_SERVICES)
    return SERVICE_RETRIEVED_BASE.format(
        fake_adapter_name=_ToJsStr(adapter),
        service_uuid=_ToJsStr(random.choice(services)))
