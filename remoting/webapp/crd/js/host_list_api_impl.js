// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * REST API for host-list management.
 */

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

'use strict';

/**
 * @constructor
 * @implements {remoting.HostListApi}
 */
remoting.HostListApiImpl = function() {
};

/**
 * Fetch the list of hosts for a user.
 *
 * @param {function(Array<remoting.Host>):void} onDone
 * @param {function(!remoting.Error):void} onError
 */
remoting.HostListApiImpl.prototype.get = function(onDone, onError) {
  /** @type {function(!remoting.Xhr.Response):void} */
  var parseHostListResponse =
      this.parseHostListResponse_.bind(this, onDone, onError);
  /** @param {string} token */
  var onToken = function(token) {
    new remoting.Xhr({
      method: 'GET',
      url: remoting.settings.DIRECTORY_API_BASE_URL + '/@me/hosts',
      oauthToken: token
    }).start().then(parseHostListResponse);
  };
  remoting.identity.getToken().then(onToken, remoting.Error.handler(onError));
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
remoting.HostListApiImpl.prototype.put =
    function(hostId, hostName, hostPublicKey, onDone, onError) {
  new remoting.Xhr({
    method: 'PUT',
    url: remoting.settings.DIRECTORY_API_BASE_URL + '/@me/hosts/' + hostId,
    jsonContent: {
      'data': {
        'hostId': hostId,
        'hostName': hostName,
        'publicKey': hostPublicKey
      }
    },
    useIdentity: true
  }).start().then(remoting.HostListApiImpl.defaultResponse_(onDone, onError));
};

/**
 * Delete a host.
 *
 * @param {function():void} onDone
 * @param {function(!remoting.Error):void} onError
 * @param {string} hostId
 */
remoting.HostListApiImpl.prototype.remove = function(hostId, onDone, onError) {
  new remoting.Xhr({
    method: 'DELETE',
    url: remoting.settings.DIRECTORY_API_BASE_URL + '/@me/hosts/' + hostId,
    useIdentity: true
  }).start().then(remoting.HostListApiImpl.defaultResponse_(
      onDone, onError, [remoting.Error.Tag.NOT_FOUND]));
};

/**
 * Handle the results of the host list request.  A success response will
 * include a JSON-encoded list of host descriptions, which is parsed and
 * passed to the callback.
 *
 * @param {function(Array<remoting.Host>):void} onDone
 * @param {function(!remoting.Error):void} onError
 * @param {!remoting.Xhr.Response} response
 * @private
 */
remoting.HostListApiImpl.prototype.parseHostListResponse_ =
    function(onDone, onError, response) {
  if (response.status == 200) {
    var obj = /** @type {{data: {items: Array}}} */
        (base.jsonParseSafe(response.getText()));
    if (!obj || !obj.data) {
      console.error('Invalid "hosts" response from server.');
      onError(remoting.Error.unexpected());
    } else {
      var items = obj.data.items || [];
      var hosts = items.map(
        function(/** Object */ item) {
          var host = new remoting.Host(base.getStringAttr(item, 'hostId', ''));
          host.hostName = base.getStringAttr(item, 'hostName', '');
          host.status = base.getStringAttr(item, 'status', '');
          host.jabberId = base.getStringAttr(item, 'jabberId', '');
          host.publicKey = base.getStringAttr(item, 'publicKey', '');
          host.hostVersion = base.getStringAttr(item, 'hostVersion', '');
          host.tokenUrlPatterns =
              base.getArrayAttr(item, 'tokenUrlPatterns', []);
          host.updatedTime = base.getStringAttr(item, 'updatedTime', '');
          host.hostOfflineReason =
              base.getStringAttr(item, 'hostOfflineReason', '');
          return host;
      });
      onDone(hosts);
    }
  } else {
    onError(remoting.Error.fromHttpStatus(response.status));
  }
};

/**
 * Generic success/failure response proxy.
 *
 * @param {function():void} onDone
 * @param {function(!remoting.Error):void} onError
 * @param {Array<remoting.Error.Tag>=} opt_ignoreErrors
 * @return {function(!remoting.Xhr.Response):void}
 * @private
 */
remoting.HostListApiImpl.defaultResponse_ = function(
    onDone, onError, opt_ignoreErrors) {
  /** @param {!remoting.Xhr.Response} response */
  var result = function(response) {
    var error = remoting.Error.fromHttpStatus(response.status);
    if (error.isNone()) {
      onDone();
      return;
    }

    if (opt_ignoreErrors && error.hasTag.apply(error, opt_ignoreErrors)) {
      onDone();
      return;
    }

    onError(error);
  };
  return result;
};

/** @type {remoting.HostListApi} */
remoting.hostListApi = new remoting.HostListApiImpl();

})();
