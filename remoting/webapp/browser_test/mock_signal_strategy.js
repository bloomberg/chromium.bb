// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * @suppress {checkTypes}
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 * @implements {remoting.SignalStrategy}
 */
remoting.MockSignalStrategy = function() {};

remoting.MockSignalStrategy.prototype.dispose = function() {};

remoting.MockSignalStrategy.prototype.setIncomingStanzaCallback =
    function(onIncomingStanzaCallback) {};

remoting.MockSignalStrategy.prototype.connect =
    function(server, username, authToken) {};

remoting.MockSignalStrategy.prototype.sendMessage = function(message) {};

remoting.MockSignalStrategy.prototype.getState = function() {
  return remoting.SignalStrategy.State.CONNECTED;
};

remoting.MockSignalStrategy.prototype.getError = function() {
  return remoting.Error.NONE;
};

remoting.MockSignalStrategy.prototype.getJid = function() {
  return '';
};
