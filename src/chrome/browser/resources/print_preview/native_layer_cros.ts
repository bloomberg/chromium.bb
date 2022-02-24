// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';

import {Cdd} from './data/cdd.js';
import {ProvisionalDestinationInfo} from './data/local_parsers.js';
import {PrinterStatus, PrinterStatusReason} from './data/printer_status_cros.js';

export type PrinterSetupResponse = {
  printerId: string,
  capabilities: Cdd,
};

export type PrintServer = {
  id: string,
  name: string,
};

export type PrintServersConfig = {
  printServers: PrintServer[],
  isSingleServerFetchingMode: boolean,
};

/**
 * An interface to the Chrome OS platform specific part of the native Chromium
 * printing system layer.
 */
export interface NativeLayerCros {
  /**
   * Requests access token for cloud print requests for DEVICE origin.
   */
  getAccessToken(): Promise<string>;

  /**
   * Requests the destination's end user license information. Returns a promise
   * that will be resolved with the destination's EULA URL if obtained
   * successfully.
   * @param destinationId ID of the destination.
   */
  getEulaUrl(destinationId: string): Promise<string>;

  /**
   * Requests Chrome to resolve provisional extension destination by granting
   * the provider extension access to the printer.
   * @param provisionalDestinationId
   */
  grantExtensionPrinterAccess(provisionalDestinationId: string):
      Promise<ProvisionalDestinationInfo>;

  /**
   * Requests that Chrome perform printer setup for the given printer.
   */
  setupPrinter(printerId: string): Promise<PrinterSetupResponse>;

  /**
   * Sends a request to the printer with id |printerId| for its current status.
   */
  requestPrinterStatusUpdate(printerId: string): Promise<PrinterStatus>;

  /**
   * Records the histogram to capture the printer status of the current
   * destination and whether the user chose to print or cancel.
   * @param statusReason Current destination printer status
   * @param didUserAttemptPrint True if user printed, false if user canceled.
   */
  recordPrinterStatusHistogram(
      statusReason: PrinterStatusReason|null,
      didUserAttemptPrint: boolean): void;

  /**
   * Records the histogram to capture if the retried printer status was
   * able to get a valid response from the local printer.
   */
  recordPrinterStatusRetrySuccessHistogram(retrySuccessful: boolean): void;

  /**
   * Selects all print servers with ids in |printServerIds| to query for their
   * printers.
   */
  choosePrintServers(printServerIds: string[]): void;

  /**
   * Gets the available print servers and whether we are in single server
   * fetching mode.
   */
  getPrintServersConfig(): Promise<PrintServersConfig>;
}

export class NativeLayerCrosImpl implements NativeLayerCros {
  getAccessToken() {
    return sendWithPromise('getAccessToken');
  }

  getEulaUrl(destinationId: string) {
    return sendWithPromise('getEulaUrl', destinationId);
  }

  grantExtensionPrinterAccess(provisionalDestinationId: string) {
    return sendWithPromise(
        'grantExtensionPrinterAccess', provisionalDestinationId);
  }

  setupPrinter(printerId: string) {
    return sendWithPromise('setupPrinter', printerId);
  }

  requestPrinterStatusUpdate(printerId: string) {
    return sendWithPromise('requestPrinterStatus', printerId);
  }

  recordPrinterStatusHistogram(
      statusReason: PrinterStatusReason|null, didUserAttemptPrint: boolean) {
    if (statusReason === null) {
      return;
    }

    let histogram;
    switch (statusReason) {
      case (PrinterStatusReason.UNKNOWN_REASON):
        histogram =
            'PrintPreview.PrinterStatus.AttemptedPrintWithUnknownStatus';
        break;
      case (PrinterStatusReason.NO_ERROR):
        histogram = 'PrintPreview.PrinterStatus.AttemptedPrintWithGoodStatus';
        break;
      default:
        histogram = 'PrintPreview.PrinterStatus.AttemptedPrintWithErrorStatus';
        break;
    }
    chrome.send(
        'metricsHandler:recordBooleanHistogram',
        [histogram, didUserAttemptPrint]);
  }

  recordPrinterStatusRetrySuccessHistogram(retrySuccessful: boolean) {
    chrome.send(
        'metricsHandler:recordBooleanHistogram',
        ['PrinterStatusRetrySuccess', retrySuccessful]);
  }

  choosePrintServers(printServerIds: string[]) {
    chrome.send('choosePrintServers', [printServerIds]);
  }

  getPrintServersConfig() {
    return sendWithPromise('getPrintServersConfig');
  }

  static getInstance(): NativeLayerCros {
    return instance || (instance = new NativeLayerCrosImpl());
  }

  static setInstance(obj: NativeLayerCros) {
    instance = obj;
  }
}

let instance: NativeLayerCros|null = null;
