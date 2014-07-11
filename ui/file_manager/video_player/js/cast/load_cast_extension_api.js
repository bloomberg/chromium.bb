// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Defines the cast.ExtensionApi class. This class inherits cast.Api and must
 * be decraled after the parent is loaded. So we introduce this method to
 * decrale it with delay.
 */
function loadCastExtensionApi() {
  if (!cast) {
    console.error('"cast" namespace is not defined.');
    return;
  }

  /**
   * @constructor
   * @param {string} castExtensionId Extension ID of cast extension.
   * @extends {cast.Api}
   */
  cast.ExtensionApi = function(castExtensionId) {
    this.castExtensionId_ = castExtensionId;
    cast.Client.clientId_ = chrome.runtime.id;

    cast.Api.call(this);
  };

  cast.ExtensionApi.prototype.__proto__ = cast.Api.prototype;

  /**
   * @override
   */
  cast.ExtensionApi.prototype.init = function() {
    chrome.runtime.onMessageExternal.addListener(
        this.onMessageExternal_.bind(this));
    this.sendRequest(cast.AppRequestType.REGISTER_CLIENT, {});
    cast.isAvailable = true;
  };

  /**
   * @override
   */
  cast.ExtensionApi.prototype.onDisconnect = function() {};

  /**
   * @override
   */
  cast.ExtensionApi.prototype.sendRequest = function(requestType, request) {
    if (!this.castExtensionId_)
      return null;

    var appRequest = new cast.Message(cast.Client.getClientId(),
        cast.NAME, cast.Client.getNextSeq(), requestType, request);

    // Use response callback for message ACK to detect extension unload.
    var responseCallback = function() {
      if (chrome.runtime.lastError) {
        // Unregister the cast extension.
        this.castExtensionId_ = '';
        cast.isAvailable = false;
        this.onDisconnect();
      }
    }.bind(this);

    chrome.runtime.sendMessage(this.castExtensionId_,
                               appRequest,
                               responseCallback);
    return appRequest;
  };

  /**
   * @param {string} message
   * @param {function(boolean)} sender
   * @param {function(boolean)} sendResponse
   * @private
   */
  cast.ExtensionApi.prototype.onMessageExternal_ = function(message,
      sender, sendResponse) {
    if (!this.castExtensionId_ || sender.id != this.castExtensionId_)
      return;

    // No content.
    if (!message)
      return;

    this.processCastMessage(/** @type {cast.Message} */(message));
  };
}
