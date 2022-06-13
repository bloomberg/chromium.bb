// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Destination, DestinationConnectionStatus, DestinationOrigin, DestinationType, NativeLayerCrosImpl, PrinterStatusReason, PrinterStatusSeverity, PrintPreviewDestinationListItemElement} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {waitBeforeNextRender} from 'chrome://webui-test/test_util.js';

import {NativeLayerCrosStub} from './native_layer_cros_stub.js';

const destination_item_test_cros = {
  suiteName: 'DestinationItemTestCros',
  TestNames: {
    NewStatusUpdatesIcon: 'new status updates icon',
    ChangingDestinationUpdatesIcon: 'changing destination updates icon',
    OnlyUpdateMatchingDestination: 'only update matching destination',
  },
};

Object.assign(window, {destination_item_test_cros: destination_item_test_cros});

suite(destination_item_test_cros.suiteName, function() {
  let listItem: PrintPreviewDestinationListItemElement;

  let nativeLayerCros: NativeLayerCrosStub;

  function setNativeLayerPrinterStatusMap() {
    [{
      printerId: 'One',
      statusReasons: [{
        reason: PrinterStatusReason.NO_ERROR,
        severity: PrinterStatusSeverity.UNKNOWN_SEVERITY,
      }],
      timestamp: 0,
    },
     {
       printerId: 'Two',
       statusReasons: [{
         reason: PrinterStatusReason.OUT_OF_INK,
         severity: PrinterStatusSeverity.ERROR,
       }],
       timestamp: 0,
     }]
        .forEach(
            status => nativeLayerCros.addPrinterStatusToMap(
                status.printerId, status));
  }

  setup(function() {
    document.body.innerHTML = `
          <print-preview-destination-list-item id="listItem">
          </print-preview-destination-list-item>`;

    // Stub out native layer.
    nativeLayerCros = new NativeLayerCrosStub();
    NativeLayerCrosImpl.setInstance(nativeLayerCros);
    setNativeLayerPrinterStatusMap();

    listItem =
        document.body.querySelector<PrintPreviewDestinationListItemElement>(
            '#listItem')!;
    listItem.destination = new Destination(
        'One', DestinationType.LOCAL, DestinationOrigin.CROS, 'Destination One',
        DestinationConnectionStatus.ONLINE, {description: 'ABC'});
    flush();
  });

  test(
      assert(destination_item_test_cros.TestNames.NewStatusUpdatesIcon),
      function() {
        const icon = listItem.shadowRoot!.querySelector('iron-icon')!;
        assertEquals('print-preview:printer-status-grey', icon.icon);

        return listItem.destination.requestPrinterStatus().then(() => {
          assertEquals('print-preview:printer-status-green', icon.icon);
        });
      });

  test(
      assert(
          destination_item_test_cros.TestNames.ChangingDestinationUpdatesIcon),
      function() {
        const icon = listItem.shadowRoot!.querySelector('iron-icon')!;
        assertEquals('print-preview:printer-status-grey', icon.icon);

        listItem.destination = new Destination(
            'Two', DestinationType.LOCAL, DestinationOrigin.CROS,
            'Destination Two', DestinationConnectionStatus.ONLINE,
            {description: 'ABC'});

        return waitBeforeNextRender(listItem).then(() => {
          assertEquals('print-preview:printer-status-red', icon.icon);
        });
      });

  // Tests that the printer stauts icon is only notified to update if the
  // destination key in the printer status response matches the current
  // destination.
  test(
      assert(
          destination_item_test_cros.TestNames.OnlyUpdateMatchingDestination),
      function() {
        const icon = listItem.shadowRoot!.querySelector('iron-icon')!;
        assertEquals('print-preview:printer-status-grey', icon.icon);
        const firstDestinationStatusRequestPromise =
            listItem.destination.requestPrinterStatus();

        // Simulate destination_list updating and switching the destination
        // after the request for the original destination was already sent out.
        listItem.destination = new Destination(
            'Two', DestinationType.LOCAL, DestinationOrigin.CROS,
            'Destination Two', DestinationConnectionStatus.ONLINE,
            {description: 'ABC'});

        return firstDestinationStatusRequestPromise.then(() => {
          // PrinterState should stay the same because the destination in the
          // status request response doesn't match.
          assertEquals('print-preview:printer-status-grey', icon.icon);
        });
      });
});
