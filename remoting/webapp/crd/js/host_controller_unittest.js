// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Unit tests for host_controller.js.
 */

(function() {

'use strict';

/** @type {remoting.HostController} */
var controller;

/** @type {sinon.Mock} */
var hostListMock = null;

/** @type {sinon.TestStub} */
var generateUuidStub;

/** @type {remoting.MockHostDaemonFacade} */
var mockHostDaemonFacade;

/** @type {sinon.TestStub} */
var hostDaemonFacadeCtorStub;

/** @type {remoting.MockSignalStrategy} */
var mockSignalStrategy;

/** @type {sinon.TestStub} */
var signalStrategyCreateStub;

var FAKE_HOST_PIN = '<FAKE_HOST_PIN>';
var FAKE_USER_EMAIL = '<FAKE_USER_EMAIL>';
var FAKE_USER_NAME = '<FAKE_USER_NAME>';
var FAKE_UUID = '0bad0bad-0bad-0bad-0bad-0bad0bad0bad';
var FAKE_DAEMON_VERSION = '1.2.3.4';
var FAKE_HOST_NAME = '<FAKE_HOST_NAME>';
var FAKE_PUBLIC_KEY = '<FAKE_PUBLIC_KEY>';
var FAKE_PRIVATE_KEY = '<FAKE_PRIVATE_KEY>';
var FAKE_AUTH_CODE = '<FAKE_AUTH_CODE>';
var FAKE_REFRESH_TOKEN = '<FAKE_REFRESH_TOKEN>';
var FAKE_PIN_HASH = '<FAKE_PIN_HASH>';
var FAKE_HOST_CLIENT_ID = '<FAKE_HOST_CLIENT_ID>';
var FAKE_CLIENT_JID = '<FAKE_CLIENT_JID>';
var FAKE_CLIENT_BASE_JID = '<FAKE_CLIENT_BASE_JID>';
var FAKE_IDENTITY_TOKEN = '<FAKE_IDENTITY_TOKEN>';

/** @type {sinon.Spy|Function} */
var getCredentialsFromAuthCodeSpy;

/** @type {sinon.Spy|Function} */
var getPinHashSpy;

/** @type {sinon.Spy|Function} */
var startDaemonSpy;

/** @type {sinon.Spy} */
var unregisterHostByIdSpy;

/** @type {sinon.Spy} */
var onLocalHostStartedSpy;

QUnit.module('host_controller', {
  beforeEach: function(/** QUnit.Assert */ assert) {
    chromeMocks.activate(['identity', 'runtime']);
    chromeMocks.identity.mock$setToken(FAKE_IDENTITY_TOKEN);
    remoting.identity = new remoting.Identity();
    remoting.MockXhr.activate();
    base.debug.assert(remoting.oauth2 === null);
    remoting.oauth2 = new remoting.OAuth2();
    base.debug.assert(remoting.hostList === null);
    remoting.hostList = /** @type {remoting.HostList} */
        (Object.create(remoting.HostList.prototype));

    // When the HostList's unregisterHostById method is called, make
    // sure the argument is correct.
    unregisterHostByIdSpy =
        sinon.stub(remoting.hostList, 'unregisterHostById', function(
            /** string */ hostId) {
          assert.equal(hostId, FAKE_UUID);
        });

    // When the HostList's onLocalHostStarted method is called, make
    // sure the arguments are correct.
    onLocalHostStartedSpy =
        sinon.stub(
            remoting.hostList, 'onLocalHostStarted', function(
                /** string */ hostName,
                /** string */ newHostId,
                /** string */ publicKey) {
              assert.equal(hostName, FAKE_HOST_NAME);
              assert.equal(newHostId, FAKE_UUID);
              assert.equal(publicKey, FAKE_PUBLIC_KEY);
            });

    mockSignalStrategy = new remoting.MockSignalStrategy(
        FAKE_CLIENT_JID + '/extra_junk',
        remoting.SignalStrategy.Type.XMPP);
    signalStrategyCreateStub = sinon.stub(remoting.SignalStrategy, 'create');
    signalStrategyCreateStub.returns(mockSignalStrategy);

    hostDaemonFacadeCtorStub = sinon.stub(remoting, 'HostDaemonFacade');
    mockHostDaemonFacade = new remoting.MockHostDaemonFacade();
    hostDaemonFacadeCtorStub.returns(mockHostDaemonFacade);
    generateUuidStub = sinon.stub(base, 'generateUuid');
    generateUuidStub.returns(FAKE_UUID);
    getCredentialsFromAuthCodeSpy = sinon.spy(
        mockHostDaemonFacade, 'getCredentialsFromAuthCode');
    getPinHashSpy = sinon.spy(mockHostDaemonFacade, 'getPinHash');
    startDaemonSpy = sinon.spy(mockHostDaemonFacade, 'startDaemon');

    // Set up successful responses from mockHostDaemonFacade.
    // Individual tests override these values to create errors.
    mockHostDaemonFacade.features =
        [remoting.HostController.Feature.OAUTH_CLIENT];
    mockHostDaemonFacade.daemonVersion = FAKE_DAEMON_VERSION;
    mockHostDaemonFacade.hostName = FAKE_HOST_NAME;
    mockHostDaemonFacade.privateKey = FAKE_PRIVATE_KEY;
    mockHostDaemonFacade.publicKey = FAKE_PUBLIC_KEY;
    mockHostDaemonFacade.hostClientId = FAKE_HOST_CLIENT_ID;
    mockHostDaemonFacade.userEmail = FAKE_USER_EMAIL;
    mockHostDaemonFacade.refreshToken = FAKE_REFRESH_TOKEN;
    mockHostDaemonFacade.pinHash = FAKE_PIN_HASH;
    mockHostDaemonFacade.startDaemonResult =
        remoting.HostController.AsyncResult.OK;

    sinon.stub(remoting.identity, 'getEmail').returns(
        Promise.resolve(FAKE_USER_EMAIL));
    sinon.stub(remoting.oauth2, 'getRefreshToken').returns(
        FAKE_REFRESH_TOKEN);

    controller = new remoting.HostController();
  },

  afterEach: function() {
    controller = null;
    getCredentialsFromAuthCodeSpy.restore();
    generateUuidStub.restore();
    hostDaemonFacadeCtorStub.restore();
    signalStrategyCreateStub.restore();
    remoting.hostList = null;
    remoting.oauth2 = null;
    remoting.MockXhr.restore();
    chromeMocks.restore();
    remoting.identity = null;
  }
});

/**
 * Install an HTTP response for requests to the registry.
 * @param {QUnit.Assert} assert
 * @param {boolean} withAuthCode
 */
function queueRegistryResponse(assert, withAuthCode) {
  var responseJson = {
      data: {
        authorizationCode: FAKE_AUTH_CODE
      }
  };
  if (!withAuthCode) {
    delete responseJson.data.authorizationCode;
  }

  remoting.MockXhr.setResponseFor(
      'POST', 'DIRECTORY_API_BASE_URL/@me/hosts',
      function(/** remoting.MockXhr */ xhr) {
        assert.deepEqual(
            xhr.params.jsonContent,
            { data: {
              hostId: FAKE_UUID,
              hostName: FAKE_HOST_NAME,
              publicKey: FAKE_PUBLIC_KEY
            } });
        xhr.setTextResponse(200, JSON.stringify(responseJson));
      });
}

/**
 * @param {boolean} successful
 */
function stubSignalStrategyConnect(successful) {
  sinon.stub(mockSignalStrategy, 'connect', function() {
    Promise.resolve().then(function() {
      mockSignalStrategy.setStateForTesting(
          successful ?
            remoting.SignalStrategy.State.CONNECTED :
            remoting.SignalStrategy.State.FAILED);
    });
  });
}

// Check that hasFeature returns false for missing features.
QUnit.test('hasFeature returns false', function(assert) {
  return new Promise(function(resolve, reject) {
    controller.hasFeature(
        remoting.HostController.Feature.PAIRING_REGISTRY,
        resolve);
  }).then(function(/** boolean */ result) {
    assert.equal(result, false);
  });
});

// Check that hasFeature returns true for supported features.
QUnit.test('hasFeature returns true', function(assert) {
  return new Promise(function(resolve, reject) {
    controller.hasFeature(
        remoting.HostController.Feature.OAUTH_CLIENT,
        resolve);
  }).then(function(/** boolean */ result) {
    assert.equal(result, true);
  });
});

// Check what happens when the HostDaemonFacade's getHostName method
// fails.
QUnit.test('start with getHostName failure', function(assert) {
  mockHostDaemonFacade.hostName = null;
  return new Promise(function(resolve, reject) {
    controller.start(FAKE_HOST_PIN, true, function() {
      reject('test failed');
    }, function(/** remoting.Error */ e) {
      assert.equal(e.getDetail(), 'getHostName');
      assert.equal(e.getTag(), remoting.Error.Tag.UNEXPECTED);
      assert.equal(unregisterHostByIdSpy.callCount, 0);
      assert.equal(onLocalHostStartedSpy.callCount, 0);
      assert.equal(startDaemonSpy.callCount, 0);
      resolve(null);
    });
  });
});

// Check what happens when the HostDaemonFacade's generateKeyPair
// method fails.
QUnit.test('start with generateKeyPair failure', function(assert) {
  mockHostDaemonFacade.publicKey = null;
  mockHostDaemonFacade.privateKey = null;
  return new Promise(function(resolve, reject) {
    controller.start(FAKE_HOST_PIN, true, function() {
      reject('test failed');
    }, function(/** remoting.Error */ e) {
      assert.equal(e.getDetail(), 'generateKeyPair');
      assert.equal(e.getTag(), remoting.Error.Tag.UNEXPECTED);
      assert.equal(unregisterHostByIdSpy.callCount, 0);
      assert.equal(onLocalHostStartedSpy.callCount, 0);
      assert.equal(startDaemonSpy.callCount, 0);
      resolve(null);
    });
  });
});

// Check what happens when the HostDaemonFacade's getHostClientId
// method fails.
QUnit.test('start with getHostClientId failure', function(assert) {
  mockHostDaemonFacade.hostClientId = null;
  return new Promise(function(resolve, reject) {
    controller.start(FAKE_HOST_PIN, true, function() {
      reject('test failed');
    }, function(/** remoting.Error */ e) {
      assert.equal(e.getDetail(), 'getHostClientId');
      assert.equal(e.getTag(), remoting.Error.Tag.UNEXPECTED);
      assert.equal(unregisterHostByIdSpy.callCount, 0);
      assert.equal(onLocalHostStartedSpy.callCount, 0);
      assert.equal(startDaemonSpy.callCount, 0);
      resolve(null);
    });
  });
});

// Check what happens when the registry returns an HTTP when we try to
// register a host.
QUnit.test('start with host registration failure', function(assert) {
  remoting.MockXhr.setEmptyResponseFor(
      'POST', 'DIRECTORY_API_BASE_URL/@me/hosts', 500);
  return new Promise(function(resolve, reject) {
    controller.start(FAKE_HOST_PIN, true, function() {
      reject('test failed');
    }, function(/** remoting.Error */ e) {
      assert.equal(e.getTag(), remoting.Error.Tag.REGISTRATION_FAILED);
      assert.equal(unregisterHostByIdSpy.callCount, 0);
      assert.equal(onLocalHostStartedSpy.callCount, 0);
      assert.equal(startDaemonSpy.callCount, 0);
      resolve(null);
    });
  });
});

// Check what happens when the HostDaemonFacade's
// getCredentialsFromAuthCode method fails.
QUnit.test('start with getCredentialsFromAuthCode failure', function(assert) {
  mockHostDaemonFacade.useEmail = null;
  mockHostDaemonFacade.refreshToken = null;
  queueRegistryResponse(assert, true);
  return new Promise(function(resolve, reject) {
    controller.start(FAKE_HOST_PIN, true, function() {
      reject('test failed');
    }, function(/** remoting.Error */ e) {
      assert.equal(e.getDetail(), 'getCredentialsFromAuthCode');
      assert.equal(e.getTag(), remoting.Error.Tag.UNEXPECTED);
      assert.equal(getCredentialsFromAuthCodeSpy.callCount, 1);
      assert.equal(unregisterHostByIdSpy.callCount, 0);
      assert.equal(onLocalHostStartedSpy.callCount, 0);
      assert.equal(startDaemonSpy.callCount, 0);
      resolve(null);
    });
  });
});

// Check what happens when the HostDaemonFacade's getPinHash method
// fails, and verify that getPinHash is called when the registry
// does't return an auth code.
QUnit.test('start with getRefreshToken+getPinHash failure', function(assert) {
  mockHostDaemonFacade.pinHash = null;
  queueRegistryResponse(assert, false);
  return new Promise(function(resolve, reject) {
    controller.start(FAKE_HOST_PIN, true, function() {
      reject('test failed');
    }, function(/** remoting.Error */ e) {
      assert.equal(e.getDetail(), 'getPinHash');
      assert.equal(e.getTag(), remoting.Error.Tag.UNEXPECTED);
      assert.equal(unregisterHostByIdSpy.callCount, 0);
      assert.equal(onLocalHostStartedSpy.callCount, 0);
      assert.equal(startDaemonSpy.callCount, 0);
      resolve(null);
    });
  });
});

// Check what happens when the SignalStrategy fails to connect.
QUnit.test('start with signalStrategy failure', function(assert) {
  queueRegistryResponse(assert, true);
  stubSignalStrategyConnect(false);
  return new Promise(function(resolve, reject) {
    controller.start(FAKE_HOST_PIN, true, function() {
      reject('test failed');
    }, function(/** remoting.Error */ e) {
      assert.equal(e.getDetail(), 'setStateForTesting');
      assert.equal(e.getTag(), remoting.Error.Tag.UNEXPECTED);
      assert.equal(unregisterHostByIdSpy.callCount, 1);
      resolve(null);
    });
  });
});

// Check what happens when the HostDaemonFacade's startDaemon method
// fails and calls its onError argument.
QUnit.test('start with startDaemon failure', function(assert) {
  queueRegistryResponse(assert, true);
  stubSignalStrategyConnect(true);
  mockHostDaemonFacade.startDaemonResult = null;
  return new Promise(function(resolve, reject) {
    controller.start(FAKE_HOST_PIN, true, function() {
      reject('test failed');
    }, function(/** remoting.Error */ e) {
      assert.equal(e.getDetail(), 'startDaemon');
      assert.equal(e.getTag(), remoting.Error.Tag.UNEXPECTED);
      assert.equal(unregisterHostByIdSpy.callCount, 1);
      assert.equal(onLocalHostStartedSpy.callCount, 0);
      resolve(null);
    });
  });
});

// Check what happens when the HostDaemonFacade's startDaemon method
// calls is onDone method with a CANCELLED error code.
QUnit.test('start with startDaemon cancelled', function(assert) {
  queueRegistryResponse(assert, true);
  stubSignalStrategyConnect(true);
  mockHostDaemonFacade.startDaemonResult =
      remoting.HostController.AsyncResult.CANCELLED;
  return new Promise(function(resolve, reject) {
    controller.start(FAKE_HOST_PIN, true, function() {
      reject('test failed');
    }, function(/** remoting.Error */ e) {
      assert.equal(e.getTag(), remoting.Error.Tag.CANCELLED);
      assert.equal(unregisterHostByIdSpy.callCount, 1);
      assert.equal(onLocalHostStartedSpy.callCount, 0);
      resolve(null);
    });
  });
});

// Check what happens when the HostDaemonFacade's startDaemon method
// calls is onDone method with an async error code.
QUnit.test('start with startDaemon returning failure code', function(assert) {
  queueRegistryResponse(assert, true);
  stubSignalStrategyConnect(true);
  mockHostDaemonFacade.startDaemonResult =
      remoting.HostController.AsyncResult.FAILED;
  return new Promise(function(resolve, reject) {
    controller.start(FAKE_HOST_PIN, true, function() {
      reject('test failed');
    }, function(/** remoting.Error */ e) {
      assert.equal(e.getTag(), remoting.Error.Tag.UNEXPECTED);
      assert.equal(unregisterHostByIdSpy.callCount, 1);
      assert.equal(onLocalHostStartedSpy.callCount, 0);
      resolve(null);
    });
  });
});

// Check what happens when the entire host registration process
// succeeds.
[false, true].forEach(function(/** boolean */ consent) {
  QUnit.test('start succeeds with consent=' + consent, function(assert) {
    queueRegistryResponse(assert, true);
    stubSignalStrategyConnect(true);
    return new Promise(function(resolve, reject) {
      controller.start(FAKE_HOST_PIN, consent, function() {
        assert.equal(getCredentialsFromAuthCodeSpy.callCount, 1);
        assert.deepEqual(
            getCredentialsFromAuthCodeSpy.args[0][0],
            FAKE_AUTH_CODE);
        assert.equal(getPinHashSpy.callCount, 1);
        assert.deepEqual(
            getPinHashSpy.args[0].slice(0, 2),
            [FAKE_UUID, FAKE_HOST_PIN]);

        assert.equal(unregisterHostByIdSpy.callCount, 0);
        assert.equal(onLocalHostStartedSpy.callCount, 1);
        assert.equal(startDaemonSpy.callCount, 1);
        assert.deepEqual(
            startDaemonSpy.args[0].slice(0, 2),
            [{
              xmpp_login: FAKE_USER_EMAIL,
              oauth_refresh_token: FAKE_REFRESH_TOKEN,
              host_owner: FAKE_CLIENT_JID.toLowerCase(),
              host_owner_email: FAKE_USER_EMAIL,
              host_id: FAKE_UUID,
              host_name: FAKE_HOST_NAME,
              host_secret_hash: FAKE_PIN_HASH,
              private_key: FAKE_PRIVATE_KEY
            }, consent]);
        resolve(null);
      }, reject);
    });
  });
});

// TODO(jrw): Add tests for |stop| method.
// TODO(jrw): Add tests for |updatePin| method.
// TODO(jrw): Add tests for |getLocalHostState| method.
// TODO(jrw): Add tests for |getLocalHostId| method.
// TODO(jrw): Add tests for |getPairedClients| method.
// TODO(jrw): Add tests for |deletePairedClient| method.
// TODO(jrw): Add tests for |clearPairedClients| method.
// TODO(jrw): Make sure getClientBaseJid_ method is tested.

})();
