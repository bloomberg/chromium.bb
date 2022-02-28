// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

/**
 * @fileoverview A helper object used from the "CUPS printing" section to
 * interact with the browser. Used only on Chrome OS.
 */

/**
 * @typedef {{
 *   isManaged: boolean,
 *   ppdManufacturer: string,
 *   ppdModel: string,
 *   printerAddress: string,
 *   printerDescription: string,
 *   printerId: string,
 *   printerMakeAndModel: string,
 *   printerName: string,
 *   printerPPDPath: string,
 *   printerPpdReference: {
 *     userSuppliedPpdUrl: string,
 *     effectiveMakeAndModel: string,
 *     autoconf: boolean,
 *   },
 *   printerProtocol: string,
 *   printerQueue: string,
 *   printerStatus: string,
 *   printServerUri: string,
 * }}
 *
 * Note: |printerPPDPath| refers to a PPD retrieved from the user at the
 * add-printer-manufacturer-model-dialog. |printerPpdReference| refers to either
 * information retrieved from the printer or resolved via ppd_provider.
 */
export let CupsPrinterInfo;

/**
 * @typedef {{
 *   printerList: !Array<!CupsPrinterInfo>,
 * }}
 */
export let CupsPrintersList;

/**
 * @typedef {{
 *   success: boolean,
 *   manufacturers: Array<string>
 * }}
 */
export let ManufacturersInfo;

/**
 * @typedef {{
 *   success: boolean,
 *   models: Array<string>
 * }}
 */
export let ModelsInfo;

/**
 * @typedef {{
 *   makeAndModel: string,
 *   autoconf: boolean,
 *   ppdRefUserSuppliedPpdUrl: string,
 *   ppdRefEffectiveMakeAndModel: string,
 *   ppdReferenceResolved: boolean
 * }}
 */
export let PrinterMakeModel;

/**
 * @typedef {{
 *   ppdManufacturer: string,
 *   ppdModel: string
 * }}
 */
export let PrinterPpdMakeModel;

/**
 *  @enum {number}
 *  These values must be kept in sync with the PrinterSetupResult enum in
 *  chrome/browser/chromeos/printing/printer_configurer.h.
 */
export const PrinterSetupResult = {
  FATAL_ERROR: 0,
  SUCCESS: 1,
  PRINTER_UNREACHABLE: 2,
  DBUS_ERROR: 3,
  NATIVE_PRINTERS_NOT_ALLOWED: 4,
  INVALID_PRINTER_UPDATE: 5,
  COMPONENT_UNAVAILAVLE: 6,
  EDIT_SUCCESS: 7,
  PPD_TOO_LARGE: 10,
  INVALID_PPD: 11,
  PPD_NOT_FOUND: 12,
  PPD_UNRETRIEVABLE: 13,
  DBUS_NO_REPLY: 64,
  DBUS_TIMEOUT: 65,
};

/**
 *  @enum {number}
 *  These values must be kept in sync with the PrintServerQueryResult enum in
 *  /chrome/browser/ash/printing/server_printers_fetcher.h
 */
export const PrintServerResult = {
  NO_ERRORS: 0,
  INCORRECT_URL: 1,
  CONNECTION_ERROR: 2,
  HTTP_ERROR: 3,
  CANNOT_PARSE_IPP_RESPONSE: 4,
};

/**
 * @typedef {{
 *   message: string
 * }}
 */
let QueryFailure;

  /** @interface */
export class CupsPrintersBrowserProxy {
  /**
   * @return {!Promise<!CupsPrintersList>}
   */
  getCupsSavedPrintersList() {}

  /**
   * @return {!Promise<!CupsPrintersList>}
   */
  getCupsEnterprisePrintersList() {}

  /**
   * @param {string} printerId
   * @param {string} printerName
   * @return {!Promise<!PrinterSetupResult>}
   */
  updateCupsPrinter(printerId, printerName) {}

  /**
   * @param {string} printerId
   * @param {string} printerName
   */
  removeCupsPrinter(printerId, printerName) {}

  /**
   * @return {!Promise<string>} The full path of the printer PPD file.
   */
  getCupsPrinterPPDPath() {}

  /**
   * @param {!CupsPrinterInfo} newPrinter
   * @return {!Promise<!PrinterSetupResult>}
   */
  addCupsPrinter(newPrinter) {}

  /**
   * @param {!CupsPrinterInfo} printer
   * @return {!Promise<!PrinterSetupResult>}
   */
  reconfigureCupsPrinter(printer) {}

  startDiscoveringPrinters() {}
  stopDiscoveringPrinters() {}

  /**
   * @return {!Promise<!ManufacturersInfo>}
   */
  getCupsPrinterManufacturersList() {}

  /**
   * @param {string} manufacturer
   * @return {!Promise<!ModelsInfo>}
   */
  getCupsPrinterModelsList(manufacturer) {}

  /**
   * @param {!CupsPrinterInfo} newPrinter
   * @return {!Promise<!PrinterMakeModel>}
   */
  getPrinterInfo(newPrinter) {}

  /**
   * @param {string} printerId
   * @return {!Promise<!PrinterPpdMakeModel>}
   */
  getPrinterPpdManufacturerAndModel(printerId) {}

  /**
   * @param{string} printerId
   * @return {!Promise<!PrinterSetupResult>}
   */
  addDiscoveredPrinter(printerId) {}

  /**
   * Report to the handler that setup was cancelled.
   * @param {!CupsPrinterInfo} newPrinter
   */
  cancelPrinterSetUp(newPrinter) {}

  /**
   * @param {string} ppdManufacturer
   * @param {string} ppdModel
   * @return {!Promise<string>} Returns the EULA URL of the printer. Returns
   * an empty string if no EULA is required.
   */
  getEulaUrl(ppdManufacturer, ppdModel) {}

  /**
   * Attempts to query the |serverUrl| and retrieve printers from the url.
   * @param {string} serverUrl
   * @return {!Promise<!CupsPrintersList>}
   */
  queryPrintServer(serverUrl) {}

  /**
   * Opens the print management app in its own window.
   */
  openPrintManagementApp() {}

  /**
   * Opens the Scanning app in its own window.
   */
  openScanningApp() {}
}

/**
 * @implements {CupsPrintersBrowserProxy}
 */
export class CupsPrintersBrowserProxyImpl {
  /** @override */
  getCupsSavedPrintersList() {
    return sendWithPromise('getCupsSavedPrintersList');
  }

  /** @override */
  getCupsEnterprisePrintersList() {
    return sendWithPromise('getCupsEnterprisePrintersList');
  }

  /** @override */
  updateCupsPrinter(printerId, printerName) {
    return sendWithPromise('updateCupsPrinter', printerId, printerName);
  }

  /** @override */
  removeCupsPrinter(printerId, printerName) {
    chrome.send('removeCupsPrinter', [printerId, printerName]);
  }

  /** @override */
  addCupsPrinter(newPrinter) {
    return sendWithPromise('addCupsPrinter', newPrinter);
  }

  /** @override */
  reconfigureCupsPrinter(printer) {
    return sendWithPromise('reconfigureCupsPrinter', printer);
  }

  /** @override */
  getCupsPrinterPPDPath() {
    return sendWithPromise('selectPPDFile');
  }

  /** @override */
  startDiscoveringPrinters() {
    chrome.send('startDiscoveringPrinters');
  }

  /** @override */
  stopDiscoveringPrinters() {
    chrome.send('stopDiscoveringPrinters');
  }

  /** @override */
  getCupsPrinterManufacturersList() {
    return sendWithPromise('getCupsPrinterManufacturersList');
  }

  /** @override */
  getCupsPrinterModelsList(manufacturer) {
    return sendWithPromise('getCupsPrinterModelsList', manufacturer);
  }

  /** @override */
  getPrinterInfo(newPrinter) {
    return sendWithPromise('getPrinterInfo', newPrinter);
  }

  /** @override */
  getPrinterPpdManufacturerAndModel(printerId) {
    return sendWithPromise('getPrinterPpdManufacturerAndModel', printerId);
  }

  /** @override */
  addDiscoveredPrinter(printerId) {
    return sendWithPromise('addDiscoveredPrinter', printerId);
  }

  /** @override */
  cancelPrinterSetUp(newPrinter) {
    chrome.send('cancelPrinterSetUp', [newPrinter]);
  }

  /** @override */
  getEulaUrl(ppdManufacturer, ppdModel) {
    return sendWithPromise('getEulaUrl', ppdManufacturer, ppdModel);
  }

  /** @override */
  queryPrintServer(serverUrl) {
    return sendWithPromise('queryPrintServer', serverUrl);
  }

  /** @override */
  openPrintManagementApp() {
    chrome.send('openPrintManagementApp');
  }

  /** @override */
  openScanningApp() {
    chrome.send('openScanningApp');
  }
}

addSingletonGetter(CupsPrintersBrowserProxyImpl);
