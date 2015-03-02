// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains various hacks needed to inform JSCompiler of various
// WebKit- and Chrome-specific properties and methods. It is used only with
// JSCompiler to verify the type-correctness of our code.

/** @type {Object} */
chrome.cast = {};

/** @constructor */
chrome.cast.AutoJoinPolicy = function() {};

/** @type {chrome.cast.AutoJoinPolicy} */
chrome.cast.AutoJoinPolicy.PAGE_SCOPED;

/** @type {chrome.cast.AutoJoinPolicy} */
chrome.cast.AutoJoinPolicy.ORIGIN_SCOPED;

/** @type {chrome.cast.AutoJoinPolicy} */
chrome.cast.AutoJoinPolicy.TAB_AND_ORIGIN_SCOPED;

/** @constructor */
chrome.cast.DefaultActionPolicy = function() {};

/** @type {chrome.cast.DefaultActionPolicy} */
chrome.cast.DefaultActionPolicy.CAST_THIS_TAB;

/** @type {chrome.cast.DefaultActionPolicy} */
chrome.cast.DefaultActionPolicy.CREATE_SESSION;

/** @constructor */
chrome.cast.Error = function() {};

/** @constructor */
chrome.cast.ReceiverAvailability = function() {};

/** @type {chrome.cast.ReceiverAvailability} */
chrome.cast.ReceiverAvailability.AVAILABLE;

/** @type {chrome.cast.ReceiverAvailability} */
chrome.cast.ReceiverAvailability.UNAVAILABLE;

/** @type {Object} */
chrome.cast.media = {};

/** @constructor */
chrome.cast.media.Media = function() {
  /** @type {number} */
  this.mediaSessionId = 0;
};

/** @constructor */
chrome.cast.Session = function() {
  /** @type {Array<chrome.cast.media.Media>} */
  this.media = [];

  /** @type {string} */
  this.sessionId = '';
};

/**
 * @param {string} namespace
 * @param {Object} message
 * @param {function():void} successCallback
 * @param {function(chrome.cast.Error):void} errorCallback
 */
chrome.cast.Session.prototype.sendMessage =
    function(namespace, message, successCallback, errorCallback) {};

/**
 * @param {function(chrome.cast.media.Media):void} listener
 */
chrome.cast.Session.prototype.addMediaListener = function(listener) {};

/**
 * @param {function(boolean):void} listener
 */
chrome.cast.Session.prototype.addUpdateListener = function(listener) {};

/**
 * @param {string} namespace
 * @param {function(string, string):void} listener
 */
chrome.cast.Session.prototype.addMessageListener =
    function(namespace, listener){};

/**
 * @param {function():void} successCallback
 * @param {function(chrome.cast.Error):void} errorCallback
 */
chrome.cast.Session.prototype.stop =
    function(successCallback, errorCallback) {};

/**
 * @constructor
 * @param {string} applicationID
 */
chrome.cast.SessionRequest = function(applicationID) {};

/**
 * @constructor
 * @param {chrome.cast.SessionRequest} sessionRequest
 * @param {function(chrome.cast.Session):void} sessionListener
 * @param {function(chrome.cast.ReceiverAvailability):void} receiverListener
 * @param {chrome.cast.AutoJoinPolicy=} opt_autoJoinPolicy
 * @param {chrome.cast.DefaultActionPolicy=} opt_defaultActionPolicy
 */
chrome.cast.ApiConfig = function(sessionRequest,
                                 sessionListener,
                                 receiverListener,
                                 opt_autoJoinPolicy,
                                 opt_defaultActionPolicy) {};

/**
 * @param {chrome.cast.ApiConfig} apiConfig
 * @param {function():void} onInitSuccess
 * @param {function(chrome.cast.Error):void} onInitError
 */
chrome.cast.initialize =
    function(apiConfig, onInitSuccess, onInitError) {};

/**
 * @param {function(chrome.cast.Session):void} successCallback
 * @param {function(chrome.cast.Error):void} errorCallback
 */
chrome.cast.requestSession =
    function(successCallback, errorCallback) {};
