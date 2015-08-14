// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

/** @type {remoting.FormatIq} */
var formatter = null;


QUnit.module('FormatIq', {
  beforeEach: function() {
    formatter = new remoting.FormatIq('clientJid', 'hostJid');
  },
  afterEach: function() {
    formatter = null;
  }
});

/**
 * @param {QUnit.Assert} assert
 * @param {string} xmlString
 * @param {boolean} fromClient
 * @param {string} jingleAction
 * @param {remoting.ChromotingEvent.XmppError} expectedError
 */
function runErrorParserTest(assert, xmlString, fromClient, jingleAction,
                            expectedError) {
  var parser = new DOMParser();
  var xml = parser.parseFromString(xmlString, 'text/xml');
  formatter.prettyError(fromClient, jingleAction, xml.firstChild);
  assert.deepEqual(formatter.getMostRecentError(), expectedError);
}

QUnit.test('prettyError()', function(assert) {
  var Event = remoting.ChromotingEvent;

  runErrorParserTest(
    assert,
    '<error type="cancel">' +
      '<service-unavailable xmlns="urn:ietf:params:xml:ns:xmpp-stanzas"/>' +
    '</error>',
    true,
    'session-initiate',
    {
      jingle_action: Event.JingleAction.SESSION_INITIATE,
      condition_string: 'service-unavailable',
      from_client: true,
      type: Event.XmppErrorType.CANCEL
    }
  );

  runErrorParserTest(
    assert,
    '<error type="cancel">' +
      '<bad-request xmlns="urn:ietf:params:xml:ns:xmpp-stanzas"/>' +
    '</error>',
    false,
    'SESSION-INFO',
    {
      jingle_action: Event.JingleAction.SESSION_INFO,
      condition_string: 'bad-request',
      from_client: false,
      type: Event.XmppErrorType.CANCEL
    }
  );

  runErrorParserTest(
    assert,
    '<error type="wait">' +
      '<resource-constraint xmlns="urn:ietf:params:xml:ns:xmpp-stanzas"/>' +
    '</error>',
    true,
    'session-accept',
    {
      jingle_action: Event.JingleAction.SESSION_ACCEPT,
      condition_string: 'resource-constraint',
      from_client: true,
      type: Event.XmppErrorType.WAIT
    }
  );

  runErrorParserTest(
    assert,
    '<error type="modify">' +
      '<redirect xmlns="urn:ietf:params:xml:ns:xmpp-stanzas">' +
        'xmpp:voicemail@capulet.lit' +
      '</redirect>' +
    '</error>',
    true,
    'transport-info',
    {
      jingle_action: Event.JingleAction.TRANSPORT_INFO,
      condition_string: 'redirect',
      from_client: true,
      type: Event.XmppErrorType.MODIFY
    }
  );

  runErrorParserTest(
    assert,
     '<error type="auth">' +
      '<not-authorized xmlns="urn:ietf:params:xml:ns:xmpp-stanzas"/>' +
    '</error>',
    false,
    'session-terminate',
    {
      jingle_action: Event.JingleAction.SESSION_TERMINATE,
      condition_string: 'not-authorized',
      from_client: false,
      type: Event.XmppErrorType.AUTH
    }
  );
});

})();
