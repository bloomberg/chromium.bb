// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {

'use strict';

var eventSource = null;
var domElement = null;
var myChromeEvent = null;
var listener = null;

var Listener = function(element) {
  this.onChromeEvent = sinon.stub();
  this.onClickEvent = sinon.stub();
  this.onCustomEvent = sinon.stub();
  this.eventHooks_ = new base.Disposables(
      new base.DomEventHook(element, 'click', this.onClickEvent.bind(this),
                            false),
      new base.EventHook(eventSource, 'customEvent',
                         this.onCustomEvent.bind(this)),
      new base.ChromeEventHook(myChromeEvent, this.onChromeEvent.bind(this)));
};

Listener.prototype.dispose = function() {
  this.eventHooks_.dispose();
};

function raiseAllEvents() {
  domElement.click();
  myChromeEvent.mock$fire();
  eventSource.raiseEvent('customEvent');
}

module('base.EventHook', {
  setup: function() {
    domElement = document.createElement('div');
    eventSource = new base.EventSourceImpl();
    eventSource.defineEvents(['customEvent']);
    myChromeEvent = new chromeMocks.Event();
    listener = new Listener(domElement);
  },
  tearDown: function() {
    domElement = null;
    eventSource = null;
    myChromeEvent = null;
    listener = null;
  }
});

test('EventHook should hook events when constructed', function() {
  raiseAllEvents();
  sinon.assert.calledOnce(listener.onClickEvent);
  sinon.assert.calledOnce(listener.onChromeEvent);
  sinon.assert.calledOnce(listener.onCustomEvent);
  listener.dispose();
});

test('EventHook should unhook events when disposed', function() {
  listener.dispose();
  raiseAllEvents();
  sinon.assert.notCalled(listener.onClickEvent);
  sinon.assert.notCalled(listener.onChromeEvent);
  sinon.assert.notCalled(listener.onCustomEvent);
});

})();
