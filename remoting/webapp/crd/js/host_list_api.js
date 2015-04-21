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
 * @return {!Promise<!Array<!remoting.Host>>}
 */
remoting.HostListApi.prototype.get = function() {
};

/**
 * Update the information for a host.
 *
 * @param {string} hostId
 * @param {string} hostName
 * @param {string} hostPublicKey
 * @return {!Promise<void>}
 */
remoting.HostListApi.prototype.put =
    function(hostId, hostName, hostPublicKey) {
};

/**
 * Delete a host.
 *
 * @param {string} hostId
 * @return {!Promise<void>}
 */
remoting.HostListApi.prototype.remove = function(hostId) {
};
