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
  this.installInspectorControllerDelegate_('storeLastActivePanel');
  this.installInspectorControllerDelegate_('highlightDOMNode');
  this.installInspectorControllerDelegate_('hideDOMNodeHighlight');
  this.installInspectorControllerDelegate_('getChildNodes');
  this.installInspectorControllerDelegate_('setAttribute');
  this.installInspectorControllerDelegate_('removeAttribute');
  this.installInspectorControllerDelegate_('setTextNodeValue');
  this.installInspectorControllerDelegate_('enableResourceTracking');
  this.installInspectorControllerDelegate_('disableResourceTracking');

  this.installInjectedScriptDelegate_('getStyles');
  this.installInjectedScriptDelegate_('getComputedStyle');
  this.installInjectedScriptDelegate_('getInlineStyle');
  this.installInjectedScriptDelegate_('applyStyleText');
  this.installInjectedScriptDelegate_('setStyleText');
  this.installInjectedScriptDelegate_('toggleStyleEnabled');
  this.installInjectedScriptDelegate_('applyStyleRuleText');
  this.installInjectedScriptDelegate_('addStyleSelector');
  this.installInjectedScriptDelegate_('setStyleProperty');
  this.installInjectedScriptDelegate_('getPrototypes');
  this.installInjectedScriptDelegate_('setPropertyValue');
  this.installInjectedScriptDelegate_('evaluate');
  this.installInjectedScriptDelegate_('addInspectedNode');
  this.installInjectedScriptDelegate_('pushNodeToFrontend');
  this.installInjectedScriptDelegate_('performSearch');
  this.installInjectedScriptDelegate_('searchCanceled');
  this.installInjectedScriptDelegate_('openInInspectedWindow');
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
  return 'databases';
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
  DevToolsHost.toggleInspectElementMode(this.searchingForNode());
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
  DevToolsHost.addResourceSourceToFrame(identifier, resource.mimeType, element);
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
    sourceID, line) {
  devtools.tools.getDebuggerAgent().addBreakpoint(sourceID, line);
};


devtools.InspectorControllerImpl.prototype.removeBreakpoint = function(
    sourceID, line) {
  devtools.tools.getDebuggerAgent().removeBreakpoint(sourceID, line);
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
devtools.InspectorControllerImpl.prototype.evaluateInCallFrame =
    function(callFrameId, code, callback) {
  devtools.tools.getDebuggerAgent().evaluateInCallFrame(callFrameId, code,
                                                        callback);
};


/**
 * @override
 */
devtools.InspectorControllerImpl.prototype.getProperties = function(
    objectProxy, ignoreHasOwnProperty, callback) {
  if (objectProxy.isScope) {
    devtools.tools.getDebuggerAgent().resolveScope(objectProxy.objectId,
        callback);
  } else if (objectProxy.isV8Ref) {
    devtools.tools.getDebuggerAgent().resolveChildren(objectProxy.objectId,
        callback, true);
  } else {
    this.callInjectedScript_('getProperties', objectProxy,
        ignoreHasOwnProperty, callback);
  }
};


/**
 * Installs delegating handler into the inspector controller.
 * @param {string} methodName Method to install delegating handler for.
 */
devtools.InspectorControllerImpl.prototype.installInjectedScriptDelegate_ =
    function(methodName) {
  this[methodName] = goog.bind(this.callInjectedScript_, this,
      methodName);
};


/**
 * Bound function with the installInjectedScriptDelegate_ actual
 * implementation.
 */
devtools.InspectorControllerImpl.prototype.callInjectedScript_ =
    function(methodName, var_arg) {
  var allArgs = Array.prototype.slice.call(arguments);
  var callback = allArgs[allArgs.length - 1];
  var args = Array.prototype.slice.call(allArgs, 0, allArgs.length - 1);
  RemoteToolsAgent.ExecuteUtilityFunction(
      devtools.InspectorControllerImpl.parseWrap_(callback),
      'InjectedScript', JSON.stringify(args));
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
  var args = Array.prototype.slice.call(arguments);
  RemoteToolsAgent.ExecuteUtilityFunction(
      devtools.Callback.wrap(function(){}),
      'InspectorController', JSON.stringify(args));
};


devtools.InspectorControllerImpl.parseWrap_ = function(callback) {
  return devtools.Callback.wrap(
      function(data) {
        callback.call(this, JSON.parse(data));
      });
};


InspectorController = new devtools.InspectorControllerImpl();
