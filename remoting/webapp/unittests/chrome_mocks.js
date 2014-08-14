// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains various mock objects for the chrome platform to make
// unit testing easier.

(function(scope){

var chromeMocks = {};

chromeMocks.Event = function() {
  this.listeners_ = [];
};

chromeMocks.Event.prototype.addListener = function(callback) {
  this.listeners_.push(callback);
};

chromeMocks.Event.prototype.removeListener = function(callback) {
  for (var i = 0; i < this.listeners_.length; i++) {
    if (this.listeners_[i] === callback) {
      this.listeners_.splice(i, 1);
      break;
    }
  }
};

chromeMocks.Event.prototype.mock$fire = function(data) {
  this.listeners_.forEach(function(listener){
    listener(data);
  });
};

chromeMocks.runtime = {};

chromeMocks.runtime.Port = function() {
  this.onMessage = new chromeMocks.Event();
  this.onDisconnect = new chromeMocks.Event();

  this.name = '';
  this.sender = null;
};

chromeMocks.runtime.Port.prototype.disconnect = function() {};
chromeMocks.runtime.Port.prototype.postMessage = function() {};

scope.chromeMocks = chromeMocks;

})(window);
