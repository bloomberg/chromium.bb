// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Unit test for host_daemon_facade.js.
 */

/** @type {chromeMocks.runtime.Port} */
var nativePortMock;

(function() {
'use strict';

/** @type {sinon.TestStub} */
var postMessageStub;

/** @type {Array<Object>} */
var mockHostResponses;

/** @type {remoting.HostDaemonFacade} */
var it;

QUnit.module('host_daemon_facade', {
  beforeEach: function(/** QUnit.Assert */ assert) {
    chromeMocks.activate(['runtime']);
    chromeMocks.identity.mock$setToken('my_token');
    nativePortMock =
        chromeMocks.runtime.connectNative('com.google.chrome.remote_desktop');
    mockHostResponses = [];
    postMessageStub = sinon.stub(
        nativePortMock, 'postMessage', sendMockHostResponse);
  },
  afterEach: function(/** QUnit.Assert */ assert) {
    if (mockHostResponses.length) {
      throw new Error('responses not all used');
    }
    mockHostResponses = null;
    postMessageStub.restore();
    chromeMocks.restore();
    it = null;
  }
});

function sendMockHostResponse() {
  if (mockHostResponses.length == 0) {
    throw new Error('don\'t know how to responsd');
  }
  var toSend = mockHostResponses.pop();
  Promise.resolve().then(function() {
    nativePortMock.onMessage.mock$fire(toSend);
  });
}

QUnit.test('initialize/hasFeature true', function(assert) {
  mockHostResponses.push({
    id: 0,
    type: 'helloResponse',
    version: '',
    supportedFeatures: [
      remoting.HostController.Feature.OAUTH_CLIENT,
      remoting.HostController.Feature.PAIRING_REGISTRY
    ]
  });
  it = new remoting.HostDaemonFacade();
  assert.deepEqual(postMessageStub.args[0][0], {
    id: 0,
    type: 'hello'
  });
  var done = assert.async();
  it.hasFeature(
      remoting.HostController.Feature.PAIRING_REGISTRY,
      function onDone(arg) {
        assert.equal(arg, true);
        done();
      });
});

QUnit.test('initialize/hasFeature false', function(assert) {
  mockHostResponses.push({
    id: 0,
    type: 'helloResponse',
    version: '',
    supportedFeatures: []
  });
  it = new remoting.HostDaemonFacade();
  assert.deepEqual(postMessageStub.args[0][0], {
    id: 0,
    type: 'hello'
  });
  var done = assert.async();
  it.hasFeature(
      remoting.HostController.Feature.PAIRING_REGISTRY,
      function onDone(arg) {
        assert.equal(arg, false);
        done();
      });
});

QUnit.test('initialize/getDaemonVersion', function(assert) {
  mockHostResponses.push({
    id: 0,
    type: 'helloResponse',
    version: '<daemonVersion>',
    supportedFeatures: []
  });
  it = new remoting.HostDaemonFacade();
  assert.deepEqual(postMessageStub.args[0][0], {
    id: 0,
    type: 'hello'
  });
  var done = assert.async();
  it.getDaemonVersion(
      function onDone(arg) {
        assert.equal(arg, '<daemonVersion>');
        done();
      },
      function onError() {
        assert.ok(false);
        done();
      });
});

/**
 * @param {string} description
 * @param {function(!QUnit.Assert):*} callback
 */
function postInitTest(description, callback) {
  QUnit.test(description, function(assert) {
    mockHostResponses.push({
      id: 0,
      type: 'helloResponse',
      version: ''
    });
    base.debug.assert(it == null);
    it = new remoting.HostDaemonFacade();
    assert.deepEqual(postMessageStub.args[0][0], {
      id: 0,
      type: 'hello'
    });
    return new Promise(function(resolve, reject) {
      it.getDaemonVersion(resolve, reject);
    }).then(function() {
      return callback(assert);
    });
  });
}

/**
 * A combinator that turns an ordinary 1-argument resolve function
 * into a function that accepts multiple arguments and bundles them
 * into an array.
 * @param {function(*):void} resolve
 * @return {function(...*):void}
 */
function resolveMulti(resolve) {
  return function() {
    resolve(Array.prototype.slice.call(arguments));
  };
}

postInitTest('getHostName', function(assert) {
  mockHostResponses.push({
    id: 1,
    type: 'getHostNameResponse',
    hostname: '<fakeHostName>'
  });
  return new Promise(function (resolve, reject) {
    it.getHostName(resolve, reject);
  }).then(function(/** string */ hostName) {
    assert.deepEqual(postMessageStub.args[1][0], {
      id: 1,
      type: 'getHostName'
    });
    assert.equal(hostName, '<fakeHostName>');
  });
});

postInitTest('getPinHash', function(assert) {
  mockHostResponses.push({
    id: 1,
    type: 'getPinHashResponse',
    hash: '<fakePinHash>'
  });
  return new Promise(function (resolve, reject) {
    it.getPinHash('<hostId>', '<pin>', resolve, reject);
  }).then(function(/** string */ hostName) {
    assert.deepEqual(postMessageStub.args[1][0], {
      id: 1,
      type: 'getPinHash',
      hostId: '<hostId>',
      pin: '<pin>'
    });
    assert.equal(hostName, '<fakePinHash>');
  });
});

postInitTest('generateKeyPair', function(assert) {
  mockHostResponses.push({
    id: 1,
    type: 'generateKeyPairResponse',
    privateKey: '<fakePrivateKey>',
    publicKey: '<fakePublicKey>'
  });
  return new Promise(function (resolve, reject) {
    it.generateKeyPair(resolveMulti(resolve), reject);
  }).then(function(/** Array */ result) {
    assert.deepEqual(postMessageStub.args[1][0], {
      id: 1,
      type: 'generateKeyPair'
    });
    assert.deepEqual(result, ['<fakePrivateKey>', '<fakePublicKey>']);
  });
});

postInitTest('updateDaemonConfig', function(assert) {
  mockHostResponses.push({
    id: 1,
    type: 'updateDaemonConfigResponse',
    result: 'OK'
  });
  return new Promise(function (resolve, reject) {
    it.updateDaemonConfig({ fakeDaemonConfig: true }, resolve, reject);
  }).then(function(/** * */ result) {
    assert.deepEqual(postMessageStub.args[1][0], {
      id: 1,
      type: 'updateDaemonConfig',
      config: { fakeDaemonConfig: true }
    });
    assert.equal(result, remoting.HostController.AsyncResult.OK);
  });
});

postInitTest('getDaemonConfig', function(assert) {
  mockHostResponses.push({
    id: 1,
    type: 'getDaemonConfigResponse',
    config: { fakeDaemonConfig: true }
  });
  return new Promise(function (resolve, reject) {
    it.getDaemonConfig(resolve, reject);
  }).then(function(/** * */ result) {
    assert.deepEqual(postMessageStub.args[1][0], {
      id: 1,
      type: 'getDaemonConfig'
    });
    assert.deepEqual(result, { fakeDaemonConfig: true });
  });
});

[0,1,2,3,4,5,6,7].forEach(function(/** number */ flags) {
  postInitTest('getUsageStatsConsent, flags=' + flags, function(assert) {
    var supported = Boolean(flags & 1);
    var allowed = Boolean(flags & 2);
    var setByPolicy = Boolean(flags & 4);
    mockHostResponses.push({
      id: 1,
      type: 'getUsageStatsConsentResponse',
      supported: supported,
      allowed: allowed,
      setByPolicy: setByPolicy
    });
    return new Promise(function (resolve, reject) {
      it.getUsageStatsConsent(resolveMulti(resolve), reject);
    }).then(function(/** * */ result) {
      assert.deepEqual(postMessageStub.args[1][0], {
        id: 1,
        type: 'getUsageStatsConsent'
      });
      assert.deepEqual(result, [supported, allowed, setByPolicy]);
    });
  });
});

[false, true].forEach(function(/** boolean */ consent) {
  postInitTest('startDaemon, consent=' + consent, function(assert) {
    mockHostResponses.push({
      id: 1,
      type: 'startDaemonResponse',
      result: 'FAILED'
    });
    return new Promise(function (resolve, reject) {
      it.startDaemon({ fakeConfig: true }, consent, resolve, reject);
    }).then(function(/** * */ result) {
      assert.deepEqual(postMessageStub.args[1][0], {
        id: 1,
        type: 'startDaemon',
        config: { fakeConfig: true },
        consent: consent
      });
      assert.equal(result, remoting.HostController.AsyncResult.FAILED);
    });
  });
});

postInitTest('stopDaemon', function(assert) {
  mockHostResponses.push({
    id: 1,
    type: 'stopDaemonResponse',
    result: 'CANCELLED'
  });
  return new Promise(function (resolve, reject) {
    it.stopDaemon(resolve, reject);
  }).then(function(/** * */ result) {
    assert.deepEqual(postMessageStub.args[1][0], {
      id: 1,
      type: 'stopDaemon'
    });
    assert.equal(result, remoting.HostController.AsyncResult.CANCELLED);
  });
});

postInitTest('getPairedClients', function(assert) {
  /**
   * @param {number} n
   * @return {remoting.PairedClient}
   */
  function makeClient(n) {
    return /** @type {remoting.PairedClient} */ ({
      clientId: '<fakeClientId' + n + '>',
      clientName: '<fakeClientName' + n + '>',
      createdTime: n * 316571  // random prime number
    });
  };

  var client0 = makeClient(0);
  var client1 = makeClient(1);
  mockHostResponses.push({
    id: 1,
    type: 'getPairedClientsResponse',
    pairedClients: [client0, client1]
  });
  return new Promise(function (resolve, reject) {
    it.getPairedClients(resolve, reject);
  }).then(function(/** Array<remoting.PairedClient> */ result) {
    assert.deepEqual(postMessageStub.args[1][0], {
      id: 1,
      type: 'getPairedClients'
    });
    // Our facade is not really a facade!  It adds extra fields.
    // TODO(jrw): Move non-facade logic to host_controller.js.
    assert.equal(result.length, 2);
    assert.equal(result[0].clientId, '<fakeClientId0>');
    assert.equal(result[0].clientName, '<fakeClientName0>');
    assert.equal(result[0].createdTime, client0.createdTime);
    assert.equal(typeof result[0].createDom, 'function');
    assert.equal(result[1].clientId, '<fakeClientId1>');
    assert.equal(result[1].clientName, '<fakeClientName1>');
    assert.equal(result[1].createdTime, client1.createdTime);
    assert.equal(typeof result[1].createDom, 'function');
  });
});

[false, true].map(function(/** boolean */ deleted) {
  postInitTest('clearPairedClients, deleted=' + deleted, function(assert) {
    mockHostResponses.push({
      id: 1,
      type: 'clearPairedClientsResponse',
      result: deleted
    });
    return new Promise(function (resolve, reject) {
      it.clearPairedClients(resolve, reject);
    }).then(function(/** * */ result) {
      assert.deepEqual(postMessageStub.args[1][0], {
        id: 1,
        type: 'clearPairedClients'
      });
      assert.equal(result, deleted);
    });
  });
});

[false, true].map(function(/** boolean */ deleted) {
  postInitTest('deletePairedClient, deleted=' + deleted, function(assert) {
    mockHostResponses.push({
      id: 1,
      type: 'deletePairedClientResponse',
      result: deleted
    });
    return new Promise(function (resolve, reject) {
      it.deletePairedClient('<fakeClientId>', resolve, reject);
    }).then(function(/** * */ result) {
      assert.deepEqual(postMessageStub.args[1][0], {
        id: 1,
        type: 'deletePairedClient',
        clientId: '<fakeClientId>'
      });
      assert.equal(result, deleted);
    });
  });
});

postInitTest('getHostClientId', function(assert) {
  mockHostResponses.push({
    id: 1,
    type: 'getHostClientIdResponse',
    clientId: '<fakeClientId>'
  });
  return new Promise(function (resolve, reject) {
    it.getHostClientId(resolve, reject);
  }).then(function(/** * */ result) {
    assert.deepEqual(postMessageStub.args[1][0], {
      id: 1,
      type: 'getHostClientId'
    });
    assert.equal(result, '<fakeClientId>');
  });
});

postInitTest('getCredentialsFromAuthCode', function(assert) {
  mockHostResponses.push({
    id: 1,
    type: 'getCredentialsFromAuthCodeResponse',
    userEmail: '<fakeUserEmail>',
    refreshToken: '<fakeRefreshToken>'
  });
  return new Promise(function (resolve, reject) {
    it.getCredentialsFromAuthCode(
        '<fakeAuthCode>', resolveMulti(resolve), reject);
  }).then(function(/** * */ result) {
    assert.deepEqual(postMessageStub.args[1][0], {
      id: 1,
      type: 'getCredentialsFromAuthCode',
      authorizationCode: '<fakeAuthCode>'
    });
    assert.deepEqual(result, ['<fakeUserEmail>', '<fakeRefreshToken>']);
  });
});

})();