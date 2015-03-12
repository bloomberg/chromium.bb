// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * @suppress {checkTypes|checkVars|reportUnknownTypes|visibility}
 */

(function() {

'use strict';

/** @type {sinon.TestStub} */
var mockIsAppsV2 = null;
var mockChromeStorage = {};

function pass() {
  ok(true);
  QUnit.start();
}

function fail() {
  ok(false);
  QUnit.start();
}

/**
 * @param {string} v1UserName
 * @param {string} v1UserEmail
 * @param {boolean} v1HasHosts
 */
function setMigrationData_(v1UserName, v1UserEmail, v1HasHosts) {
  /** @return {!Promise} */
  remoting.identity.getUserInfo = function() {
    if (base.isAppsV2()) {
      return Promise.resolve(
          {email: 'v2user@gmail.com', name: 'v2userName'});
    } else {
      return Promise.resolve(
          {email: v1UserEmail, name: v1UserName});
    }
  };
  /** @return {!Promise} */
  remoting.identity.getEmail = function() {
    return remoting.identity.getUserInfo().then(
        /** @param {{email:string, name:string}} info */
        function(info) {
          return info.email;
        });
  };

  mockIsAppsV2.returns(false);
  if (v1HasHosts) {
    remoting.AppsV2Migration.saveUserInfo();
  }
}

module('AppsV2Migration', {
  setup: function() {
    chromeMocks.activate(['storage']);
    mockIsAppsV2 = sinon.stub(base, 'isAppsV2');
    remoting.identity = new remoting.Identity();
  },
  teardown: function() {
    chromeMocks.restore();
    mockIsAppsV2.restore();
    remoting.identity = null;
  }
});

QUnit.asyncTest(
  'hasHostsInV1App() should reject the promise if v1 user has same identity',
  function() {
    setMigrationData_('v1userName', 'v2user@gmail.com', true);
    mockIsAppsV2.returns(true);
    remoting.AppsV2Migration.hasHostsInV1App().then(fail, pass);
});

QUnit.asyncTest(
  'hasHostsInV1App() should reject the promise if v1 user has no hosts',
  function() {
    setMigrationData_('v1userName', 'v1user@gmail.com', false);
    mockIsAppsV2.returns(true);
    remoting.AppsV2Migration.hasHostsInV1App().then(fail, pass);
});

QUnit.asyncTest(
  'hasHostsInV1App() should reject the promise in v1', function() {
    setMigrationData_('v1userName', 'v1user@gmail.com', true);
    mockIsAppsV2.returns(false);
    remoting.AppsV2Migration.hasHostsInV1App().then(fail, pass);
});

QUnit.asyncTest(
  'hasHostsInV1App() should return v1 identity if v1 user has hosts',
  function() {
    setMigrationData_('v1userName', 'v1user@gmail.com', true);
    mockIsAppsV2.returns(true);
    remoting.AppsV2Migration.hasHostsInV1App().then(
      /** @param {{email:string, name:string}} result */
      function(result) {
        QUnit.equal(result.email, 'v1user@gmail.com');
        QUnit.equal(result.fullName, 'v1userName');
        pass();
      }, fail
    );
});

QUnit.asyncTest(
  'saveUserInfo() should clear the preferences on v2',
  function() {
    setMigrationData_('v1userName', 'v1user@gmail.com', true);
    mockIsAppsV2.returns(true);
    remoting.AppsV2Migration.saveUserInfo();
    remoting.AppsV2Migration.hasHostsInV1App().then(fail, pass);
});

})();
