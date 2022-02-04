// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './strings.m.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {PageCallbackRouter, PageHandlerFactory, PageHandlerRemote} from './access_code_cast.mojom-webui.js';

declare const chrome: {
  send(message: string): void;
  getVariableValue(variable: string): string;
};

export class BrowserProxy {
  callbackRouter: PageCallbackRouter;
  handler: PageHandlerRemote;

  constructor() {
    this.callbackRouter = new PageCallbackRouter();

    this.handler = new PageHandlerRemote();

    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(
        this.callbackRouter.$.bindNewPipeAndPassRemote(),
        this.handler.$.bindNewPipeAndPassReceiver());
  }

  closeDialog() {
    chrome.send('dialogClose');
  }

  getDialogArgs() {
    return JSON.parse(chrome.getVariableValue('dialogArguments'));
  }

  isDialog() {
    return chrome.getVariableValue('dialogArguments').length > 0;
  }

  isBarcodeApiAvailable() {
    return ('BarcodeDetector' in window);
  }

  async isQrScanningAvailable() {
    return loadTimeData.getBoolean('qrScannerEnabled')
        && this.isBarcodeApiAvailable()
        && (await this.isCameraAvailable());
  }

  async isCameraAvailable() {
    const devices = await navigator.mediaDevices.enumerateDevices();
    for (const device of devices) {
      if (device.kind === 'videoinput') {
        return true;
      }
    }
    return false;
  }

  static getInstance(): BrowserProxy {
    return instance || (instance = new BrowserProxy());
  }

  static setInstance(obj: BrowserProxy) {
    instance = obj;
  }
}

let instance: BrowserProxy|null = null;
