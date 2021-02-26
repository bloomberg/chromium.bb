// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {TestBrowserProxy} from '../../test_browser_proxy.m.js';
// #import {CupsPrintersBrowserProxy,PrinterSetupResult,PrintServerResult} from 'chrome://os-settings/chromeos/lazy_load.js';
// clang-format on

cr.define('printerBrowserProxy', function() {
  /** @implements {settings.CupsPrintersBrowserProxy} */
  /* #export */ class TestCupsPrintersBrowserProxy extends TestBrowserProxy {
    constructor() {
      super([
        'addCupsPrinter',
        'addDiscoveredPrinter',
        'getCupsPrintersList',
        'getCupsPrinterManufacturersList',
        'getCupsPrinterModelsList',
        'getPrinterInfo',
        'getPrinterPpdManufacturerAndModel',
        'queryPrintServer',
        'startDiscoveringPrinters',
        'stopDiscoveringPrinters',
        'cancelPrinterSetUp',
        'updateCupsPrinter',
        'removeCupsPrinter',
        'reconfigureCupsPrinter',
        'getEulaUrl',
      ]);

      this.printerList = /** @type{?CupsPrintersList} */ ({printerList: []});
      this.printServerPrinters =
          /** @type{?CupsPrintersList}  */ ({printerList: []});
      this.manufacturers =
          /** @type{?ManufacturersInfo} */ (
              {success: false, manufacturers: []});
      this.models =
          /** @type{?ModelsInfo} */ ({success: false, models: []});
      this.printerInfo = {};
      this.printerPpdMakeModel =
          /** @type{PrinterPpdMakeModel */ (
              {ppdManufacturer: '', ppdModel: ''});

      /**
       * |eulaUrl_| in conjunction with |setEulaUrl| mimics setting the EULA url
       * for a printer.
       * @private {string}
       */
      this.eulaUrl_ = '';

      /**
       * If set, 'getPrinterInfo' will fail and the promise will be reject with
       * this PrinterSetupResult.
       * @private {PrinterSetupResult}
       */
      this.getPrinterInfoResult_ = null;

      /**
       * Contains the result code from querying a print server.
       * @private {PrintServerResult}
       */
      this.queryPrintServerResult_ = null;

      /**
       * If set, 'addDiscoveredPrinter' will fail and the promise will be
       * rejected with this printer.
       * @private {CupsPrinterInfo}
       */
      this.addDiscoveredFailedPrinter_ = null;
    }

    /** @override */
    addCupsPrinter(newPrinter) {
      this.methodCalled('addCupsPrinter', newPrinter);
      return Promise.resolve(PrinterSetupResult.SUCCESS);
    }

    /** @override */
    addDiscoveredPrinter(printerId) {
      this.methodCalled('addDiscoveredPrinter', printerId);
      if (this.addDiscoveredFailedPrinter_ != null) {
        return Promise.reject(this.addDiscoveredFailedPrinter_);
      }
      return Promise.resolve(PrinterSetupResult.SUCCESS);
    }

    /** @override */
    getCupsPrintersList() {
      this.methodCalled('getCupsPrintersList');
      return Promise.resolve(this.printerList);
    }

    /** @override */
    getCupsPrinterManufacturersList() {
      this.methodCalled('getCupsPrinterManufacturersList');
      return Promise.resolve(this.manufacturers);
    }

    /** @override */
    getCupsPrinterModelsList(manufacturer) {
      this.methodCalled('getCupsPrinterModelsList', manufacturer);
      return Promise.resolve(this.models);
    }

    /** @override */
    getPrinterInfo(newPrinter) {
      this.methodCalled('getPrinterInfo', newPrinter);
      if (this.getPrinterInfoResult_ != null) {
        return Promise.reject(this.getPrinterInfoResult_);
      }
      return Promise.resolve(this.printerInfo);
    }

    /** @override */
    startDiscoveringPrinters() {
      this.methodCalled('startDiscoveringPrinters');
    }

    /** @override */
    stopDiscoveringPrinters() {
      this.methodCalled('stopDiscoveringPrinters');
    }

    /** @override */
    cancelPrinterSetUp(newPrinter) {
      this.methodCalled('cancelPrinterSetUp', newPrinter);
    }

    /** @override */
    updateCupsPrinter(printerId, printerName) {
      this.methodCalled('updateCupsPrinter', [printerId, printerName]);
      return Promise.resolve(PrinterSetupResult.EDIT_SUCCESS);
    }

    /** @override */
    removeCupsPrinter(printerId, printerName) {
      this.methodCalled('removeCupsPrinter', [printerId, printerName]);
    }

    /** @override */
    getPrinterPpdManufacturerAndModel(printerId) {
      this.methodCalled('getPrinterPpdManufacturerAndModel', printerId);
      return Promise.resolve(this.printerPpdMakeModel);
    }

    /** @override */
    reconfigureCupsPrinter(printer) {
      this.methodCalled('reconfigureCupsPrinter', printer);
      return Promise.resolve(PrinterSetupResult.EDIT_SUCCESS);
    }

    /** @override */
    getEulaUrl(ppdManufacturer, ppdModel) {
      this.methodCalled('getEulaUrl', [ppdManufacturer, ppdModel]);
      return Promise.resolve(this.eulaUrl_);
    }

    /** @override */
    queryPrintServer(serverUrl) {
      this.methodCalled('queryPrintServer', serverUrl);
      if (this.queryPrintServerResult_ !== PrintServerResult.NO_ERRORS) {
        return Promise.reject(this.queryPrintServerResult_);
      }
      return Promise.resolve(this.printServerPrinters);
    }


    /** @param {string} eulaUrl */
    setEulaUrl(eulaUrl) {
      this.eulaUrl_ = eulaUrl;
    }

    /** @param {PrinterSetupResult} result */
    setGetPrinterInfoResult(result) {
      this.getPrinterInfoResult_ = result;
    }

    /** @param {PrintServerResult} result */
    setQueryPrintServerResult(result) {
      this.queryPrintServerResult_ = result;
    }

    /** @param {!CupsPrinterInfo} printer */
    setAddDiscoveredPrinterFailure(printer) {
      this.addDiscoveredFailedPrinter_ = printer;
    }
  }
  // #cr_define_end
  return {
    TestCupsPrintersBrowserProxy: TestCupsPrintersBrowserProxy,
  };
});