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
      // The header cares about color, duplex, and header/footer to determine
      // whether to show the enterprise managed icon, and pages, copies, and
      // duplex to compute the number of sheets of paper.
      const settings = {
        color: {
          value: true, /* color */
          unavailableValue: false,
          valid: true,
          available: true,
          setByPolicy: false,
          key: 'isColorEnabled',
        },
        copies: {
          value: '1',
          unavailableValue: '1',
          valid: true,
          available: true,
          setByPolicy: false,
          key: '',
        },
        duplex: {
          value: false,
          unavailableValue: false,
          valid: true,
          available: true,
          setByPolicy: false,
          key: 'isDuplexEnabled',
        },
        headerFooter: {
          value: true,
          unavailableValue: false,
          valid: true,
          available: true,
          setByPolicy: false,
          key: 'isHeaderFooterEnabled',
        },
        pages: {
          value: [1],
          unavailableValue: [],
          valid: true,
          available: true,
          setByPolicy: false,
          key: '',
        },
      };

      PolymerTest.clearBody();
      header = document.createElement('print-preview-header');
      header.settings = settings;
      header.destination = new print_preview.Destination(
          'FooDevice', print_preview.DestinationType.GOOGLE,
          print_preview.DestinationOrigin.COOKIES, 'FooName',
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
