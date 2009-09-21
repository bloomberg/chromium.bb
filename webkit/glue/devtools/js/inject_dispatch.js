// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Injects 'injected' object into the inspectable page.
 */


var InspectorControllerDispatcher = {};


/**
 * Main dispatch method, all calls from the host to InspectorController go
 * through this one.
 * @param {string} functionName Function to call
 * @param {string} json_args JSON-serialized call parameters.
 * @return {string} JSON-serialized result of the dispatched call.
 */
InspectorControllerDispatcher.dispatch = function(functionName, json_args) {
  var params = JSON.parse(json_args);
  InspectorController[functionName].apply(InspectorController, params);
};


/**
 * This is called by the InspectorFrontend for serialization.
 * We serialize the call and send it to the client over the IPC
 * using dispatchOut bound method.
 */
function dispatch(method, var_args) {
  // Handle all messages with non-primitieve arguments here.
  var args = Array.prototype.slice.call(arguments);

  if (method == 'inspectedWindowCleared' ||
      method == 'reset' ||
      method == 'setAttachedWindow') {
    // Filter out messages we don't need here.
    // We do it on the sender side since they may have non-serializable
    // parameters.
    return;
  }

  var call = JSON.stringify(args);
  DevToolsAgentHost.dispatch(call);
};


// Plugging into upstreamed support.
InjectedScript._window = function() {
  return contentWindow;
};


// Plugging into upstreamed support.
Object.className = function(obj) {
  return (obj == null) ? "null" : obj.constructor.name;
};


/**
 * A no-op function that is called by debugger agent just to trigger v8
 * execution.
 */
function devtools$$void() {
}
