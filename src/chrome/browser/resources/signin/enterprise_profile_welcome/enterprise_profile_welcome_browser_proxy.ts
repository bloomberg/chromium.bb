// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used by the enterprise profile welcome screen
 * to interact with the browser.
 */

import {sendWithPromise} from 'chrome://resources/js/cr.m.js';

// Enterprise profile info sent from C++.
export type EnterpriseProfileInfo = {
  backgroundColor: string,
  pictureUrl: string,
  showEnterpriseBadge: boolean,
  enterpriseTitle: string,
  enterpriseInfo: string,
  proceedLabel: string,
};

export interface EnterpriseProfileWelcomeBrowserProxy {
  // Called when the page is ready
  initialized(): Promise<EnterpriseProfileInfo>;

  initializedWithSize(height: number): void;

  /**
   * Called when the user clicks the proceed button.
   */
  proceed(): void;

  /**
   * Called when the user clicks the cancel button.
   */
  cancel(): void;
}

export class EnterpriseProfileWelcomeBrowserProxyImpl implements
    EnterpriseProfileWelcomeBrowserProxy {
  initialized() {
    return sendWithPromise('initialized');
  }

  initializedWithSize(height: number) {
    chrome.send('initializedWithSize', [height]);
  }

  proceed() {
    chrome.send('proceed');
  }

  cancel() {
    chrome.send('cancel');
  }

  static getInstance(): EnterpriseProfileWelcomeBrowserProxy {
    return instance ||
        (instance = new EnterpriseProfileWelcomeBrowserProxyImpl());
  }

  static setInstance(obj: EnterpriseProfileWelcomeBrowserProxy) {
    instance = obj;
  }
}

let instance: EnterpriseProfileWelcomeBrowserProxy|null = null;
