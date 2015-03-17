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
 * @constructor
 * @implements {remoting.Identity.ConsentDialog}
 */
var MockConsent = function() {
  /** @type {boolean} */
  this.grantConsent = true;
  /** @type {Array<string> | undefined} */
  this.scopes = undefined;
};

MockConsent.prototype.show = function() {
  // The consent dialog should only be shown if a previous call to getAuthToken
  // with {interactive: false} failed, and it should occur before any call with
  // {interactive: true}.
  ok(getAuthToken.calledOnce);
  ok(getAuthToken.calledWith({'interactive': false}));
  getAuthToken.reset();

  if (this.grantConsent) {
    chromeMocks.identity.mock$setToken('token');
  }
  return Promise.resolve();
};

module('Identity', {
  setup: function() {
    chromeMocks.identity.mock$clearToken();
    chromeMocks.activate(['identity', 'runtime']);
    consentDialog = new MockConsent();
    promptForConsent = sinon.spy(consentDialog, 'show');
    identity = new remoting.Identity(consentDialog);
    getAuthToken = sinon.spy(chromeMocks.identity, 'getAuthToken');
  },
  teardown: function() {
    chromeMocks.restore();
    getAuthToken.restore();
  }
});

test('consent is requested only on first invocation', function() {
  ok(!promptForConsent.called);
  return identity.getToken().then(
      function(/** string */ token) {
        ok(promptForConsent.called);
        ok(getAuthToken.calledOnce);
        ok(getAuthToken.calledWith({'interactive': true}));

        // Request another token.
        promptForConsent.reset();
        getAuthToken.reset();
        return identity.getToken();

      }).then(function(/** string */ token) {
        ok(!promptForConsent.called);
        ok(getAuthToken.calledOnce);
        ok(getAuthToken.calledWith({'interactive': true}));
        equal(token, 'token');
      });
});

test('cancellations are reported correctly', function() {
  consentDialog.grantConsent = false;
  chromeMocks.runtime.lastError.message = 'The user did not approve access.';
  return identity.getToken().then(
      function(/** string */ token) {
        ok(false, 'expected getToken() to fail');
      }).catch(function(/** remoting.Error */ error) {
        equal(error.getTag(), remoting.Error.Tag.CANCELLED);
      });
});


test('other errors are reported correctly', function() {
  consentDialog.grantConsent = false;
  chromeMocks.runtime.lastError.message = '<some other error message>';
  return identity.getToken().then(
      function(/** string */ token) {
        ok(false, 'expected getToken() to fail');
      }).catch(function(/** remoting.Error */ error) {
        equal(error.getTag(), remoting.Error.Tag.NOT_AUTHENTICATED);
      });
});

}());
