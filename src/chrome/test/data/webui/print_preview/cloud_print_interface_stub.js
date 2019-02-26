// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('print_preview', function() {
  /**
   * Test version of the cloud print interface.
   * @implements {cloudprint.CloudPrintInterface}
   */
  class CloudPrintInterfaceStub {
    constructor() {
      /** @private {!cr.EventTarget} */
      this.eventTarget_ = new cr.EventTarget();

      /** @private {boolean} */
      this.searchInProgress_ = false;

      /** @private {!Map<string, !print_preview.Destination>} */
      this.cloudPrintersMap_ = new Map();
    }

    /** @override */
    getEventTarget() {
      return this.eventTarget_;
    }

    /** @override */
    isCloudDestinationSearchInProgress() {
      return this.searchInProgress_;
    }

    /**
     * @param {string} id The ID of the printer.
     * @param {!print_preview.Destination} printer The destination to return
     *     when the printer is requested.
     */
    setPrinter(id, printer) {
      this.cloudPrintersMap_.set(id, printer);
    }

    /**
     * Dispatches a CloudPrintInterfaceEventType.SEARCH_DONE event with the
     * printers that have been set so far using setPrinter().
     * @override
     */
    search() {
      this.searchInProgress_ = true;
      const searchDoneEvent =
          new Event(cloudprint.CloudPrintInterfaceEventType.SEARCH_DONE);
      searchDoneEvent.origin = print_preview.DestinationOrigin.COOKIES;
      searchDoneEvent.printers = [];
      this.cloudPrintersMap_.forEach((value) => {
        searchDoneEvent.printers.push(value);
      });
      searchDoneEvent.isRecent = true;
      searchDoneEvent.user = 'foo@chromium.org';
      searchDoneEvent.searchDone = true;
      this.searchInProgress_ = false;
      this.eventTarget_.dispatchEvent(searchDoneEvent);
    }

    /** @override */
    invites(account) {}

    /**
     * Dispatches a CloudPrintInterfaceEventType.PRINTER_DONE event with the
     * printer details if the printer has been added by calling setPrinter().
     * @override
     */
    printer(printerId, origin, account) {
      const printer = this.cloudPrintersMap_.get(printerId);
      if (!!printer) {
        const printerDoneEvent =
            new Event(cloudprint.CloudPrintInterfaceEventType.PRINTER_DONE);
        printerDoneEvent.printer = printer;
        printerDoneEvent.printer.capabilities =
            print_preview_test_utils.getCddTemplate(printerId);
        this.eventTarget_.dispatchEvent(printerDoneEvent);
      }
    }
  }

  return {
    CloudPrintInterfaceStub: CloudPrintInterfaceStub,
  };
});
