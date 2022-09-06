// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageCallbackRouter, PageHandler, PageHandlerInterface} from '../realbox.mojom-webui.js';

/**
 * @fileoverview This file provides a singleton class that exposes the Mojo
 * handler interface used for bidirectional communication between the NTP
 * realbox JS and the browser.
 */

let instance: RealboxBrowserProxy|null = null;

export class RealboxBrowserProxy {
  static getInstance(): RealboxBrowserProxy {
    return instance || (instance = new RealboxBrowserProxy());
  }

  static setInstance(newInstance: RealboxBrowserProxy) {
    instance = newInstance;
  }

  handler: PageHandlerInterface;
  callbackRouter: PageCallbackRouter;

  constructor() {
    this.handler = PageHandler.getRemote();
    this.callbackRouter = new PageCallbackRouter();

    this.handler.setPage(this.callbackRouter.$.bindNewPipeAndPassRemote());
  }
}
