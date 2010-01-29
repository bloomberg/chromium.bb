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
  InspectorBackend[functionName].apply(InspectorBackend, params);
};

/**
 * Special controller object for APU related messages. Outgoing messages
 * are sent to this object if the ApuAgentDispatcher is enabled.
 **/
var ApuAgentDispatcher = { enabled : false };

/**
 * Dispatches messages to APU. This filters and transforms
 * outgoing messages that are used by APU.
 * @param {string} method name of the dispatch method.
 **/
ApuAgentDispatcher.dispatchToApu = function(method, args) {
  if (method != 'addRecordToTimeline' &&
      method != 'updateResource' &&
      method != 'addResource') {
    return;
  }
  // TODO(knorton): Transform args so they can be used
  // by APU.
  DevToolsAgentHost.dispatchToApu(JSON.stringify(args));
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

  // Sniff some inspector controller state changes in order to support
  // cross-navigation instrumentation. Keep names in sync with
  // webdevtoolsagent_impl.
  if (method == 'timelineProfilerWasStarted')
    DevToolsAgentHost.runtimeFeatureStateChanged('timeline-profiler', true);
  else if (method == 'timelineProfilerWasStopped')
    DevToolsAgentHost.runtimeFeatureStateChanged('timeline-profiler', false);
  else if (method == 'resourceTrackingWasEnabled')
    DevToolsAgentHost.runtimeFeatureStateChanged('resource-tracking', true);
  else if (method == 'resourceTrackingWasDisabled')
    DevToolsAgentHost.runtimeFeatureStateChanged('resource-tracking', false);

  if (ApuAgentDispatcher.enabled) {
    ApuAgentDispatcher.dispatchToApu(method, args);
    return;
  }

  var call = JSON.stringify(args);
  DevToolsAgentHost.dispatch(call);
};

/**
 * A no-op function that is called by debugger agent just to trigger v8
 * execution.
 */
function devtools$$void() {
}
