// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CommandHandlerFactory, CommandHandlerRemote} from './promo_browser_command.mojom-webui.js';

/**
 * @fileoverview This file provides a class that exposes the Mojo handler
 * interface used for sending the NTP promos browser commands to the browser and
 * receiving the browser response.
 */

/** @type {PromoBrowserCommandProxy} */
let instance = null;

export class PromoBrowserCommandProxy {
  /** @return {!PromoBrowserCommandProxy} */
  static getInstance() {
    return instance || (instance = new PromoBrowserCommandProxy());
  }

  /** @param {PromoBrowserCommandProxy} newInstance */
  static setInstance(newInstance) {
    instance = newInstance;
  }

  constructor() {
    /** @type {!CommandHandlerRemote} */
    this.handler = new CommandHandlerRemote();
    const factory = CommandHandlerFactory.getRemote();
    factory.createBrowserCommandHandler(
        this.handler.$.bindNewPipeAndPassReceiver());
  }
}
