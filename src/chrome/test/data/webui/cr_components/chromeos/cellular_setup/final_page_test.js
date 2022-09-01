// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_components/chromeos/cellular_setup/final_page.m.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertTrue} from '../../../chai_assert.js';

import {FakeCellularSetupDelegate} from './fake_cellular_setup_delegate.js';

suite('CrComponentsFinalPageTest', function() {
  let finalPage;
  setup(function() {
    finalPage = document.createElement('final-page');
    finalPage.delegate = new FakeCellularSetupDelegate();
    document.body.appendChild(finalPage);
    flush();
  });

  test('Base test', function() {
    const basePage = finalPage.$$('base-page');
    assertTrue(!!basePage);
  });
});
