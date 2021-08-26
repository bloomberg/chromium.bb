// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationConnectionStatus, DestinationOrigin, DestinationType, NativeLayer, NativeLayerCros, NativeLayerCrosImpl, NativeLayerImpl, PrinterStatus, PrinterStatusReason, PrinterStatusSeverity, PrintPreviewDestinationDropdownCrosElement, PrintPreviewDestinationSelectCrosElement, SAVE_TO_DRIVE_CROS_DESTINATION_KEY} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {Base, flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals, assertFalse, assertTrue} from '../chai_assert.js';
import {waitBeforeNextRender} from '../test_util.m.js';

import {NativeLayerCrosStub} from './native_layer_cros_stub.js';
import {NativeLayerStub} from './native_layer_stub.js';
import {getGoogleDriveDestination, getSaveAsPdfDestination} from './print_preview_test_utils.js';

window.printer_status_test_cros = {};
const printer_status_test_cros = window.printer_status_test_cros;
printer_status_test_cros.suiteName = 'PrinterStatusTestCros';
/** @enum {string} */
printer_status_test_cros.TestNames = {
  PrinterStatusUpdatesColor: 'printer status updates color',
  SendStatusRequestOnce: 'send status request once',
  HiddenStatusText: 'hidden status text',
  ChangeIcon: 'change icon',
  SuccessfulPrinterStatusAfterRetry: 'successful printer status after retry',
};

suite(printer_status_test_cros.suiteName, function() {
  /** @type {!PrintPreviewDestinationSelectCrosElement} */
  let destinationSelect;

  const account = 'foo@chromium.org';

  /** @type {?NativeLayerCrosStub} */
  let nativeLayerCros = null;

  function setNativeLayerPrinterStatusMap() {
    [{
      printerId: 'ID1',
      statusReasons: [],
    },
     {
       printerId: 'ID2',
       statusReasons: [{
         reason: PrinterStatusReason.LOW_ON_PAPER,
         severity: PrinterStatusSeverity.UNKNOWN_SEVERITY
       }],
     },
     {
       printerId: 'ID3',
       statusReasons: [{
         reason: PrinterStatusReason.LOW_ON_PAPER,
         severity: PrinterStatusSeverity.REPORT
       }],
     },
     {
       printerId: 'ID4',
       statusReasons: [{
         reason: PrinterStatusReason.LOW_ON_PAPER,
         severity: PrinterStatusSeverity.WARNING
       }],
     },
     {
       printerId: 'ID5',
       statusReasons: [{
         reason: PrinterStatusReason.LOW_ON_PAPER,
         severity: PrinterStatusSeverity.ERROR
       }],
     },
     {
       printerId: 'ID6',
       statusReasons: [
         {
           reason: PrinterStatusReason.DEVICE_ERROR,
           severity: PrinterStatusSeverity.UNKNOWN_SEVERITY
         },
         {
           reason: PrinterStatusReason.PRINTER_QUEUE_FULL,
           severity: PrinterStatusSeverity.ERROR
         }
       ],
     },
     {
       printerId: 'ID7',
       statusReasons: [
         {
           reason: PrinterStatusReason.DEVICE_ERROR,
           severity: PrinterStatusSeverity.REPORT
         },
         {
           reason: PrinterStatusReason.PRINTER_QUEUE_FULL,
           severity: PrinterStatusSeverity.UNKNOWN_SEVERITY
         }
       ],
     },
     {
       printerId: 'ID8',
       statusReasons: [{
         reason: PrinterStatusReason.UNKNOWN_REASON,
         severity: PrinterStatusSeverity.ERROR
       }],
     },
     {
       printerId: 'ID9',
       statusReasons: [{
         reason: PrinterStatusReason.UNKNOWN_REASON,
         severity: PrinterStatusSeverity.UNKNOWN_SEVERITY
       }],
     },
     {
       printerId: 'ID10',
       statusReasons: [{
         reason: PrinterStatusReason.PRINTER_UNREACHABLE,
         severity: PrinterStatusSeverity.ERROR
       }],
     }]
        .forEach(
            status => nativeLayerCros.addPrinterStatusToMap(
                status.printerId, status));
  }

  /**
   * @param {string} id
   * @param {string} displayName
   * @param {!DestinationOrigin} destinationOrigin
   * @return {!Destination}
   */
  function createDestination(id, displayName, destinationOrigin) {
    return new Destination(
        id, DestinationType.LOCAL, destinationOrigin, displayName,
        DestinationConnectionStatus.ONLINE);
  }

  /**
   * @param {string} value
   * @return {string}
   */
  function escapeForwardSlahes(value) {
    return value.replace(/\//g, '\\/');
  }

  /**
   * @param {!PrintPreviewDestinationDropdownCrosElement} dropdown
   * @param {string} key
   * @return {string}
   */
  function getIconString(dropdown, key) {
    return dropdown.shadowRoot.querySelector(`#${escapeForwardSlahes(key)}`)
        .firstChild.icon;
  }

  setup(function() {
    document.body.innerHTML = '';

    // Stub out native layer.
    NativeLayerImpl.instance_ = new NativeLayerStub();
    nativeLayerCros = new NativeLayerCrosStub();
    NativeLayerCrosImpl.instance_ = nativeLayerCros;
    setNativeLayerPrinterStatusMap();

    destinationSelect =
        /** @type {!PrintPreviewDestinationSelectCrosElement} */
        (document.createElement('print-preview-destination-select-cros'));
    document.body.appendChild(destinationSelect);
  });

  test(
      assert(printer_status_test_cros.TestNames.PrinterStatusUpdatesColor),
      function() {
        const destination1 =
            createDestination('ID1', 'One', DestinationOrigin.CROS);
        const destination2 =
            createDestination('ID2', 'Two', DestinationOrigin.CROS);
        const destination3 =
            createDestination('ID3', 'Three', DestinationOrigin.CROS);
        const destination4 =
            createDestination('ID4', 'Four', DestinationOrigin.CROS);
        const destination5 =
            createDestination('ID5', 'Five', DestinationOrigin.CROS);
        const destination6 =
            createDestination('ID6', 'Six', DestinationOrigin.CROS);
        const destination7 =
            createDestination('ID7', 'Seven', DestinationOrigin.CROS);
        const destination8 =
            createDestination('ID8', 'Eight', DestinationOrigin.CROS);
        const destination9 =
            createDestination('ID9', 'Nine', DestinationOrigin.CROS);

        return waitBeforeNextRender(destinationSelect)
            .then(() => {
              const whenStatusRequestsDone =
                  nativeLayerCros.waitForMultiplePrinterStatusRequests(7);

              destinationSelect.recentDestinationList = [
                destination1,
                destination2,
                destination3,
                destination4,
                destination5,
                destination6,
                destination7,
                destination8,
                destination9,
              ];

              return whenStatusRequestsDone;
            })
            .then(() => {
              return waitBeforeNextRender(destinationSelect);
            })
            .then(() => {
              const dropdown =
                  /** @type {!PrintPreviewDestinationDropdownCrosElement} */ (
                      destinationSelect.shadowRoot.querySelector('#dropdown'));

              // Empty printer status.
              assertEquals(
                  'print-preview:printer-status-green',
                  getIconString(dropdown, destination1.key));

              // Error printer status with unknown severity.
              assertEquals(
                  'print-preview:printer-status-green',
                  getIconString(dropdown, destination2.key));

              // Error printer status with report severity.
              assertEquals(
                  'print-preview:printer-status-green',
                  getIconString(dropdown, destination3.key));

              // Error printer status with warning severity.
              assertEquals(
                  'print-preview:printer-status-red',
                  getIconString(dropdown, destination4.key));

              // Error printer status with error severity.
              assertEquals(
                  'print-preview:printer-status-red',
                  getIconString(dropdown, destination5.key));

              // Error printer status with unknown severity + error printer
              // status with error severity.
              assertEquals(
                  'print-preview:printer-status-red',
                  getIconString(dropdown, destination6.key));

              // Error printer status with unknown severity + error printer
              // status with report severity.
              assertEquals(
                  'print-preview:printer-status-green',
                  getIconString(dropdown, destination7.key));

              // Unknown reason printer status with error severity.
              assertEquals(
                  'print-preview:printer-status-grey',
                  getIconString(dropdown, destination8.key));

              // Unknown reason printer status with unknown severity.
              assertEquals(
                  'print-preview:printer-status-green',
                  getIconString(dropdown, destination9.key));
            });
        });

  test(
      assert(printer_status_test_cros.TestNames.SendStatusRequestOnce),
      function() {
        return waitBeforeNextRender(destinationSelect).then(() => {
          const destination1 =
              createDestination('ID1', 'One', DestinationOrigin.CROS);
          const destination2 =
              createDestination('ID2', 'Two', DestinationOrigin.CROS);

          destinationSelect.recentDestinationList = [
            destination1,
            destination2,
            createDestination('ID3', 'Three', DestinationOrigin.EXTENSION),
            createDestination('ID4', 'Four', DestinationOrigin.EXTENSION),
          ];
          assertEquals(
              2, nativeLayerCros.getCallCount('requestPrinterStatusUpdate'));

          // Update list with 2 existing destinations and one new destination.
          // Make sure the requestPrinterStatusUpdate only gets called for the
          // new destination.
          destinationSelect.recentDestinationList = [
            destination1,
            destination2,
            createDestination('ID5', 'Five', DestinationOrigin.CROS),
          ];
          assertEquals(
              3, nativeLayerCros.getCallCount('requestPrinterStatusUpdate'));
        });
      });

  test(assert(printer_status_test_cros.TestNames.HiddenStatusText), function() {
    const destinationStatus = destinationSelect.shadowRoot.querySelector(
        '.destination-additional-info');
    return waitBeforeNextRender(destinationSelect)
        .then(() => {
          const destinationWithoutErrorStatus =
              createDestination('ID1', 'One', DestinationOrigin.CROS);
          // Destination with ID4 will return an error printer status that will
          // trigger the error text being populated.
          const destinationWithErrorStatus =
              createDestination('ID4', 'Four', DestinationOrigin.CROS);
          const cloudPrintDestination = new Destination(
              'ID2', DestinationType.GOOGLE, DestinationOrigin.COOKIES, 'Two',
              DestinationConnectionStatus.OFFLINE, {account: account});

          destinationSelect.recentDestinationList = [
            destinationWithoutErrorStatus,
            destinationWithErrorStatus,
            cloudPrintDestination,
          ];

          const destinationEulaWrapper =
              destinationSelect.shadowRoot.querySelector(
                  '#destinationEulaWrapper');

          destinationSelect.destination = cloudPrintDestination;
          assertFalse(destinationStatus.hidden);
          assertTrue(destinationEulaWrapper.hidden);

          destinationSelect.destination = destinationWithoutErrorStatus;
          assertTrue(destinationStatus.hidden);
          assertTrue(destinationEulaWrapper.hidden);

          destinationSelect.set(
              'destination.eulaUrl', 'chrome://os-credits/eula');
          assertFalse(destinationEulaWrapper.hidden);

          destinationSelect.destination = destinationWithErrorStatus;
          return nativeLayerCros.whenCalled('requestPrinterStatusUpdate');
        })
        .then(() => {
          return waitBeforeNextRender(destinationSelect);
        })
        .then(() => {
          assertFalse(destinationStatus.hidden);
        });
  });

  test(assert(printer_status_test_cros.TestNames.ChangeIcon), function() {
    return waitBeforeNextRender(destinationSelect).then(() => {
      const localCrosPrinter =
          createDestination('ID1', 'One', DestinationOrigin.CROS);
      const localNonCrosPrinter =
          createDestination('ID2', 'Two', DestinationOrigin.LOCAL);
      const cloudPrintDestination = new Destination(
          'ID3', DestinationType.GOOGLE, DestinationOrigin.COOKIES, 'Three',
          DestinationConnectionStatus.ONLINE, {account: account});
      const ownedCloudPrintDestination = new Destination(
          'ID4', DestinationType.GOOGLE, DestinationOrigin.COOKIES, 'Four',
          DestinationConnectionStatus.ONLINE,
          {account: account, isOwned: true});
      const crosEnterprisePrinter = new Destination(
          'ID5', DestinationType.LOCAL, DestinationOrigin.CROS, 'Five',
          DestinationConnectionStatus.ONLINE, {isEnterprisePrinter: true});
      const mobilePrinter = new Destination(
          'ID7', DestinationType.MOBILE, DestinationOrigin.COOKIES, 'Seven',
          DestinationConnectionStatus.ONLINE);
      const saveToDrive = getGoogleDriveDestination('account');
      const saveAsPdf = getSaveAsPdfDestination();

      destinationSelect.recentDestinationList = [
        localCrosPrinter,
        saveToDrive,
        saveAsPdf,
      ];
      const dropdown = destinationSelect.shadowRoot.querySelector('#dropdown');

      destinationSelect.destination = localCrosPrinter;
      destinationSelect.updateDestination();
      assertEquals(
          'print-preview:printer-status-grey', dropdown.destinationIcon);

      destinationSelect.destination = localNonCrosPrinter;
      destinationSelect.updateDestination();
      assertEquals('print-preview:print', dropdown.destinationIcon);

      destinationSelect.destination = cloudPrintDestination;
      destinationSelect.updateDestination();
      assertEquals('print-preview:printer-shared', dropdown.destinationIcon);

      destinationSelect.destination = ownedCloudPrintDestination;
      destinationSelect.updateDestination();
      assertEquals('print-preview:print', dropdown.destinationIcon);

      destinationSelect.destination = crosEnterprisePrinter;
      destinationSelect.updateDestination();
      assertEquals(
          'print-preview:business-printer-status-grey',
          dropdown.destinationIcon);

      destinationSelect.destination = mobilePrinter;
      destinationSelect.updateDestination();
      assertEquals('print-preview:smartphone', dropdown.destinationIcon);

      destinationSelect.destination = saveToDrive;
      destinationSelect.updateDestination();
      assertEquals('print-preview:save-to-drive', dropdown.destinationIcon);

      destinationSelect.destination = saveAsPdf;
      destinationSelect.updateDestination();
      assertEquals('cr:insert-drive-file', dropdown.destinationIcon);
    });
  });

  test(
      assert(
          printer_status_test_cros.TestNames.SuccessfulPrinterStatusAfterRetry),
      function() {
        nativeLayerCros.simulateStatusRetrySuccesful();

        const destination =
            createDestination('ID10', 'Ten', DestinationOrigin.CROS);
        destination.setPrinterStatusRetryTimeoutForTesting(100);
        const whenStatusRequestsDonePromise =
            nativeLayerCros.waitForMultiplePrinterStatusRequests(2);
        destinationSelect.recentDestinationList = [
          destination,
        ];

        const dropdown =
            /** @type {!PrintPreviewDestinationDropdownCrosElement} */ (
                destinationSelect.shadowRoot.querySelector('#dropdown'));
        return whenStatusRequestsDonePromise
            .then(() => {
              assertEquals(
                  'print-preview:printer-status-grey',
                  getIconString(dropdown, destination.key));
              assertEquals(
                  0,
                  nativeLayerCros.getCallCount(
                      'recordPrinterStatusRetrySuccessHistogram'));
              return waitBeforeNextRender(destinationSelect);
            })
            .then(() => {
              // The printer status is requested twice because of the retry.
              assertEquals(
                  2,
                  nativeLayerCros.getCallCount('requestPrinterStatusUpdate'));
              assertEquals(
                  'print-preview:printer-status-green',
                  getIconString(dropdown, destination.key));
              assertEquals(
                  1,
                  nativeLayerCros.getCallCount(
                      'recordPrinterStatusRetrySuccessHistogram'));
              assertEquals(
                  true,
                  nativeLayerCros.getArgs(
                      'recordPrinterStatusRetrySuccessHistogram')[0]);
            });
      });
});
