// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/strings.m.js';
// #import 'chrome://resources/cr_components/chromeos/cellular_setup/setup_loading_page.m.js';

// #import {flush, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {assertFalse, assertTrue} from '../../../chai_assert.js';
// clang-format on

suite('CrComponentsSetupLoadingPageTest', function() {
  let setupLoadingPage;
  let basePage;

  setup(function() {
    setupLoadingPage = document.createElement('setup-loading-page');
    document.body.appendChild(setupLoadingPage);
    Polymer.dom.flush();

    basePage = setupLoadingPage.$$('base-page');
    assertTrue(!!basePage);
  });

  test('Loading animation and error graphic shown correctly', function() {
    setupLoadingPage.isSimDetectError = false;
    Polymer.dom.flush();
    assertTrue(!!setupLoadingPage.$$('#animationContainer'));
    assertTrue(setupLoadingPage.$$('#simDetectError').hidden);

    setupLoadingPage.isSimDetectError = true;
    Polymer.dom.flush();
    assertFalse(!!setupLoadingPage.$$('#animationContainer'));
    assertFalse(setupLoadingPage.$$('#simDetectError').hidden);
  });
});
