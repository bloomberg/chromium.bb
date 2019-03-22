// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('destination_select_test', function() {
  /** @enum {string} */
  const TestNames = {
    SingleRecentDestination: 'single recent destination',
    MultipleRecentDestinations: 'multiple recent destinations',
    MultipleRecentDestinationsOneRequest:
        'multiple recent destinations one request',
    DefaultDestinationSelectionRules: 'default destination selection rules',
    SystemDefaultPrinterPolicy: 'system default printer policy',
    KioskModeSelectsFirstPrinter: 'kiosk mode selects first printer',
    NoPrintersShowsError: 'no printers shows error',
  };

  const suiteName = 'DestinationSelectTests';
  suite(suiteName, function() {
    /** @type {?PrintPreviewAppElement} */
    let page = null;

    /** @type {?PrintPreview.NativeLayerStub} */
    let nativeLayer = null;

    /** @type {?print_preview.NativeInitialSettngs} */
    let initialSettings = null;

    /** @type {!Array<!print_preview.LocalDestinationInfo>} */
    let localDestinations = [];

    /** @type {!Array<!print_preview.Destination>} */
    let destinations = [];

    /** @override */
    setup(function() {
      initialSettings = print_preview_test_utils.getDefaultInitialSettings();
      nativeLayer = new print_preview.NativeLayerStub();
      localDestinations = [];
      destinations = print_preview_test_utils.getDestinations(
          nativeLayer, localDestinations);
    });

    /*
     * Sets the initial settings to the stored value and creates the page.
     * @param {boolean=} opt_expectPrinterFailure Whether printer fetch is
     *     expected to fail
     * @return {!Promise} Promise that resolves when initial settings and,
     *     if printer failure is not expected, printer capabilities have
     *     been returned.
     */
    function setInitialSettings(opt_expectPrinterFailure) {
      nativeLayer.setInitialSettings(initialSettings);
      nativeLayer.setLocalDestinations(localDestinations);
      print_preview.NativeLayer.setInstance(nativeLayer);
      PolymerTest.clearBody();
      page = document.createElement('print-preview-app');
      document.body.appendChild(page);

      const promises = [nativeLayer.whenCalled('getInitialSettings')];
      if (!opt_expectPrinterFailure)
        promises.push(nativeLayer.whenCalled('getPrinterCapabilities'));
      return Promise.all(promises);
    }

    /**
     * Checks that a printer is displayed to the user with the name given
     * by |printerName|.
     * @param {string} printerName The printer name that should be displayed.
     */
    function assertPrinterDisplay(printerName) {
      const destinationSettings = page.$$('print-preview-destination-settings');

      // Check that the throbber is hidden and the destination info is shown.
      assertTrue(destinationSettings.$$('.throbber-container').hidden);
      assertFalse(destinationSettings.$$('.destination-settings-box').hidden);

      // Check that the destination matches the expected destination.
      assertEquals(
          printerName, destinationSettings.$$('.destination-name').textContent);
    }

    /**
     * Tests that if the user has a single valid recent destination the
     * destination is automatically reselected.
     */
    test(assert(TestNames.SingleRecentDestination), function() {
      const recentDestination =
          print_preview.makeRecentDestination(destinations[0]);
      initialSettings.serializedAppStateStr = JSON.stringify({
        version: 2,
        recentDestinations: [recentDestination],
      });

      return setInitialSettings().then(function(argsArray) {
        assertEquals('ID1', argsArray[1].destinationId);
        assertEquals(print_preview.PrinterType.LOCAL, argsArray[1].type);
        assertEquals('ID1', page.destination_.id);
        assertPrinterDisplay('One');
      });
    });

    /**
     * Tests that if the user has multiple valid recent destination the most
     * recent destination is automatically reselected and the remaining
     * destinations are marked as recent in the store.
     */
    test(assert(TestNames.MultipleRecentDestinations), function() {
      const recentDestinations = destinations.slice(0, 3).map(
          destination => print_preview.makeRecentDestination(destination));

      initialSettings.serializedAppStateStr = JSON.stringify({
        version: 2,
        recentDestinations: recentDestinations,
      });

      return setInitialSettings()
          .then(function(argsArray) {
            // Should have loaded ID1 as the selected printer, since it was most
            // recent.
            assertEquals('ID1', argsArray[1].destinationId);
            assertEquals(print_preview.PrinterType.LOCAL, argsArray[1].type);
            assertEquals('ID1', page.destination_.id);
            assertPrinterDisplay('One');

            // Load all local destinations.
            page.destinationStore_.startLoadDestinations(
                print_preview.PrinterType.LOCAL_PRINTER);
            return nativeLayer.whenCalled('getPrinters');
          })
          .then(function() {
            // Verify the correct printers are marked as recent in the store.
            const reportedPrinters = page.destinationStore_.destinations();
            destinations.forEach((destination, index) => {
              const match = reportedPrinters.find((reportedPrinter) => {
                return reportedPrinter.id == destination.id;
              });
              assertFalse(typeof match === 'undefined');
              assertEquals(index < 3, match.isRecent);
            });
          });
    });

    /**
     * Tests that if the user has multiple valid recent destinations, this
     * does not result in multiple calls to getPrinterCapabilities and the
     * correct destination is selected for the preview request.
     * For crbug.com/666595.
     */
    test(assert(TestNames.MultipleRecentDestinationsOneRequest), function() {
      const recentDestinations = destinations.slice(0, 3).map(
          destination => print_preview.makeRecentDestination(destination));

      initialSettings.serializedAppStateStr = JSON.stringify({
        version: 2,
        recentDestinations: recentDestinations,
      });

      return setInitialSettings()
          .then(function(argsArray) {
            // Should have loaded ID1 as the selected printer, since it was most
            // recent.
            assertEquals('ID1', argsArray[1].destinationId);
            assertEquals(print_preview.PrinterType.LOCAL, argsArray[1].type);
            assertEquals('ID1', page.destination_.id);

            return nativeLayer.whenCalled('getPreview');
          })
          .then(function(previewArgs) {
            const ticket = JSON.parse(previewArgs.printTicket);
            assertEquals(0, ticket.requestID);
            assertEquals('ID1', ticket.deviceName);

            // None of the other printers should have been loaded. Should only
            // have ID1 and Save as PDF. They will be loaded when the dialog is
            // opened and startLoadDestinations() is called.
            const reportedPrinters = page.destinationStore_.destinations();
            assertEquals(2, reportedPrinters.length);
            destinations.forEach((destination, index) => {
              if (destination.id == 'ID1')
                return;

              assertFalse(reportedPrinters.some(p => p.id == destination.id));
            });
          });
    });

    /**
     * Tests that if there are default destination selection rules they are
     * respected and a matching destination is automatically selected.
     */
    test(assert(TestNames.DefaultDestinationSelectionRules), function() {
      initialSettings.serializedDefaultDestinationSelectionRulesStr =
          JSON.stringify({namePattern: '.*Four.*'});
      initialSettings.serializedAppStateStr = '';
      return setInitialSettings().then(function(argsArray) {
        // Should have loaded ID4 as the selected printer, since it matches
        // the rules.
        assertEquals('ID4', argsArray[1].destinationId);
        assertEquals(print_preview.PrinterType.LOCAL, argsArray[1].type);
        assertEquals('ID4', page.destination_.id);
        assertPrinterDisplay('Four');
      });
    });

    /**
     * Tests that if the system default printer policy is enabled the system
     * default printer is automatically selected even if the user has recent
     * destinations.
     */
    test(assert(TestNames.SystemDefaultPrinterPolicy), function() {
      // Setup some recent destinations to ensure they are not selected.
      const recentDestinations = [];
      destinations.slice(0, 3).forEach(destination => {
        recentDestinations.push(
            print_preview.makeRecentDestination(destination));
      });

      initialSettings.serializedAppStateStr = JSON.stringify({
        version: 2,
        recentDestinations: recentDestinations,
      });

      return setInitialSettings().then(function(argsArray) {
        // Need to load FooDevice as the printer, since it is the system
        // default.
        assertEquals('FooDevice', argsArray[1].destinationId);
        assertEquals(print_preview.PrinterType.LOCAL, argsArray[1].type);
        assertEquals('FooDevice', page.destination_.id);
        assertPrinterDisplay('FooName');
      });
    });

    /**
     * Tests that if there is no system default destination, the default
     * selection rules and recent destinations are empty, and the preview
     * is in app kiosk mode (so no PDF printer), the first destination returned
     * from printer fetch is selected.
     */
    test(assert(TestNames.KioskModeSelectsFirstPrinter), function() {
      initialSettings.serializedDefaultDestinationSelectionRulesStr = '';
      initialSettings.serializedAppStateStr = '';
      initialSettings.isInAppKioskMode = true;
      initialSettings.printerName = '';

      return setInitialSettings().then(function(argsArray) {
        // Should have loaded the first destination as the selected printer.
        assertEquals(destinations[0].id, argsArray[1].destinationId);
        assertEquals(print_preview.PrinterType.LOCAL, argsArray[1].type);
        assertEquals(destinations[0].id, page.destination_.id);
        assertPrinterDisplay(destinations[0].displayName);
      });
    });

    /**
     * Tests that if there is no system default destination, the default
     * selection rules and recent destinations are empty, the preview
     * is in app kiosk mode (so no PDF printer), and there are no
     * destinations found, the no destinations found error is displayed.
     */
    test(assert(TestNames.NoPrintersShowsError), function() {
      initialSettings.serializedDefaultDestinationSelectionRulesStr = '';
      initialSettings.serializedAppStateStr = '';
      initialSettings.isInAppKioskMode = true;
      initialSettings.printerName = '';
      localDestinations = [];

      return Promise
          .all([
            setInitialSettings(true),
            test_util.eventToPromise(
                print_preview.DestinationStore.EventType.NO_DESTINATIONS_FOUND,
                page.destinationStore_),
          ])
          .then(function() {
            assertEquals(undefined, page.destination_);
            const destinationSettings =
                page.$$('print-preview-destination-settings');
            assertTrue(destinationSettings.$$('.throbber-container').hidden);
            assertTrue(
                destinationSettings.$$('.destination-settings-box').hidden);
            assertFalse(
                destinationSettings.$$('.no-destinations-display').hidden);
          });
    });
  });

  return {
    suiteName: suiteName,
    TestNames: TestNames,
  };
});
