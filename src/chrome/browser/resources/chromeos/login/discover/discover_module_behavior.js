// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'DiscoverModuleBehavior' is a behavior for discover modules.
 * Extends OobeDialogHostBehavior, I18nBehavior.
 */


/**
 * Implements extension of OobeDialogHostBehavior.
 * @polymerBehavior
 */
var DiscoverModuleBehaviorImpl = {
  properties: {
    /**
     * Discover module name. Must be set explicitly.
     */
    module: String,
  },

  /**
   * Sends one-way message to Discover API.
   * @param {string} message Message to send.  Must start with
   *     'discover.moduleName.'.
   * @param {Array<*>} parameters
   */
  discoverCall(message, parameters) {
    assert(this.module.length > 0);
    assert(message.startsWith('discover.' + this.module + '.'));
    window.discoverSendImpl(message, undefined, parameters);
  },

  /**
   * Sends Discover API message with callback to be invoked on reply.
   * @param {string} message Message to send.  Must start with
   *     'discover.moduleName.'.
   * @param {Array<*>} parameters Message parameters.
   * @param {!function(*)} callback Callback to be invoked on reply.
   */
  discoverCallWithReply(message, parameters, callback) {
    assert(this.module.length > 0);
    assert(message.startsWith('discover.' + this.module + '.'));
    assert(callback instanceof Function);
    window.discoverSendImpl(message, callback, parameters);
  },

  updateLocalizedContent() {
    this.i18nUpdateLocale();
  },

  show() {},

  updateLocalizedContent() {
    // Pass to I18nBehavior.
    this.i18nUpdateLocale();
  },
};

var DiscoverModuleBehavior =
    [OobeI18nBehavior, OobeDialogHostBehavior, DiscoverModuleBehaviorImpl];
