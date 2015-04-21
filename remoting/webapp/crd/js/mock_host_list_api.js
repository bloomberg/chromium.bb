// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Mock implementation of remoting.HostList
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 * @implements {remoting.HostListApi}
 */
remoting.MockHostListApi = function() {
  /**
   * The auth code for the |register| method to return, or null if it
   * should fail.
   * @type {?string}
   */
  this.registerResult = null;

  /** @type {Array<remoting.Host>} */
  this.hosts = [
    {
      'hostName': 'Online host',
      'hostId': 'online-host-id',
      'status': 'ONLINE',
      'jabberId': 'online-jid',
      'publicKey': 'online-public-key',
      'tokenUrlPatterns': [],
      'updatedTime': new Date().toISOString()
    },
    {
      'hostName': 'Offline host',
      'hostId': 'offline-host-id',
      'status': 'OFFLINE',
      'jabberId': 'offline-jid',
      'publicKey': 'offline-public-key',
      'tokenUrlPatterns': [],
      'updatedTime': new Date(1970, 1, 1).toISOString()
    }
  ];
};

/** @override */
remoting.MockHostListApi.prototype.register = function(
    newHostId, hostName, publicKey, hostClientId) {
  if (this.registerResult === null) {
    return Promise.reject(
        new remoting.Error(remoting.Error.Tag.REGISTRATION_FAILED));
  } else {
    return Promise.resolve(this.registerResult);
  }
};

/** @override */
remoting.MockHostListApi.prototype.get = function() {
  var that = this;
  new Promise(function(resolve, reject) {
    remoting.mockIdentity.validateTokenAndCall(
        resolve, remoting.Error.handler(reject), [that.hosts]);
  });
};

/** @override */
remoting.MockHostListApi.prototype.put =
    function(hostId, hostName, hostPublicKey) {
  /** @type {remoting.MockHostListApi} */
  var that = this;
  return new Promise(function(resolve, reject) {
    var onTokenValid = function() {
      for (var i = 0; i < that.hosts.length; ++i) {
        var host = that.hosts[i];
        if (host.hostId == hostId) {
          host.hostName = hostName;
          host.hostPublicKey = hostPublicKey;
          resolve(undefined);
          return;
        }
      }
      console.error('PUT request for unknown host: ' + hostId +
                    ' (' + hostName + ')');
      reject(remoting.Error.unexpected());
    };
    remoting.mockIdentity.validateTokenAndCall(onTokenValid, reject, []);
  });
};

/** @override */
remoting.MockHostListApi.prototype.remove = function(hostId) {
  /** @type {remoting.MockHostListApi} */
  var that = this;
  return new Promise(function(resolve, reject) {
    var onTokenValid = function() {
      for (var i = 0; i < that.hosts.length; ++i) {
        var host = that.hosts[i];
        if (host.hostId == hostId) {
          that.hosts.splice(i, 1);
          resolve(undefined);
          return;
        }
      }
      console.error('DELETE request for unknown host: ' + hostId);
      reject(remoting.Error.unexpected());
    };
    remoting.mockIdentity.validateTokenAndCall(onTokenValid, reject, []);
  });
};

/**
 * @param {boolean} active
 */
remoting.MockHostListApi.setActive = function(active) {
  remoting.hostListApi = active ? new remoting.MockHostListApi()
                                : new remoting.HostListApiImpl();
};
