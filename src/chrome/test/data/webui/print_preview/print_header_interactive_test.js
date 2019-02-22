// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_header_interactive_test', function() {
  /** @enum {string} */
  const TestNames = {
    FocusPrintOnReady: 'focus print on ready',
  };

  const suiteName = 'PrintHeaderInteractiveTest';

  suite(suiteName, function() {
    /** @type {?PrintPreviewHeaderElement} */
    let header = null;

    /** @override */
    setup(function() {
      // Only care about copies, duplex, pages, and pages per sheet.
      const settings = {
        copies: {
          value: '1',
          unavailableValue: '1',
          valid: true,
          available: true,
          key: '',
        },
        duplex: {
          value: false,
          unavailableValue: false,
          valid: true,
          available: true,
          key: 'isDuplexEnabled',
        },
        pages: {
          value: [1],
          unavailableValue: [],
          valid: true,
          available: true,
          key: '',
        },
        pagesPerSheet: {
          value: 1,
          unavailableValue: 1,
          valid: true,
          available: true,
          key: '',
        },
      };

      PolymerTest.clearBody();
      header = document.createElement('print-preview-header');
      header.settings = settings;
      header.destination = new print_preview.Destination(
          'FooDevice', print_preview.DestinationType.GOOGLE,
          print_preview.DestinationOrigin.COOKIES, 'FooName',
          true /* isRecent */,
          print_preview.DestinationConnectionStatus.ONLINE);
      header.errorMessage = '';
      header.state = print_preview_new.State.NOT_READY;
      document.body.appendChild(header);
    });

    // Tests that the print button is automatically focused when the destination
    // is ready.
    test(assert(TestNames.FocusPrintOnReady), function() {
      const printButton = header.$$('.action-button');
      assertTrue(!!printButton);
      const whenFocusDone = test_util.eventToPromise('focus', printButton);

      // Simulate initialization finishing.
      header.state = print_preview_new.State.READY;
      return whenFocusDone;
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
