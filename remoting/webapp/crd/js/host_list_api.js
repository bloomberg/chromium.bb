// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * API for host-list management.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/** @interface */
remoting.HostListApi = function() {
};

/**
 * @param {string} newHostId
 * @param {string} hostName
 * @param {string} publicKey
 * @param {?string} hostClientId
 * @return {!Promise<string>} An OAuth2 auth code or the empty string.
 */
remoting.HostListApi.prototype.register;

/**
 * Fetch the list of hosts for a user.
 *
 * @param {function(Array<remoting.Host>):void} onDone
 * @param {function(!remoting.Error):void} onError
 */
remoting.HostListApi.prototype.get = function(onDone, onError) {
};

/**
 * Update the information for a host.
 *
 * @param {function():void} onDone
 * @param {function(!remoting.Error):void} onError
 * @param {string} hostId
 * @param {string} hostName
 * @param {string} hostPublicKey
 */
remoting.HostListApi.prototype.put =
    function(hostId, hostName, hostPublicKey, onDone, onError) {
};

/**
 * Delete a host.
 *
 * @param {function():void} onDone
 * @param {function(!remoting.Error):void} onError
 * @param {string} hostId
 */
remoting.HostListApi.prototype.remove = function(hostId, onDone, onError) {
};
