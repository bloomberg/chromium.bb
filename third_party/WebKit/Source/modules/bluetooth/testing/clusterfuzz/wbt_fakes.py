# Copyright 2016 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Module that contains information about Web Bluetooth's Fake Adapters."""

BLACKLISTED_UUID = '611c954a-263b-4f4a-aab6-01ddb953f985'
DISCONNECTION_UUID = '01d7d889-7451-419f-aeb8-d65e7b9277af'
GATT_ERROR_UUID = '000000a0-97e5-4cd7-b9f1-f5a427670c59'

CONNECTION_ERROR_UUIDS = [
    '00000000-97e5-4cd7-b9f1-f5a427670c59',
    '00000001-97e5-4cd7-b9f1-f5a427670c59',
    '00000002-97e5-4cd7-b9f1-f5a427670c59',
    '00000003-97e5-4cd7-b9f1-f5a427670c59',
    '00000004-97e5-4cd7-b9f1-f5a427670c59',
    '00000005-97e5-4cd7-b9f1-f5a427670c59',
    '00000006-97e5-4cd7-b9f1-f5a427670c59',
    '00000007-97e5-4cd7-b9f1-f5a427670c59',
    '00000008-97e5-4cd7-b9f1-f5a427670c59',
    '00000009-97e5-4cd7-b9f1-f5a427670c59',
    '0000000a-97e5-4cd7-b9f1-f5a427670c59',
    '0000000b-97e5-4cd7-b9f1-f5a427670c59',
    '0000000c-97e5-4cd7-b9f1-f5a427670c59',
    '0000000d-97e5-4cd7-b9f1-f5a427670c59',
    '0000000e-97e5-4cd7-b9f1-f5a427670c59',
]

# List of services that are included in our fake adapters.
ADVERTISED_SERVICES = [
    'generic_access',
    'glucose',
    'tx_power',
    'heart_rate',
    'human_interface_device',
    'device_information',
    BLACKLISTED_UUID,
    CONNECTION_ERROR_UUIDS[0],
    DISCONNECTION_UUID,
    GATT_ERROR_UUID,
]

# List of available fake adapters.
ALL_ADAPTERS = [
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

# List of fake adapters that include devices.
ADAPTERS_WITH_DEVICES = [
    (
        'GlucoseHeartRateAdapter',
        ['generic_access', 'heart_rate', 'glucose', 'tx_power'],
    ),
    (
        'MissingServiceHeartRateAdapter',
        ['generic_access', 'heart_rate'],
    ),
    (
        'MissingCharacteristicHeartRateAdapter',
        ['generic_access', 'heart_rate'],
    ),
    (
        'HeartRateAdapter',
        ['generic_access', 'heart_rate'],
    ),
    (
        'TwoHeartRateServicesAdapter',
        ['generic_access', 'heart_rate'],
    ),
    (
        'DisconnectingHeartRateAdapter',
        ['generic_access', 'heart_rate'],
    ),
    (
        'BlacklistTestAdapter',
        [BLACKLISTED_UUID, 'device_information', 'generic_access',
         'heart_rate', 'human_interface_device'],
    ),
    (
        'FailingConnectionsAdapter',
        CONNECTION_ERROR_UUIDS,
    ),
    (
        'FailingGATTOperationsAdapter',
        [GATT_ERROR_UUID],
    ),
    (
        'DelayedServicesDiscoveryAdapter',
        ['generic_access', 'heart_rate'],
    ),
]

# List of fake adapters with services.
ADAPTERS_WITH_SERVICES = [
    (
        'MissingCharacteristicHeartRateAdapter',
        ['generic_access', 'heart_rate'],
    ),
    (
        'HeartRateAdapter',
        ['generic_access', 'heart_rate'],
    ),
    (
        'TwoHeartRateServicesAdapter',
        ['generic_access', 'heart_rate'],
    ),
    (
        'DisconnectingHeartRateAdapter',
        ['generic_access', 'heart_rate', DISCONNECTION_UUID],
    ),
    (
        'BlacklistTestAdapter',
        [BLACKLISTED_UUID, 'device_information', 'generic_access',
         'heart_rate', 'human_interface_device'],
    ),
    (
        'FailingGATTOperationsAdapter',
        [GATT_ERROR_UUID],
    ),
    (
        'DelayedServicesDiscoveryAdapter',
        ['heart_rate']
    ),
]
