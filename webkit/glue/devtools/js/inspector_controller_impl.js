// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview DevTools' implementation of the InspectorController API.
 */
goog.require('devtools.InspectorController');

goog.provide('devtools.InspectorControllerImpl');

devtools.InspectorControllerImpl = function() {
  devtools.InspectorController.call(this);
  this.frame_element_id_ = 1;

  this.installInspectorControllerDelegate_('clearMessages');
  this.installInspectorControllerDelegate_('copyNode');
  this.installInspectorControllerDelegate_('deleteCookie');
  this.installInspectorControllerDelegate_('disableResourceTracking');
  this.installInspectorControllerDelegate_('disableTimeline');
  this.installInspectorControllerDelegate_('enableResourceTracking');
  this.installInspectorControllerDelegate_('enableTimeline');
  this.installInspectorControllerDelegate_('getChildNodes');
  this.installInspectorControllerDelegate_('getCookies');
  this.installInspectorControllerDelegate_('getDatabaseTableNames');
  this.installInspectorControllerDelegate_('getDOMStorageEntries');
  this.installInspectorControllerDelegate_('getEventListenersForNode');
  this.installInspectorControllerDelegate_('highlightDOMNode');
  this.installInspectorControllerDelegate_('hideDOMNodeHighlight');
  this.installInspectorControllerDelegate_('releaseWrapperObjectGroup');
  this.installInspectorControllerDelegate_('removeAttribute');
  this.installInspectorControllerDelegate_('setAttribute');
  this.installInspectorControllerDelegate_('setDOMStorageItem');
  this.installInspectorControllerDelegate_('setSetting');
  this.installInspectorControllerDelegate_('setTextNodeValue');
  this.installInspectorControllerDelegate_('setting');
  this.installInspectorControllerDelegate_('startTimelineProfiler');
  this.installInspectorControllerDelegate_('stopTimelineProfiler');
  this.installInspectorControllerDelegate_('storeLastActivePanel');
};
goog.inherits(devtools.InspectorControllerImpl,
    devtools.InspectorController);


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.platform = function() {
  return DevToolsHost.getPlatform();
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.closeWindow = function() {
  DevToolsHost.closeWindow();
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.attach = function() {
  DevToolsHost.dockWindow();
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.detach = function() {
  DevToolsHost.undockWindow();
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.hiddenPanels = function() {
  return DevToolsHost.hiddenPanels();
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.search = function(sourceRow, query) {
  return DevToolsHost.search(sourceRow, query);
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.toggleNodeSearch = function() {
  devtools.InspectorController.prototype.toggleNodeSearch.call(this);
  this.callInspectorController_.call(this, 'toggleNodeSearch');
  if (!this.searchingForNode()) {
    // This is called from ElementsPanel treeOutline's focusNodeChanged().
    DevToolsHost.activateWindow();
  }
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.localizedStringsURL =
    function(opt_prefix) {
  // l10n is turned off in test mode because delayed loading of strings
  // causes test failures.
  if (false) {
    var locale = DevToolsHost.getApplicationLocale();
    locale = locale.replace('_', '-');
    return 'l10n/localizedStrings_' + locale + '.js';
  } else {
    return undefined;
  }
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.addSourceToFrame =
    function(mimeType, source, element) {
  return DevToolsHost.addSourceToFrame(mimeType, source, element);
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.addResourceSourceToFrame =
    function(identifier, element) {
  var resource = WebInspector.resources[identifier];
  if (!resource) {
    return;
  }

  // Temporary fix for http://crbug/23260.
  var mimeType = resource.mimeType;
  if (!mimeType && resource.url) {
    if (resource.url.search('\.js$') != -1) {
      mimeType = 'application/x-javascript';
    } else if (resource.url.search('\.html$') != -1) {
      mimeType = 'text/html';
    }
  }

  DevToolsHost.addResourceSourceToFrame(identifier, mimeType, element);
};


/**
 * {@inheritDoc}.
 */
devtools.InspectorControllerImpl.prototype.inspectedWindow = function() {
  return null;
};


/**
 * @override
 */
devtools.InspectorControllerImpl.prototype.debuggerEnabled = function() {
  return true;
};


devtools.InspectorControllerImpl.prototype.addBreakpoint = function(
    sourceID, line, condition) {
  devtools.tools.getDebuggerAgent().addBreakpoint(sourceID, line, condition);
};


devtools.InspectorControllerImpl.prototype.removeBreakpoint = function(
    sourceID, line) {
  devtools.tools.getDebuggerAgent().removeBreakpoint(sourceID, line);
};

devtools.InspectorControllerImpl.prototype.updateBreakpoint = function(
    sourceID, line, condition) {
  devtools.tools.getDebuggerAgent().updateBreakpoint(
      sourceID, line, condition);
};

devtools.InspectorControllerImpl.prototype.pauseInDebugger = function() {
  devtools.tools.getDebuggerAgent().pauseExecution();
};


devtools.InspectorControllerImpl.prototype.resumeDebugger = function() {
  devtools.tools.getDebuggerAgent().resumeExecution();
};


devtools.InspectorControllerImpl.prototype.stepIntoStatementInDebugger =
    function() {
  devtools.tools.getDebuggerAgent().stepIntoStatement();
};


devtools.InspectorControllerImpl.prototype.stepOutOfFunctionInDebugger =
    function() {
  devtools.tools.getDebuggerAgent().stepOutOfFunction();
};


devtools.InspectorControllerImpl.prototype.stepOverStatementInDebugger =
    function() {
  devtools.tools.getDebuggerAgent().stepOverStatement();
};


/**
 * @override
 */
devtools.InspectorControllerImpl.prototype.pauseOnExceptions = function() {
  return devtools.tools.getDebuggerAgent().pauseOnExceptions();
};


/**
 * @override
 */
devtools.InspectorControllerImpl.prototype.setPauseOnExceptions = function(
    value) {
  return devtools.tools.getDebuggerAgent().setPauseOnExceptions(value);
};


/**
 * @override
 */
devtools.InspectorControllerImpl.prototype.startProfiling = function() {
  devtools.tools.getDebuggerAgent().startProfiling(
      devtools.DebuggerAgent.ProfilerModules.PROFILER_MODULE_CPU);
};


/**
 * @override
 */
devtools.InspectorControllerImpl.prototype.stopProfiling = function() {
  devtools.tools.getDebuggerAgent().stopProfiling(
      devtools.DebuggerAgent.ProfilerModules.PROFILER_MODULE_CPU);
};


/**
 * @override
 */
devtools.InspectorControllerImpl.prototype.getProfileHeaders = function(callId) {
  WebInspector.didGetProfileHeaders(callId, []);
};


/**
 * Emulate WebKit InspectorController behavior. It stores profiles on renderer side,
 * and is able to retrieve them by uid using 'getProfile'.
 */
devtools.InspectorControllerImpl.prototype.addFullProfile = function(profile) {
  WebInspector.__fullProfiles = WebInspector.__fullProfiles || {};
  WebInspector.__fullProfiles[profile.uid] = profile;
};


/**
 * @override
 */
devtools.InspectorControllerImpl.prototype.getProfile = function(callId, uid) {
  if (WebInspector.__fullProfiles && (uid in WebInspector.__fullProfiles)) {
    WebInspector.didGetProfile(callId, WebInspector.__fullProfiles[uid]);
  }
};


/**
 * @override
 */
devtools.InspectorControllerImpl.prototype.takeHeapSnapshot = function() {
  devtools.tools.getDebuggerAgent().startProfiling(
      devtools.DebuggerAgent.ProfilerModules.PROFILER_MODULE_HEAP_SNAPSHOT
      | devtools.DebuggerAgent.ProfilerModules.PROFILER_MODULE_HEAP_STATS
      | devtools.DebuggerAgent.ProfilerModules.PROFILER_MODULE_JS_CONSTRUCTORS);
};


/**
 * @override
 */
devtools.InspectorControllerImpl.prototype.dispatchOnInjectedScript = function(
    callId, methodName, argsString, async) {
  var callback = function(result, isException) {
    WebInspector.didDispatchOnInjectedScript(callId, result, isException);
  };
  RemoteToolsAgent.DispatchOnInjectedScript(
      WebInspector.Callback.wrap(callback),
      async ? methodName + "_async" : methodName,
      argsString);
};


/**
 * Installs delegating handler into the inspector controller.
 * @param {string} methodName Method to install delegating handler for.
 */
devtools.InspectorControllerImpl.prototype.installInspectorControllerDelegate_
    = function(methodName) {
  this[methodName] = goog.bind(this.callInspectorController_, this,
      methodName);
};


/**
 * Bound function with the installInjectedScriptDelegate_ actual
 * implementation.
 */
devtools.InspectorControllerImpl.prototype.callInspectorController_ =
    function(methodName, var_arg) {
  var args = Array.prototype.slice.call(arguments, 1);
  RemoteToolsAgent.DispatchOnInspectorController(
      WebInspector.Callback.wrap(function(){}),
      methodName,
      JSON.stringify(args));
};


InspectorController = new devtools.InspectorControllerImpl();
