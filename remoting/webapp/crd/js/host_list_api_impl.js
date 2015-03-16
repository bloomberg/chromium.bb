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
  /** @type {function(XMLHttpRequest):void} */
  var parseHostListResponse =
      this.parseHostListResponse_.bind(this, onDone, onError);
  /** @param {string} token */
  var onToken = function(token) {
    remoting.xhr.start({
      method: 'GET',
      url: remoting.settings.DIRECTORY_API_BASE_URL + '/@me/hosts',
      onDone: parseHostListResponse,
      oauthToken: token
    });
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
  /** @param {string} token */
  var onToken = function(token) {
    var newHostDetails = {
      'data': {
        'hostId': hostId,
        'hostName': hostName,
        'publicKey': hostPublicKey
      }
    };
    remoting.xhr.start({
      method: 'PUT',
      url: remoting.settings.DIRECTORY_API_BASE_URL + '/@me/hosts/' + hostId,
      onDone: remoting.xhr.defaultResponse(onDone, onError),
      jsonContent: newHostDetails,
      oauthToken: token
    });
  };
  remoting.identity.getToken().then(onToken, remoting.Error.handler(onError));
};

/**
 * Delete a host.
 *
 * @param {function():void} onDone
 * @param {function(!remoting.Error):void} onError
 * @param {string} hostId
 */
remoting.HostListApiImpl.prototype.remove = function(hostId, onDone, onError) {
  /** @param {string} token */
  var onToken = function(token) {
    remoting.xhr.start({
      method: 'DELETE',
      url: remoting.settings.DIRECTORY_API_BASE_URL + '/@me/hosts/' + hostId,
      onDone: remoting.xhr.defaultResponse(onDone, onError,
                                           [remoting.Error.Tag.NOT_FOUND]),
      oauthToken: token
    });
  };
  remoting.identity.getToken().then(onToken, remoting.Error.handler(onError));
};

/**
 * Handle the results of the host list request.  A success response will
 * include a JSON-encoded list of host descriptions, which is parsed and
 * passed to the callback.
 *
 * @param {function(Array<remoting.Host>):void} onDone
 * @param {function(!remoting.Error):void} onError
 * @param {XMLHttpRequest} xhr
 * @private
 */
remoting.HostListApiImpl.prototype.parseHostListResponse_ =
    function(onDone, onError, xhr) {
  if (xhr.status == 200) {
    var response = /** @type {{data: {items: Array}}} */
        (base.jsonParseSafe(xhr.responseText));
    if (!response || !response.data) {
      console.error('Invalid "hosts" response from server.');
      onError(remoting.Error.unexpected());
    } else {
      var items = response.data.items || [];
      var hosts = items.map(
        function(/** Object */ item) {
          var host = new remoting.Host();
          host.hostName = getStringAttr(item, 'hostName', '');
          host.hostId = getStringAttr(item, 'hostId', '');
          host.status = getStringAttr(item, 'status', '');
          host.jabberId = getStringAttr(item, 'jabberId', '');
          host.publicKey = getStringAttr(item, 'publicKey', '');
          host.hostVersion = getStringAttr(item, 'hostVersion', '');
          host.tokenUrlPatterns = getArrayAttr(item, 'tokenUrlPatterns', []);
          host.updatedTime = getStringAttr(item, 'updatedTime', '');
          host.hostOfflineReason = getStringAttr(item, 'hostOfflineReason', '');
          return host;
      });
      onDone(hosts);
    }
  } else {
    onError(remoting.Error.fromHttpStatus(xhr.status));
  }
};

/** @type {remoting.HostListApi} */
remoting.hostListApi = new remoting.HostListApiImpl();

})();

