// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

QUnit.module('ButterBar', {
  beforeEach: function() {
    var fixture = document.getElementById('qunit-fixture');
    fixture.innerHTML =
        '<div id="butter-bar" hidden>' +
        '  <p>' +
        '    <span id="butter-bar-message"></span>' +
        '    <a id="butter-bar-dismiss" href="#" tabindex="0">' +
        '      <img src="icon_cross.webp" class="close-icon">' +
        '    </a>' +
        '  </p>' +
        '</div>';
    this.butterBar = new remoting.ButterBar();
    chrome.storage = {
      sync: {
        get: sinon.stub(),
        set: sinon.stub(),
      }
    };
  },
  afterEach: function() {
    if (this.clock) {
      this.clock.restore();
    }
  }
});

QUnit.test('should stay hidden if index==-1', function(assert) {
  this.butterBar.currentMessage_ = -1;
  return this.butterBar.init().then(() => {
    assert.ok(this.butterBar.root_.hidden == true);
  });
});

QUnit.test('should be shown, yellow and dismissable if index==0',
           function(assert) {
  this.butterBar.currentMessage_ = 0;
  chrome.storage.sync.get.callsArgWith(1, {});
  return this.butterBar.init().then(() => {
    assert.ok(this.butterBar.root_.hidden == false);
    assert.ok(this.butterBar.dismiss_.hidden == false);
    assert.ok(!this.butterBar.root_.classList.contains('red'));
  });
});

QUnit.test('should update storage when shown', function(assert) {
  this.butterBar.currentMessage_ = 0;
  this.clock = sinon.useFakeTimers(123);
  chrome.storage.sync.get.callsArgWith(1, {});
  return this.butterBar.init().then(() => {
    assert.deepEqual(chrome.storage.sync.set.firstCall.args,
                     [{
                       "message-state": {
                         "hidden": false,
                         "index": 0,
                         "timestamp": 123,
                       }
                     }]);
  });
});

QUnit.test(
    'should be shown and should not update local storage if it has already ' +
    'shown, the timeout has not elapsed and it has not been dismissed',
    function(assert) {
  this.butterBar.currentMessage_ = 0;
  chrome.storage.sync.get.callsArgWith(1, {
    "message-state": {
      "hidden": false,
      "index": 0,
      "timestamp": 0,
    }
  });
  this.clock = sinon.useFakeTimers(remoting.ButterBar.kTimeout_);
  return this.butterBar.init().then(() => {
    assert.ok(this.butterBar.root_.hidden == false);
    assert.ok(!chrome.storage.sync.set.called);
  });
});

QUnit.test('should stay hidden if the timeout has elapsed', function(assert) {
  this.butterBar.currentMessage_ = 0;
  chrome.storage.sync.get.callsArgWith(1, {
    "message-state": {
      "hidden": false,
      "index": 0,
      "timestamp": 0,
    }
  });
  this.clock = sinon.useFakeTimers(remoting.ButterBar.kTimeout_+ 1);
  return this.butterBar.init().then(() => {
    assert.ok(this.butterBar.root_.hidden == true);
  });
});


QUnit.test('should stay hidden if it was previously dismissed',
           function(assert) {
  this.butterBar.currentMessage_ = 0;
  chrome.storage.sync.get.callsArgWith(1, {
    "message-state": {
      "hidden": true,
      "index": 0,
      "timestamp": 0,
    }
  });
  this.clock = sinon.useFakeTimers(0);
  return this.butterBar.init().then(() => {
    assert.ok(this.butterBar.root_.hidden == true);
  });
});


QUnit.test('should be shown if the index has increased', function(assert) {
  this.butterBar.currentMessage_ = 1;
  chrome.storage.sync.get.callsArgWith(1, {
    "message-state": {
      "hidden": true,
      "index": 0,
      "timestamp": 0,
    }
  });
  this.clock = sinon.useFakeTimers(remoting.ButterBar.kTimeout_ + 1);
  return this.butterBar.init().then(() => {
    assert.ok(this.butterBar.root_.hidden == false);
  });
});

QUnit.test('should be red and not dismissable for the final message',
           function(assert) {
  this.butterBar.currentMessage_ = 3;
  chrome.storage.sync.get.callsArgWith(1, {});
  return this.butterBar.init().then(() => {
    assert.ok(this.butterBar.root_.hidden == false);
    assert.ok(this.butterBar.dismiss_.hidden == true);
    assert.ok(this.butterBar.root_.classList.contains('red'));
  });
});

QUnit.test('dismiss button updates local storage', function(assert) {
  this.butterBar.currentMessage_ = 0;
  chrome.storage.sync.get.callsArgWith(1, {});
  return this.butterBar.init().then(() => {
    this.clock = sinon.useFakeTimers(0);
    this.butterBar.dismiss_.click();
    // The first call is in response to showing the message; the second is in
    // response to dismissing the message.
    assert.deepEqual(chrome.storage.sync.set.secondCall.args,
                     [{
                       "message-state": {
                         "hidden": true,
                         "index": 0,
                         "timestamp": 0,
                       }
                     }]);
  });
});

}());
