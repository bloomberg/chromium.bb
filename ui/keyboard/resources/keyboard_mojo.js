// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
var connection;
var mojo_api;
var input_focused_event;

if (!chrome.virtualKeyboardPrivate) {
  define('main', [
      'mojo/public/js/bindings/connection',
      'ui/keyboard/webui/keyboard.mojom',
      'content/public/renderer/service_provider',
  ], function(connector, keyboard, serviceProvider) {
    'use strict';
    function KeyboardImpl(kbd) {
      console.log('Creating KeyboardImpl');
      this.keyboard_ = kbd;
      mojo_api = this;
    }

    KeyboardImpl.prototype = Object.create(keyboard.KeyboardAPIStub.prototype);

    KeyboardImpl.prototype.onTextInputTypeChanged = function(input_type) {
      console.log('Text input changed: ' + input_type);
      input_focused_event.forEach(function(listener) {
        listener({type: input_type});
      });
    };

    return function() {
      connection = new connector.Connection(
          serviceProvider.connectToService(
              keyboard.KeyboardUIHandlerMojoProxy.NAME_),
          KeyboardImpl,
          keyboard.KeyboardUIHandlerMojoProxy);
    };
  });

  chrome.virtualKeyboardPrivate = {};
  chrome.virtualKeyboardPrivate.sendKeyEvent = function(event) {
    if (!mojo_api)
      return;
    console.log('sending key event: ' + event.type);
    mojo_api.keyboard_.sendKeyEvent(event.type,
                               event.charValue,
                               event.keyCode,
                               event.keyName,
                               event.modifiers);
  };
  chrome.virtualKeyboardPrivate.hideKeyboard = function() {
    if (!mojo_api)
      return;
    mojo_api.keyboard_.hideKeyboard();
  };
  chrome.virtualKeyboardPrivate.moveCursor = function() {};
  chrome.virtualKeyboardPrivate.lockKeyboard = function() {};
  chrome.virtualKeyboardPrivate.keyboardLoaded = function() {};
  chrome.virtualKeyboardPrivate.getKeyboardConfig = function() {};

  function BrowserEvent() {
    this.listeners_ = [];
  };

  BrowserEvent.prototype.addListener = function(callback) {
    this.listeners_.push(callback);
  };

  BrowserEvent.prototype.forEach = function(callback) {
    for (var i = 0; i < this.listeners_.length; ++i)
      callback(this.listeners_[i]);
  };

  input_focused_event = new BrowserEvent;
  chrome.virtualKeyboardPrivate.onTextInputBoxFocused = input_focused_event;
}
