// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';

import {fakeUpdateProviderTest} from './fake_update_provider_test.js';
import {firmwareUpdateAppTest} from './firmware_update_test.js';
import {peripheralUpdatesListTest} from './peripheral_updates_list_test.js';
import {updateCardTest} from './update_card_test.js';

window.test_suites_list = [];

function runSuite(suiteName, testFn) {
  window.test_suites_list.push(suiteName);
  suite(suiteName, testFn);
}

runSuite('FakeUpdateProviderTest', fakeUpdateProviderTest);
runSuite('FirmwareUpdateApp', firmwareUpdateAppTest);
runSuite('PeripheralUpdatesListTest', peripheralUpdatesListTest);
runSuite('UpdateCardTest', updateCardTest);
