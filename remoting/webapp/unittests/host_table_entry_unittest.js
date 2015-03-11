// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

/** @type {remoting.HostTableEntry} */
var hostTableEntry_ = null;
var onConnect_ = null;
var onRename_ = null;
var onDelete_ = null;

module('HostTableEntry', {
  setup: function() {
    onConnect_ = /** @type {function(string)} */ (sinon.spy());
    onRename_ = /** @type {function(remoting.HostTableEntry)} */ (sinon.spy());
    onDelete_ = /** @type {function(remoting.HostTableEntry)} */ (sinon.spy());
    hostTableEntry_ =
        new remoting.HostTableEntry(10,
          onConnect_, onRename_, onDelete_);

    // Setup the DOM dependencies on the confirm delete dialog.
    var fixture = document.getElementById('qunit-fixture');
    fixture.innerHTML = '<div id="confirm-host-delete-message"></div>' +
                        '<div id="confirm-host-delete"></div>' +
                        '<div id="cancel-host-delete"></div>';
    setHost('LocalHost', 'ONLINE');
    fixture.appendChild(hostTableEntry_.element());
    sinon.stub(chrome.i18n, 'getMessage', function(/** string */ tag){
      return tag;
    });
  },
  teardown: function() {
    hostTableEntry_.dispose();
    hostTableEntry_ = null;
    $testStub(chrome.i18n.getMessage).restore();
  }
});

/**
 * @param {string} hostName
 * @param {string} status
 * @param {string=} opt_offlineReason
 */
function setHost(hostName, status, opt_offlineReason) {
  var host = new remoting.Host();
  host.hostName = hostName;
  host.status = status;
  if (opt_offlineReason) {
    host.hostOfflineReason = opt_offlineReason;
  }
  hostTableEntry_.setHost(host);
}


/** @suppress {checkTypes|reportUnknownTypes} */
function sendKeydown(/** HTMLElement */ target, /** number */ keyCode) {
  var event = document.createEvent('KeyboardEvent');
  Object.defineProperty(
      event, 'which', {get: function() { return keyCode; }});
  event.initKeyboardEvent("keydown", true, true, document.defaultView,
                          false, false, false, false, keyCode, keyCode);
  target.dispatchEvent(event);
}

function verifyVisible(
  /** HTMLElement*/ element,
  /** boolean */ isVisible,
  /** string= */ opt_name) {
  var expectedVisibility = (isVisible) ? 'visible' : 'hidden';
  QUnit.equal(element.hidden, !isVisible,
              'Element ' + opt_name + ' should be ' + expectedVisibility);
}

test('Clicking on the confirm button in the confirm dialog deletes the host',
  function() {
  // Setup.
  sinon.stub(remoting, 'setMode', function(/** remoting.AppMode */ mode) {
    if (mode === remoting.AppMode.CONFIRM_HOST_DELETE) {
      document.getElementById('confirm-host-delete').click();
    }
  });

  // Invoke.
  hostTableEntry_.element().querySelector('.delete-button').click();

  // Verify.
  sinon.assert.calledWith(onDelete_, hostTableEntry_);

  // Cleanup.
  $testStub(remoting.setMode).restore();
});

test(
  'Clicking on the cancel button in the confirm dialog cancels host deletion',
  function() {
  // Setup.
  sinon.stub(remoting, 'setMode', function(/** remoting.AppMode */ mode) {
    if (mode === remoting.AppMode.CONFIRM_HOST_DELETE) {
      document.getElementById('cancel-host-delete').click();
    }
  });

  // Invoke.
  hostTableEntry_.element().querySelector('.delete-button').click();

  // Verify.
  sinon.assert.notCalled(onDelete_);

  // Cleanup.
  $testStub(remoting.setMode).restore();
});

test('Clicking on the rename button shows the input field.', function() {
  // Invoke.
  hostTableEntry_.element().querySelector('.rename-button').click();

  // Verify.
  var inputField =
      hostTableEntry_.element().querySelector('.host-rename-input');

  verifyVisible(inputField, true, 'inputField');
  QUnit.equal(document.activeElement, inputField);
});

test('Host renaming is canceled on ESCAPE key.', function() {
  // Invoke.
  var inputField =
      hostTableEntry_.element().querySelector('.host-rename-input');
  hostTableEntry_.element().querySelector('.rename-button').click();

  // Verify.
  verifyVisible(inputField, true, 'inputField');
  QUnit.equal(document.activeElement, inputField);
  sendKeydown(inputField, 27 /* ESCAPE */);
  verifyVisible(inputField, false, 'inputField');
});

test('Host renaming commits on ENTER.', function() {
  // Invoke.
  var inputField =
      hostTableEntry_.element().querySelector('.host-rename-input');
  hostTableEntry_.element().querySelector('.rename-button').click();
  inputField.value = 'Renamed Host';
  sendKeydown(inputField, 13 /* ENTER */);

  // Verify
  verifyVisible(inputField, false, 'inputField');
  sinon.assert.called(onRename_);
  QUnit.equal(hostTableEntry_.host.hostName, 'Renamed Host');

  // Renaming shouldn't trigger a connection request.
  sinon.assert.notCalled(onConnect_);
});

test('HostTableEntry renders the host name correctly.', function() {
  var label = hostTableEntry_.element().querySelector('.host-name-label');
  QUnit.equal(label.innerText, 'LocalHost');
});

test('HostTableEntry renders an offline host correctly.', function() {
  setHost('LocalHost', 'OFFLINE', 'INITIALIZATION_FAILED');
  var label = hostTableEntry_.element().querySelector('.host-name-label');
  QUnit.equal(label.innerText, 'OFFLINE');
  QUnit.equal(label.title, 'OFFLINE_REASON_INITIALIZATION_FAILED');
});

test('HostTableEntry renders an out-of-date host correctly', function() {
  sinon.stub(remoting.Host, 'needsUpdate').returns(true);
  setHost('LocalHost', 'ONLINE');
  var warningOverlay =
      hostTableEntry_.element().querySelector('.warning-overlay');
  var label = hostTableEntry_.element().querySelector('.host-name-label');
  verifyVisible(warningOverlay, true, 'warning overlay');
  QUnit.equal(label.innerText, 'UPDATE_REQUIRED');
});

test('Clicking on an online host connects it', function() {
  hostTableEntry_.element().querySelector('.host-name-label').click();
  sinon.assert.calledWith(onConnect_,
                          encodeURIComponent(hostTableEntry_.host.hostId));
});

test('Clicking on an offline host should be a no-op', function() {
  setHost('LocalHost', 'OFFLINE');
  hostTableEntry_.element().querySelector('.host-name-label').click();
  sinon.assert.notCalled(onConnect_);
});

test('HostTableEntry handles host that is null', function() {
  hostTableEntry_.setHost(null);
  hostTableEntry_.element().querySelector('.host-name-label').click();
  sinon.assert.notCalled(onConnect_);
});

})();
