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

/**
 * @param {function(Array<remoting.Host>):void} onDone
 * @param {function(!remoting.Error):void} onError
 */
remoting.MockHostListApi.prototype.get = function(onDone, onError) {
  remoting.mockIdentity.validateTokenAndCall(onDone, onError, [this.hosts]);
};

/**
 * @param {string} hostId
 * @param {string} hostName
 * @param {string} hostPublicKey
 * @param {function():void} onDone
 * @param {function(!remoting.Error):void} onError
 */
remoting.MockHostListApi.prototype.put =
    function(hostId, hostName, hostPublicKey, onDone, onError) {
  /** @type {remoting.MockHostListApi} */
  var that = this;
  var onTokenValid = function() {
    for (var i = 0; i < that.hosts.length; ++i) {
      var host = that.hosts[i];
      if (host.hostId == hostId) {
        host.hostName = hostName;
        host.hostPublicKey = hostPublicKey;
        onDone();
        return;
      }
    }
    console.error('PUT request for unknown host: ' + hostId +
                  ' (' + hostName + ')');
    onError(remoting.Error.UNEXPECTED);
  };
  remoting.mockIdentity.validateTokenAndCall(onTokenValid, onError, []);
};

/**
 * @param {string} hostId
 * @param {function():void} onDone
 * @param {function(!remoting.Error):void} onError
 */
remoting.MockHostListApi.prototype.remove =
    function(hostId, onDone, onError) {
  /** @type {remoting.MockHostListApi} */
  var that = this;
  var onTokenValid = function() {
    for (var i = 0; i < that.hosts.length; ++i) {
      var host = that.hosts[i];
      if (host.hostId == hostId) {
        that.hosts.splice(i, 1);
        onDone();
        return;
      }
    }
    console.error('DELETE request for unknown host: ' + hostId);
    onError(remoting.Error.UNEXPECTED);
  };
  remoting.mockIdentity.validateTokenAndCall(onTokenValid, onError, []);
};

/**
 * @param {boolean} active
 */
remoting.MockHostListApi.setActive = function(active) {
  remoting.hostListApi = active ? new remoting.MockHostListApi()
                                : new remoting.HostListApiImpl();
};
