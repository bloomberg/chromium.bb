// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

/** @type {MockConsent} */
var consentDialog = null;
/** @type {sinon.Spy | Function} */
var promptForConsent = null;
/** @type {sinon.Spy | Function} */
var getAuthToken = null;
/** @type {remoting.Identity} */
var identity = null;

/**
 * @param {QUnit.Assert} assert
 * @constructor
 * @implements {remoting.Identity.ConsentDialog}
 */
var MockConsent = function(assert) {
  /** @type {boolean} */
  this.grantConsent = true;
  /** @type {Array<string> | undefined} */
  this.scopes = undefined;
  /** @private {QUnit.Assert} */
  this.assert_ = assert;
};

MockConsent.prototype.show = function() {
  // The consent dialog should only be shown if a previous call to getAuthToken
  // with {interactive: false} failed, and it should occur before any call with
  // {interactive: true}.
  this.assert_.ok(getAuthToken.calledOnce);
  this.assert_.ok(getAuthToken.calledWith({'interactive': false}));
  getAuthToken.reset();

  if (this.grantConsent) {
    chromeMocks.identity.mock$setToken('token');
  }
  return Promise.resolve();
};

QUnit.module('Identity', {
  beforeEach: function(/** QUnit.Assert*/ assert) {
    chromeMocks.identity.mock$clearToken();
    chromeMocks.activate(['identity', 'runtime']);
    consentDialog = new MockConsent(assert);
    promptForConsent = sinon.spy(consentDialog, 'show');
    identity = new remoting.Identity(consentDialog);
    getAuthToken = sinon.spy(chromeMocks.identity, 'getAuthToken');
  },
  afterEach: function() {
    chromeMocks.restore();
    getAuthToken.restore();
  }
});

QUnit.test('consent is requested only on first invocation', function(assert) {
  assert.ok(!promptForConsent.called);
  return identity.getToken().then(
      function(/** string */ token) {
        assert.ok(promptForConsent.called);
        assert.ok(getAuthToken.calledOnce);
        assert.ok(getAuthToken.calledWith({'interactive': true}));

        // Request another token.
        promptForConsent.reset();
        getAuthToken.reset();
        return identity.getToken();

      }).then(function(/** string */ token) {
        assert.ok(!promptForConsent.called);
        assert.ok(getAuthToken.calledOnce);
        assert.ok(getAuthToken.calledWith({'interactive': true}));
        assert.equal(token, 'token');
      });
});

QUnit.test('cancellations are reported correctly', function(assert) {
  consentDialog.grantConsent = false;
  chromeMocks.runtime.lastError.message = 'The user did not approve access.';
  return identity.getToken().then(
      function(/** string */ token) {
        assert.ok(false, 'expected getToken() to fail');
      }).catch(function(/** remoting.Error */ error) {
        assert.equal(error.getTag(), remoting.Error.Tag.CANCELLED);
      });
});


QUnit.test('other errors are reported correctly', function(assert) {
  consentDialog.grantConsent = false;
  chromeMocks.runtime.lastError.message = '<some other error message>';
  return identity.getToken().then(
      function(/** string */ token) {
        assert.ok(false, 'expected getToken() to fail');
      }).catch(function(/** remoting.Error */ error) {
        assert.equal(error.getTag(), remoting.Error.Tag.NOT_AUTHENTICATED);
      });
});

}());
