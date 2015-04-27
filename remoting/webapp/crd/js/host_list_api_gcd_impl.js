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
remoting.HostListApiGcdImpl = function() {
  this.gcd_ = new remoting.gcd.Client({
    apiKey: remoting.settings.GOOGLE_API_KEY
  });
};

/** @override */
remoting.HostListApiGcdImpl.prototype.register = function(
    newHostId, hostName, publicKey, hostClientId) {
  var self = this;
  var deviceDraft = {
    channel: {
      supportedType: 'xmpp'
    },
    deviceKind: 'vendor',
    name: newHostId,
    displayName: hostName,
    state: {
      'publicKey': publicKey
    }
  };

  return /** @type {!Promise<string>} */ (
      this.gcd_.insertRegistrationTicket().
      then(function(ticket) {
        return self.gcd_.patchRegistrationTicket(
            ticket.id, deviceDraft, hostClientId);
      }).
      then(function(/**remoting.gcd.RegistrationTicket*/ ticket) {
        return self.gcd_.finalizeRegistrationTicket(ticket.id);
      }).
      then(function(/**remoting.gcd.RegistrationTicket*/ ticket) {
        return ticket.robotAccountAuthorizationCode;
      }).
      catch(function(error) {
        console.error('Error registering device with GCD: ' + error);
        throw new remoting.Error(remoting.Error.Tag.REGISTRATION_FAILED);
      }));
};

/** @override */
remoting.HostListApiGcdImpl.prototype.get = function() {
  // TODO(jrw)
  this.gcd_.listDevices();
  return Promise.resolve([]);
};

/** @override */
remoting.HostListApiGcdImpl.prototype.put =
    function(hostId, hostName, hostPublicKey) {
  // TODO(jrw)
  throw new Error('Not implemented');
};

/** @override */
remoting.HostListApiGcdImpl.prototype.remove = function(hostId) {
  // TODO(jrw)
  throw new Error('Not implemented');
};

})();
