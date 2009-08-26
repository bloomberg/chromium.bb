// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Injects 'injected' object into the inspectable page.
 */

/**
 * Dispatches host calls into the injected function calls.
 */
goog.require('devtools.Injected');


/**
 * Injected singleton.
 */
var devtools$$obj = new devtools.Injected();


/**
 * Main dispatch method, all calls from the host go through this one.
 * @param {string} functionName Function to call
 * @param {string} json_args JSON-serialized call parameters.
 * @return {string} JSON-serialized result of the dispatched call.
 */
function devtools$$dispatch(functionName, json_args) {
  var params = JSON.parse(json_args);
  var result = devtools$$obj[functionName].apply(devtools$$obj, params);
  return JSON.stringify(result);
}


/**
 * Removes malicious functions from the objects so that the pure JSON.stringify
 * was used.
 */
function sanitizeJson(obj) {
  for (var name in obj) {
    var property = obj[name];
    var type = typeof property;
    if (type === "function") {
      obj[name] = null;
    } else if (obj !== null && type === "object") {
      sanitizeJson(property);
    }
  }
  return obj;
}


/**
 * This is called by the InspectorFrontend for serialization.
 * We serialize the call and send it to the client over the IPC
 * using dispatchOut bound method.
 */
var dispatch = function(method, var_args) {
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

  var call = JSON.stringify(sanitizeJson(args));
  DevToolsAgentHost.dispatch(call);
};
