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

  return /** @type {!Promise<remoting.HostListApi.RegisterResult>} */ (
      this.gcd_.insertRegistrationTicket().
      then(function(ticket) {
        return self.gcd_.patchRegistrationTicket(
            ticket.id, deviceDraft, hostClientId);
      }).
      then(function(/**remoting.gcd.RegistrationTicket*/ ticket) {
        return self.gcd_.finalizeRegistrationTicket(ticket.id);
      }).
      then(function(/**remoting.gcd.RegistrationTicket*/ ticket) {
        return {
          authCode: ticket.robotAccountAuthorizationCode,
          email: ticket.robotAccountEmail
        };
      }).
      catch(function(error) {
        console.error('Error registering device with GCD: ' + error);
        throw new remoting.Error(remoting.Error.Tag.REGISTRATION_FAILED);
      }));
};

/** @override */
remoting.HostListApiGcdImpl.prototype.get = function() {
  return this.gcd_.listDevices().
      then(function(devices) {
        var hosts = [];
        devices.forEach(function(device) {
          try {
            hosts.push(deviceToHost(device));
          } catch (/** @type {*} */ error) {
            console.warn('Invalid device spec:', error);
          }
        });
        return hosts;
      });
};

/** @override */
remoting.HostListApiGcdImpl.prototype.put =
    function(hostId, hostName, hostPublicKey) {
  // TODO(jrw)
  throw new Error('Not implemented');
};

/** @override */
remoting.HostListApiGcdImpl.prototype.remove = function(hostId) {
  var that = this;
  return this.gcd_.listDevices(hostId).then(function(devices) {
    var gcdId = null;
    for (var i = 0; i < devices.length; i++) {
      var device = devices[i];
      // The "name" field in GCD holds what Chromoting considers to be
      // the host ID.
      if (device.name == hostId) {
        gcdId = device.id;
      }
    }
    if (gcdId == null) {
      return false;
    } else {
      return that.gcd_.deleteDevice(gcdId);
    }
  });
};

/** @override */
remoting.HostListApiGcdImpl.prototype.getSupportHost = function(supportId) {
  console.error('getSupportHost not supported by HostListApiGclImpl');
  return Promise.reject(remoting.Error.unexpected());
};

/**
 * Converts a GCD device description to a Host object.
 * @param {!Object} device
 * @return {!remoting.Host}
 */
function deviceToHost(device) {
  var statusMap = {
    'online': 'ONLINE',
    'offline': 'OFFLINE'
  };
  var hostId = base.getStringAttr(device, 'name');
  var host = new remoting.Host(hostId);
  host.hostName = base.getStringAttr(device, 'displayName');
  host.status = base.getStringAttr(
      statusMap, base.getStringAttr(device, 'connectionStatus'));
  var state = base.getObjectAttr(device, 'state', {});
  host.publicKey = base.getStringAttr(state, 'publicKey');
  host.jabberId = base.getStringAttr(state, 'jabberId', '');
  host.hostVersion = base.getStringAttr(state, 'hostVersion', '');
  var creationTimeMs = base.getNumberAttr(device, 'creationTimeMs', 0);
  if (creationTimeMs) {
    host.createdTime = new Date(creationTimeMs).toISOString();
  }
  var lastUpdateTimeMs = base.getNumberAttr(device, 'lastUpdateTimeMs', 0);
  if (lastUpdateTimeMs) {
    host.updatedTime = new Date(lastUpdateTimeMs).toISOString();
  }
  return host;
};

})();
