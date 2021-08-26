// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CloudPrintInterfaceEventType, Destination, DestinationConnectionStatus, DestinationErrorType, DestinationOrigin, DestinationStore, DestinationType, LocalDestinationInfo, makeRecentDestination, NativeInitialSettings, NativeLayerImpl, PrinterType} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {isChromeOS, isLacros} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {eventToPromise} from '../test_util.m.js';

import {CloudPrintInterfaceStub} from './cloud_print_interface_stub.js';
// <if expr="chromeos or lacros">
import {setNativeLayerCrosInstance} from './native_layer_cros_stub.js';
// </if>
import {NativeLayerStub} from './native_layer_stub.js';
import {createDestinationStore, createDestinationWithCertificateStatus, getCddTemplate, getDefaultInitialSettings, getDestinations, getGoogleDriveDestination, getSaveAsPdfDestination, setupTestListenerElement} from './print_preview_test_utils.js';

window.destination_store_test = {};
const destination_store_test = window.destination_store_test;
destination_store_test.suiteName = 'DestinationStoreTest';
/** @enum {string} */
destination_store_test.TestNames = {
  SingleRecentDestination: 'single recent destination',
  MultipleRecentDestinations: 'multiple recent destinations',
  RecentCloudPrintFallback:
      'failure to load cloud print destination results in save as pdf',
  MultipleRecentDestinationsOneRequest:
      'multiple recent destinations one request',
  MultipleRecentDestinationsAndCloudPrint:
      'multiple recents and a Cloud Print destination',
  DefaultDestinationSelectionRules: 'default destination selection rules',
  SystemDefaultPrinterPolicy: 'system default printer policy',
  KioskModeSelectsFirstPrinter: 'kiosk mode selects first printer',
  NoPrintersShowsError: 'no printers shows error',
  UnreachableRecentCloudPrinter: 'unreachable recent cloud printer',
  RecentSaveAsPdf: 'recent save as pdf',
  MultipleRecentDestinationsAccounts: 'multiple recent destinations accounts',
  LoadAndSelectDestination: 'select loaded destination',
  MultipleRecentDestinationsAccountsCros:
      'multiple recent destinations accounts for Chrome OS',
  LoadSaveToDriveCros: 'load Save to Drive Cros',
  DriveNotMounted: 'drive not mounted',
};

suite(destination_store_test.suiteName, function() {
  /** @type {!DestinationStore} */
  let destinationStore;

  /** @type {!NativeLayerStub} */
  let nativeLayer;

  /** @type {!CloudPrintInterfaceStub} */
  let cloudPrintInterface;

  /** @type {!NativeInitialSettings} */
  let initialSettings;

  /** @type {!Array<string>} */
  let userAccounts = [];

  /** @type {!Array<!LocalDestinationInfo>} */
  let localDestinations = [];

  /** @type {!Array<!Destination>} */
  let cloudDestinations = [];

  /** @type {!Array<!Destination>} */
  let destinations = [];

  /** @type {number} */
  let numPrintersSelected = 0;

  /** @override */
  setup(function() {
    // Clear the UI.
    document.body.innerHTML = '';

    setupTestListenerElement();

    nativeLayer = new NativeLayerStub();
    NativeLayerImpl.instance_ = nativeLayer;
    // <if expr="chromeos or lacros">
    setNativeLayerCrosInstance();
    // </if>

    initialSettings = getDefaultInitialSettings();
    localDestinations = [];
    destinations = getDestinations(localDestinations);
  });

  /*
   * Sets the initial settings to the stored value and creates the page.
   * @param {boolean=} opt_expectPrinterFailure Whether printer fetch is
   *     expected to fail
   * @param {boolean=} opt_cloudPrintEnabled Whether the cloud print interface
   *     should be present
   * @return {!Promise} Promise that resolves when initial settings and,
   *     if printer failure is not expected, printer capabilities have
   *     been returned.
   */
  function setInitialSettings(
      opt_expectPrinterFailure, opt_cloudPrintEnabled = true) {
    // Set local print list.
    nativeLayer.setLocalDestinations(localDestinations);

    // Create destination store.
    destinationStore = createDestinationStore();

    // Create cloud print interface if it's enabled. Otherwise, skip setting it
    // to replicate the behavior in DestinationSettings.
    if (opt_cloudPrintEnabled) {
      cloudPrintInterface = new CloudPrintInterfaceStub();
      cloudDestinations.forEach(cloudDestination => {
        cloudPrintInterface.setPrinter(cloudDestination);
      });
      destinationStore.setCloudPrintInterface(cloudPrintInterface);
    }

    destinationStore.addEventListener(
        DestinationStore.EventType.DESTINATION_SELECT, function() {
          numPrintersSelected++;
        });

    // Initialize.
    const recentDestinations = initialSettings.serializedAppStateStr ?
        JSON.parse(initialSettings.serializedAppStateStr).recentDestinations :
        [];
    const whenCapabilitiesReady = eventToPromise(
        DestinationStore.EventType.SELECTED_DESTINATION_CAPABILITIES_READY,
        destinationStore);
    destinationStore.init(
        initialSettings.pdfPrinterDisabled, !!initialSettings.isDriveMounted,
        initialSettings.printerName,
        initialSettings.serializedDefaultDestinationSelectionRulesStr,
        recentDestinations);

    if (userAccounts) {
      destinationStore.setActiveUser(userAccounts[0]);
      destinationStore.reloadUserCookieBasedDestinations(userAccounts[0]);
    }
    return opt_expectPrinterFailure ? Promise.resolve() : Promise.race([
      nativeLayer.whenCalled('getPrinterCapabilities'), whenCapabilitiesReady
    ]);
  }

  /**
   * Tests that if the user has a single valid recent destination the
   * destination is automatically reselected.
   */
  test(
      assert(destination_store_test.TestNames.SingleRecentDestination),
      function() {
        const recentDestination = makeRecentDestination(destinations[0]);
        initialSettings.serializedAppStateStr = JSON.stringify({
          version: 2,
          recentDestinations: [recentDestination],
        });

        return setInitialSettings(false).then(function(args) {
          assertEquals('ID1', args.destinationId);
          assertEquals(PrinterType.LOCAL_PRINTER, args.printerType);
          assertEquals('ID1', destinationStore.selectedDestination.id);
        });
      });

  /**
   * Tests that if the user has multiple valid recent destinations the most
   * recent destination is automatically reselected and its capabilities are
   * fetched.
   */
  test(
      assert(destination_store_test.TestNames.MultipleRecentDestinations),
      function() {
        const recentDestinations = destinations.slice(0, 3).map(
            destination => makeRecentDestination(destination));

        initialSettings.serializedAppStateStr = JSON.stringify({
          version: 2,
          recentDestinations: recentDestinations,
        });

        return setInitialSettings(false).then(function(args) {
          // Should have loaded ID1 as the selected printer, since it was most
          // recent.
          assertEquals('ID1', args.destinationId);
          assertEquals(PrinterType.LOCAL_PRINTER, args.printerType);
          assertEquals('ID1', destinationStore.selectedDestination.id);
          // Verify that all local printers have been added to the store.
          const reportedPrinters = destinationStore.destinations();
          destinations.forEach((destination, index) => {
            const match = reportedPrinters.find((reportedPrinter) => {
              return reportedPrinter.id === destination.id;
            });
            assertFalse(typeof match === 'undefined');
          });
        });
      });

  /**
   * Tests that if the user has multiple recent destinations and a Cloud Print
   * destination, the most recent destination is automatically reselected and
   * its capabilities are fetched except for the Cloud Print destination.
   */
  test(
      assert(destination_store_test.TestNames
                 .MultipleRecentDestinationsAndCloudPrint),
      function() {
        // Convert the first 3 entries into recents.
        const recentDestinations = destinations.slice(0, 3).map(
            destination => makeRecentDestination(destination));

        const cloudPrintDestination = new Destination(
            'cp_id', DestinationType.GOOGLE, DestinationOrigin.COOKIES,
            'Cloud Printer', DestinationConnectionStatus.ONLINE);
        // Insert a Cloud Print printer into recent destinations.
        recentDestinations.unshift(
            makeRecentDestination(cloudPrintDestination));

        initialSettings.serializedAppStateStr = JSON.stringify({
          version: 2,
          recentDestinations: recentDestinations,
        });

        // For accounts that are not allowed to use Cloud Print, they get a null
        // interface object.
        return setInitialSettings(false, false).then(function(args) {
          // Should have loaded ID1 as the selected printer, since it was most
          // recent.
          assertEquals('ID1', args.destinationId);
          assertEquals(PrinterType.LOCAL_PRINTER, args.printerType);
          assertEquals('ID1', destinationStore.selectedDestination.id);

          // Verify that all local printers have been added to the store.
          const reportedPrinters = destinationStore.destinations();
          destinations.forEach((destination, index) => {
            if (isChromeOS || isLacros) {
              assertEquals(DestinationOrigin.CROS, destination.origin);
            } else {
              assertEquals(DestinationOrigin.LOCAL, destination.origin);
            }
            const match = reportedPrinters.find((reportedPrinter) => {
              return reportedPrinter.id === destination.id;
            });
            assertFalse(typeof match === 'undefined');
          });

          // The Cloud Print printer should be missing.
          const match = reportedPrinters.find(
              reportedPrinter =>
                  cloudPrintDestination.id === reportedPrinter.id);
          assertTrue(typeof match === 'undefined');
        });
      });

  /**
   * Tests that if the user has a recent Cloud Print destination selected, we
   * fail to initialize the Cloud Print interface, and no other destinations are
   * available, we fall back to Save As PDF.
   */
  test(
      assert(destination_store_test.TestNames.RecentCloudPrintFallback),
      function() {
        const cloudPrintDestination = new Destination(
            'cp_id', DestinationType.GOOGLE, DestinationOrigin.COOKIES,
            'Cloud Printer', DestinationConnectionStatus.ONLINE);
        const recentDestination = makeRecentDestination(cloudPrintDestination);
        initialSettings.serializedAppStateStr = JSON.stringify({
          version: 2,
          recentDestinations: [recentDestination],
        });
        localDestinations = [];

        // For accounts that are not allowed to use Cloud Print, they get a null
        // interface object.
        return setInitialSettings(false, false).then(function(args) {
          assertEquals(
              Destination.GooglePromotedId.SAVE_AS_PDF,
              destinationStore.selectedDestination.id);
        });
      });

  /**
   * Tests that if the user has multiple valid recent destinations, the
   * correct destination is selected for the preview request.
   * For crbug.com/666595.
   */
  test(
      assert(destination_store_test.TestNames
                 .MultipleRecentDestinationsOneRequest),
      function() {
        const recentDestinations = destinations.slice(0, 3).map(
            destination => makeRecentDestination(destination));

        initialSettings.serializedAppStateStr = JSON.stringify({
          version: 2,
          recentDestinations: recentDestinations,
        });

        return setInitialSettings(false).then(function(args) {
          // Should have loaded ID1 as the selected printer, since it was most
          // recent.
          assertEquals('ID1', args.destinationId);
          assertEquals(PrinterType.LOCAL_PRINTER, args.printerType);
          assertEquals('ID1', destinationStore.selectedDestination.id);

          // The other local destinations should be in the store, but only one
          // should have been selected so there was only one preview request.
          const reportedPrinters = destinationStore.destinations();
          const expectedPrinters = isChromeOS || isLacros ? 7 : 6;
          assertEquals(expectedPrinters, reportedPrinters.length);
          destinations.forEach((destination, index) => {
            assertTrue(reportedPrinters.some(p => p.id === destination.id));
          });
          assertEquals(1, numPrintersSelected);
        });
      });

  /**
   * Tests that if there are default destination selection rules they are
   * respected and a matching destination is automatically selected.
   */
  test(
      assert(destination_store_test.TestNames.DefaultDestinationSelectionRules),
      function() {
        initialSettings.serializedDefaultDestinationSelectionRulesStr =
            JSON.stringify({namePattern: '.*Four.*'});
        initialSettings.serializedAppStateStr = '';
        return setInitialSettings(false).then(function(args) {
          // Should have loaded ID4 as the selected printer, since it matches
          // the rules.
          assertEquals('ID4', args.destinationId);
          assertEquals(PrinterType.LOCAL_PRINTER, args.printerType);
          assertEquals('ID4', destinationStore.selectedDestination.id);
        });
      });

  /**
   * Tests that if the system default printer policy is enabled the system
   * default printer is automatically selected even if the user has recent
   * destinations.
   */
  test(
      assert(destination_store_test.TestNames.SystemDefaultPrinterPolicy),
      function() {
        // Set the policy in loadTimeData.
        loadTimeData.overrideValues({useSystemDefaultPrinter: true});

        // Setup some recent destinations to ensure they are not selected.
        const recentDestinations = [];
        destinations.slice(0, 3).forEach(destination => {
          recentDestinations.push(makeRecentDestination(destination));
        });

        initialSettings.serializedAppStateStr = JSON.stringify({
          version: 2,
          recentDestinations: recentDestinations,
        });

        return Promise
            .all([
              setInitialSettings(false),
              eventToPromise(
                  DestinationStore.EventType
                      .SELECTED_DESTINATION_CAPABILITIES_READY,
                  destinationStore),
            ])
            .then(() => {
              // Need to load FooDevice as the printer, since it is the system
              // default.
              assertEquals(
                  'FooDevice', destinationStore.selectedDestination.id);
            });
      });

  /**
   * Tests that if there is no system default destination, the default
   * selection rules and recent destinations are empty, and the preview
   * is in app kiosk mode (so no PDF printer), the first destination returned
   * from printer fetch is selected.
   */
  test(
      assert(destination_store_test.TestNames.KioskModeSelectsFirstPrinter),
      function() {
        initialSettings.serializedDefaultDestinationSelectionRulesStr = '';
        initialSettings.serializedAppStateStr = '';
        initialSettings.pdfPrinterDisabled = true;
        initialSettings.isDriveMounted = false;
        initialSettings.printerName = '';

        return setInitialSettings(false).then(function(args) {
          // Should have loaded the first destination as the selected printer.
          assertEquals(destinations[0].id, args.destinationId);
          assertEquals(PrinterType.LOCAL_PRINTER, args.printerType);
          assertEquals(
              destinations[0].id, destinationStore.selectedDestination.id);
        });
      });

  /**
   * Tests that if there is no system default destination, the default
   * selection rules and recent destinations are empty, the preview
   * is in app kiosk mode (so no PDF printer), and there are no
   * destinations found, the NO_DESTINATIONS error is fired and the selected
   * destination is null.
   */
  test(
      assert(destination_store_test.TestNames.NoPrintersShowsError),
      function() {
        initialSettings.serializedDefaultDestinationSelectionRulesStr = '';
        initialSettings.serializedAppStateStr = '';
        initialSettings.pdfPrinterDisabled = true;
        initialSettings.isDriveMounted = false;
        initialSettings.printerName = '';
        localDestinations = [];

        return Promise
            .all([
              setInitialSettings(true),
              eventToPromise(
                  DestinationStore.EventType.ERROR, destinationStore),
            ])
            .then(function(argsArray) {
              const errorEvent = argsArray[1];
              assertEquals(
                  DestinationErrorType.NO_DESTINATIONS, errorEvent.detail);
              assertEquals(null, destinationStore.selectedDestination);
            });
      });

  /**
   * Tests that if the user has a recent destination that triggers a cloud
   * print error this does not disable the dialog.
   */
  test(
      assert(destination_store_test.TestNames.UnreachableRecentCloudPrinter),
      function() {
        const cloudPrinter = createDestinationWithCertificateStatus(
            'BarDevice', 'BarName', false);
        const recentDestination = makeRecentDestination(cloudPrinter);
        initialSettings.serializedAppStateStr = JSON.stringify({
          version: 2,
          recentDestinations: [recentDestination],
        });
        userAccounts = ['foo@chromium.org'];

        return setInitialSettings(false).then(function(args) {
          assertEquals('FooDevice', args.destinationId);
          assertEquals(PrinterType.LOCAL_PRINTER, args.printerType);
          assertEquals('FooDevice', destinationStore.selectedDestination.id);
        });
      });

  /**
   * Tests that if the user has a recent destination that is already in the
   * store (PDF printer), the DestinationStore does not try to select a
   * printer again later. Regression test for https://crbug.com/927162.
   */
  test(assert(destination_store_test.TestNames.RecentSaveAsPdf), function() {
    const pdfPrinter = getSaveAsPdfDestination();
    const recentDestination = makeRecentDestination(pdfPrinter);
    initialSettings.serializedAppStateStr = JSON.stringify({
      version: 2,
      recentDestinations: [recentDestination],
    });

    return setInitialSettings(false)
        .then(function() {
          assertEquals(
              Destination.GooglePromotedId.SAVE_AS_PDF,
              destinationStore.selectedDestination.id);
          return new Promise(resolve => setTimeout(resolve));
        })
        .then(function() {
          // Should still have Save as PDF.
          assertEquals(
              Destination.GooglePromotedId.SAVE_AS_PDF,
              destinationStore.selectedDestination.id);
        });
  });

  /**
   * Tests that if there are recent destinations from different accounts, only
   * destinations associated with the most recent account are fetched.
   */
  test(
      assert(
          destination_store_test.TestNames.MultipleRecentDestinationsAccounts),
      function() {
        const account1 = 'foo@chromium.org';
        const account2 = 'bar@chromium.org';
        const driveUser1 = getGoogleDriveDestination(account1);
        const driveUser2 = getGoogleDriveDestination(account2);
        const cloudPrintFoo = new Destination(
            'FooCloud', DestinationType.GOOGLE, DestinationOrigin.COOKIES,
            'FooCloudName', DestinationConnectionStatus.ONLINE,
            {account: account1});
        const recentDestinations = [
          makeRecentDestination(driveUser1),
          makeRecentDestination(driveUser2),
          makeRecentDestination(cloudPrintFoo),
        ];
        cloudDestinations = [driveUser1, driveUser2, cloudPrintFoo];
        initialSettings.serializedAppStateStr = JSON.stringify({
          version: 2,
          recentDestinations: recentDestinations,
        });
        userAccounts = [account1, account2];

        const waitForPrinterDone = () => {
          return eventToPromise(
              CloudPrintInterfaceEventType.PRINTER_DONE,
              cloudPrintInterface.getEventTarget());
        };

        // Wait for the first cloud printer to be fetched for selection.
        return Promise
            .all([
              setInitialSettings(false),
              waitForPrinterDone(),
            ])
            .then(() => {
              // Should have loaded Google Drive as the selected printer, since
              // it was most recent.
              assertEquals(
                  Destination.GooglePromotedId.DOCS,
                  destinationStore.selectedDestination.id);

              // Since the system default is local, local destinations will also
              // have been loaded. Should have 5 local printers + 2 cloud
              // printers for account 1 + Save as PDF.
              const loadedPrintersAccount1 =
                  destinationStore.destinations(account1);
              assertEquals(8, loadedPrintersAccount1.length);
              cloudDestinations.forEach((destination) => {
                assertEquals(
                    destination.account === account1,
                    loadedPrintersAccount1.some(
                        p => p.key === destination.key));
              });
              assertEquals(1, numPrintersSelected);

              // 5 local + Save as PDF for account 2. Cloud printers for this
              // account won't be retrieved until
              // reloadUserCookieBasedDestinations() is called when the active
              // user changes.
              const loadedPrintersAccount2 =
                  destinationStore.destinations(account2);
              assertEquals(6, loadedPrintersAccount2.length);
              assertEquals(
                  Destination.GooglePromotedId.SAVE_AS_PDF,
                  loadedPrintersAccount2[0].id);
              loadedPrintersAccount2.forEach(printer => {
                assertFalse(printer.origin === DestinationOrigin.COOKIES);
              });
            });
      });

  /**
   * Tests that if the user has a single valid recent destination the
   * destination is automatically reselected.
   */
  test(
      assert(destination_store_test.TestNames.LoadAndSelectDestination),
      function() {
        destinations = getDestinations(localDestinations);
        initialSettings.printerName = '';
        const id1 = 'ID1';
        const name1 = 'One';
        let destination = null;

        return setInitialSettings(false)
            .then(function(args) {
              assertEquals(
                  Destination.GooglePromotedId.SAVE_AS_PDF, args.destinationId);
              assertEquals(PrinterType.PDF_PRINTER, args.printerType);
              assertEquals(
                  Destination.GooglePromotedId.SAVE_AS_PDF,
                  destinationStore.selectedDestination.id);
              const localDestinationInfo = {
                deviceName: id1,
                printerName: name1
              };
              // Typecast localDestinationInfo to work around the fact that
              // policy types are only defined on Chrome OS.
              nativeLayer.setLocalDestinationCapabilities({
                printer:
                    /** @type {!LocalDestinationInfo} */ (localDestinationInfo),
                capabilities: getCddTemplate(id1, name1).capabilities,
              });
              destinationStore.startLoadAllDestinations();
              return nativeLayer.whenCalled('getPrinters');
            })
            .then(() => {
              destination =
                  destinationStore.destinations().find(d => d.id === id1);
              // No capabilities or policies yet.
              assertFalse(!!destination.capabilities);
              destinationStore.selectDestination(destination);
              return nativeLayer.whenCalled('getPrinterCapabilities');
            })
            .then(() => {
              assertEquals(destination, destinationStore.selectedDestination);
              // Capabilities are updated.
              assertTrue(!!destination.capabilities);
            });
      });

  /**
   * Tests that if there are recent destinations from different accounts, only
   * destinations associated with the most recent account are fetched.
   */
  test(
      assert(destination_store_test.TestNames
                 .MultipleRecentDestinationsAccountsCros),
      function() {
        const account1 = 'foo@chromium.org';
        const account2 = 'bar@chromium.org';
        const cloudPrintFoo = new Destination(
            'FooCloud', DestinationType.GOOGLE, DestinationOrigin.COOKIES,
            'FooCloudName', DestinationConnectionStatus.ONLINE,
            {account: account1});
        const cloudPrintBar = new Destination(
            'BarCloud', DestinationType.GOOGLE, DestinationOrigin.COOKIES,
            'BarCloudName', DestinationConnectionStatus.ONLINE,
            {account: account1});
        const cloudPrintBaz = new Destination(
            'BazCloud', DestinationType.GOOGLE, DestinationOrigin.COOKIES,
            'BazCloudName', DestinationConnectionStatus.ONLINE,
            {account: account2});
        const recentDestinations = [
          makeRecentDestination(cloudPrintFoo),
          makeRecentDestination(cloudPrintBar),
          makeRecentDestination(cloudPrintBaz),
        ];
        cloudDestinations = [cloudPrintFoo, cloudPrintBar, cloudPrintBaz];
        initialSettings.serializedAppStateStr = JSON.stringify({
          version: 2,
          recentDestinations: recentDestinations,
        });
        userAccounts = [account1, account2];

        const waitForPrinterDone = () => {
          return eventToPromise(
              CloudPrintInterfaceEventType.PRINTER_DONE,
              cloudPrintInterface.getEventTarget());
        };

        // Wait for all three cloud printers to load.
        return Promise
            .all([
              setInitialSettings(false),
              waitForPrinterDone(),
            ])
            .then(() => {
              // Should have loaded FooCloud as the selected printer, since
              // it was most recent.
              assertEquals('FooCloud', destinationStore.selectedDestination.id);

              // Since the system default is local, local destinations will also
              // have been loaded. Should have 5 local printers + 2 cloud
              // printers for account 1 + Save as PDF + Drive.
              const loadedPrintersAccount1 =
                  destinationStore.destinations(account1);
              assertEquals(9, loadedPrintersAccount1.length);
              cloudDestinations.forEach((destination) => {
                assertEquals(
                    destination.account === account1,
                    loadedPrintersAccount1.some(
                        p => p.key === destination.key));
              });
              assertEquals(1, numPrintersSelected);

              // 5 local, Save as PDF, and Save to Drive exist
              // when filtering for account 2 because its cloud printers are not
              // requested at startup.
              const loadedPrintersAccount2 =
                  destinationStore.destinations(account2);
              assertEquals(7, loadedPrintersAccount2.length);
              assertEquals(
                  Destination.GooglePromotedId.SAVE_AS_PDF,
                  loadedPrintersAccount2[0].id);
            });
      });

  /** Tests that the SAVE_TO_DRIVE_CROS destination is loaded on Chrome OS. */
  test(
      assert(destination_store_test.TestNames.LoadSaveToDriveCros), function() {
        return setInitialSettings(false).then(function(args) {
          assertTrue(!!destinationStore.destinations().find(
              destination => destination.id ===
                  Destination.GooglePromotedId.SAVE_TO_DRIVE_CROS));
        });
      });

  // Tests that the SAVE_TO_DRIVE_CROS destination is not loaded on Chrome OS
  // when Google Drive is not mounted.
  test(assert(destination_store_test.TestNames.DriveNotMounted), function() {
    initialSettings.isDriveMounted = false;
    return setInitialSettings(false).then(function(args) {
      assertFalse(!!destinationStore.destinations().find(
          destination => destination.id ===
              Destination.GooglePromotedId.SAVE_TO_DRIVE_CROS));
    });
  });
});
