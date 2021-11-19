// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeLayerCros, NativeLayerCrosImpl, PrinterSetupResponse, PrinterStatus, PrinterStatusReason, PrinterStatusSeverity, PrintServersConfig} from 'chrome://print/print_preview.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

export function setNativeLayerCrosInstance(): NativeLayerCrosStub {
  const instance = new NativeLayerCrosStub();
  NativeLayerCrosImpl.setInstance(instance);
  return instance;
}

/**
 * Test version of the Chrome OS native layer.
 */
export class NativeLayerCrosStub extends TestBrowserProxy implements
    NativeLayerCros {
  /** The response to be sent on a |setupPrinter| call. */
  private setupPrinterResponse_: PrinterSetupResponse|null = null;

  /** Whether the printer setup request should be rejected. */
  private shouldRejectPrinterSetup_: boolean = false;

  private eulaUrl_: string = '';

  private printerStatusMap_: Map<string, PrinterStatus> = new Map();

  private multiplePrinterStatusRequestsPromise_: PromiseResolver<void>|null =
      null;

  private multiplePrinterStatusRequestsCount_: number = 0;

  private printServersConfig_: PrintServersConfig|
      null = {printServers: [], isSingleServerFetchingMode: false};

  /** When true, all printer status retry requests return NO_ERROR. */
  private simulateStatusRetrySuccesful_: boolean = false;

  constructor() {
    super([
      'getEulaUrl',
      'requestPrinterStatusUpdate',
      'setupPrinter',
      'choosePrintServers',
      'getPrintServersConfig',
      'recordPrinterStatusRetrySuccessHistogram',
    ]);
  }

  getEulaUrl(destinationId: string) {
    this.methodCalled('getEulaUrl', {destinationId: destinationId});

    return Promise.resolve(this.eulaUrl_);
  }

  setupPrinter(printerId: string) {
    this.methodCalled('setupPrinter', printerId);
    return this.shouldRejectPrinterSetup_ ?
        Promise.reject(assert(this.setupPrinterResponse_!)) :
        Promise.resolve(assert(this.setupPrinterResponse_!));
  }

  getAccessToken() {
    return Promise.resolve('123');
  }

  grantExtensionPrinterAccess(provisionalId: string) {
    return Promise.resolve({
      extensionId: 'abc123',
      extensionName: 'my extension',
      id: provisionalId,
      name: provisionalId + '_Name',
    });
  }

  /**
   * @param response The response to send when |setupPrinter| is called.
   * @param opt_reject Whether printSetup requests should be
   *     rejected. Defaults to false (will resolve callback) if not provided.
   */
  setSetupPrinterResponse(
      response: PrinterSetupResponse, opt_reject?: boolean) {
    this.shouldRejectPrinterSetup_ = opt_reject || false;
    this.setupPrinterResponse_ = response;
  }

  /** @param eulaUrl The eulaUrl of the PPD. */
  setEulaUrl(eulaUrl: string) {
    this.eulaUrl_ = eulaUrl;
  }

  requestPrinterStatusUpdate(printerId: string): Promise<PrinterStatus> {
    this.methodCalled('requestPrinterStatusUpdate');
    if (this.multiplePrinterStatusRequestsPromise_) {
      this.multiplePrinterStatusRequestsCount_--;
      if (this.multiplePrinterStatusRequestsCount_ === 0) {
        this.multiplePrinterStatusRequestsPromise_.resolve();
        this.multiplePrinterStatusRequestsPromise_ = null;
      }
    }

    const printerStatus = this.printerStatusMap_.get(printerId)!;

    // When |simulateStatusRetrySuccesful_| is true, force the next status
    // request for |printerId| to return NO_ERROR.
    if (this.simulateStatusRetrySuccesful_) {
      this.addPrinterStatusToMap(printerId, {
        printerId: printerId,
        statusReasons: [{
          reason: PrinterStatusReason.NO_ERROR,
          severity: PrinterStatusSeverity.REPORT
        }],
        timestamp: Date.now(),
      });
    }
    return Promise.resolve(printerStatus);
  }

  addPrinterStatusToMap(printerId: string, printerStatus: PrinterStatus) {
    this.printerStatusMap_.set(printerId, printerStatus);
  }

  /**
   * @param count The number of printer status requests to wait for.
   * @return Promise that resolves after |count| requests.
   */
  waitForMultiplePrinterStatusRequests(count: number): Promise<void> {
    assert(this.multiplePrinterStatusRequestsPromise_ === null);
    this.multiplePrinterStatusRequestsCount_ = count;
    this.multiplePrinterStatusRequestsPromise_ = new PromiseResolver();
    return this.multiplePrinterStatusRequestsPromise_.promise;
  }

  recordPrinterStatusHistogram(
      _statusReason: PrinterStatusReason, _didUserAttemptPrint: boolean) {}

  choosePrintServers(printServerIds: string[]) {
    this.methodCalled('choosePrintServers', printServerIds);
  }

  getPrintServersConfig() {
    this.methodCalled('getPrintServersConfig');
    return Promise.resolve(this.printServersConfig_!);
  }

  recordPrinterStatusRetrySuccessHistogram(retrySuccessful: boolean) {
    this.methodCalled(
        'recordPrinterStatusRetrySuccessHistogram', retrySuccessful);
  }

  setPrintServersConfig(printServersConfig: PrintServersConfig) {
    this.printServersConfig_ = printServersConfig;
  }

  simulateStatusRetrySuccesful() {
    this.simulateStatusRetrySuccesful_ = true;
  }
}
