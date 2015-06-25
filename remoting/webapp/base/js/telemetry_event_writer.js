// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @suppress {duplicate} */
var remoting = remoting || {};

(function() {

'use strict';

remoting.TelemetryEventWriter = function() {};

/** @enum {string} */
var IpcNames = {
  WRITE: 'remoting.TelemetryEventWriter.write'
};

/**
 * @param {base.Ipc} ipc
 * @param {remoting.XhrEventWriter} eventWriter
 * @constructor
 * @implements {base.Disposable}
 */
remoting.TelemetryEventWriter.Service = function(ipc, eventWriter) {
  /** @private */
  this.eventWriter_ = eventWriter;
  /** @private */
  this.ipc_ = ipc;
  /** @private {base.Disposables} */
  this.eventHooks_ = new base.Disposables();
};

/** @return {Promise} */
remoting.TelemetryEventWriter.Service.prototype.init = function() {
  /** @this {remoting.TelemetryEventWriter.Service} */
  function init() {
    this.eventHooks_.add(
        new base.DomEventHook(window, 'online',
                              this.eventWriter_.flush.bind(this.eventWriter_),
                              false),
        new base.ChromeEventHook(chrome.runtime.onSuspend,
                                 this.onSuspend_.bind(this)));

    this.ipc_.register(IpcNames.WRITE, this.write_.bind(this));
    this.eventWriter_.flush();
  }
  // Only listen for new incoming requests after we have loaded the pending
  // ones.  This will ensure that we always process the requests in order.
  return this.eventWriter_.loadPendingRequests().then(init.bind(this));
};

remoting.TelemetryEventWriter.Service.prototype.dispose = function() {
  this.ipc_.unregister(IpcNames.WRITE);
  base.dispose(this.eventHooks_);
  this.eventHooks_ = null;
};

/**
 * Unbind any sessions that are associated with |windowId|.
 * @param {string} windowId
 */
remoting.TelemetryEventWriter.Service.prototype.unbindSession =
  function(windowId) {};

/**
 * @param {!Object} event  The event to be written to the server.
 */
remoting.TelemetryEventWriter.Service.prototype.write_ = function(event) {
  this.eventWriter_.write(event);
};

/**
 * @private
 */
remoting.TelemetryEventWriter.Service.prototype.onSuspend_ = function() {
  this.eventWriter_.writeToStorage();
};

remoting.TelemetryEventWriter.Client = function() {};

/**
 * @param {!Object} event
 * @return {Promise} A promise that resolves when the log message is sent to the
 *     logging service.
 */
remoting.TelemetryEventWriter.Client.write = function(event) {
  return base.Ipc.invoke(IpcNames.WRITE, event);
};

})();
