// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides communication interface to remote v8 profiler.
 */
goog.provide('devtools.ProfilerAgent');

/**
 * @constructor
 */
devtools.ProfilerAgent = function() {
  RemoteProfilerAgent.DidGetActiveProfilerModules =
      goog.bind(this.didGetActiveProfilerModules_, this);
  RemoteProfilerAgent.DidGetLogLines =
      goog.bind(this.didGetLogLines_, this);

  /**
   * Active profiler modules flags.
   * @type {number}
   */
  this.activeProfilerModules_ =
      devtools.ProfilerAgent.ProfilerModules.PROFILER_MODULE_NONE;

  /**
   * Interval for polling profiler state.
   * @type {number}
   */
  this.getActiveProfilerModulesInterval_ = null;

  /**
   * Profiler log position.
   * @type {number}
   */
  this.logPosition_ = 0;

  /**
   * Whether log contents retrieval must be forced next time.
   * @type {boolean}
   */
  this.forceGetLogLines_ = false;

  /**
   * Profiler processor instance.
   * @type {devtools.profiler.Processor}
   */
  this.profilerProcessor_ = new devtools.profiler.Processor();
};


/**
 * A copy of enum from include/v8.h
 * @enum {number}
 */
devtools.ProfilerAgent.ProfilerModules = {
  PROFILER_MODULE_NONE: 0,
  PROFILER_MODULE_CPU: 1,
  PROFILER_MODULE_HEAP_STATS: 1 << 1,
  PROFILER_MODULE_JS_CONSTRUCTORS: 1 << 2,
  PROFILER_MODULE_HEAP_SNAPSHOT: 1 << 16
};


/**
 * Resets profiler agent to its initial state.
 */
devtools.ProfilerAgent.prototype.reset = function() {
  this.logPosition_ = 0;
  this.activeProfilerModules_ =
      devtools.ProfilerAgent.ProfilerModules.PROFILER_MODULE_NONE;
};


/**
 * Sets up callbacks that deal with profiles processing.
 */
devtools.ProfilerAgent.prototype.setupProfilerProcessorCallbacks = function() {
  // A temporary icon indicating that the profile is being processed.
  var processingIcon = new WebInspector.SidebarTreeElement(
      'profile-sidebar-tree-item',
      WebInspector.UIString('Processing...'),
      '', null, false);
  var profilesSidebar = WebInspector.panels.profiles.getProfileType(
      WebInspector.CPUProfileType.TypeId).treeElement;

  this.profilerProcessor_.setCallbacks(
      function onProfileProcessingStarted() {
        // Set visually empty string. Subtitle hiding is done via styles
        // manipulation which doesn't play well with dynamic append / removal.
        processingIcon.subtitle = ' ';
        profilesSidebar.appendChild(processingIcon);
      },
      function onProfileProcessingStatus(ticksCount) {
        processingIcon.subtitle =
            WebInspector.UIString('%d ticks processed', ticksCount);
      },
      function onProfileProcessingFinished(profile) {
        profilesSidebar.removeChild(processingIcon);
        profile.typeId = WebInspector.CPUProfileType.TypeId;
        InspectorBackend.addFullProfile(profile);
        WebInspector.addProfileHeader(profile);
        // If no profile is currently shown, show the new one.
        var profilesPanel = WebInspector.panels.profiles;
        if (!profilesPanel.visibleView) {
          profilesPanel.showProfile(profile);
        }
      }
  );
};


/**
 * Initializes profiling state.
 */
devtools.ProfilerAgent.prototype.initializeProfiling = function() {
  this.setupProfilerProcessorCallbacks();
  this.forceGetLogLines_ = true;
  this.getActiveProfilerModulesInterval_ = setInterval(
        function() { RemoteProfilerAgent.GetActiveProfilerModules(); }, 1000);
};


/**
 * Starts profiling.
 * @param {number} modules List of modules to enable.
 */
devtools.ProfilerAgent.prototype.startProfiling = function(modules) {
  var cmd = new devtools.DebugCommand('profile', {
      'modules': modules,
      'command': 'resume'});
  devtools.DebuggerAgent.sendCommand_(cmd);
  RemoteToolsAgent.ExecuteVoidJavaScript();
  if (modules &
      devtools.ProfilerAgent.ProfilerModules.PROFILER_MODULE_HEAP_SNAPSHOT) {
    var pos = this.logPosition_;
    // Active modules will not change, instead, a snapshot will be logged.
    setTimeout(function() { RemoteProfilerAgent.GetLogLines(pos); }, 500);
  }
};


/**
 * Stops profiling.
 */
devtools.ProfilerAgent.prototype.stopProfiling = function(modules) {
  var cmd = new devtools.DebugCommand('profile', {
      'modules': modules,
      'command': 'pause'});
  devtools.DebuggerAgent.sendCommand_(cmd);
  RemoteToolsAgent.ExecuteVoidJavaScript();
};


/**
 * Handles current profiler status.
 * @param {number} modules List of active (started) modules.
 */
devtools.ProfilerAgent.prototype.didGetActiveProfilerModules_ = function(
    modules) {
  var profModules = devtools.ProfilerAgent.ProfilerModules;
  var profModuleNone = profModules.PROFILER_MODULE_NONE;
  if (this.forceGetLogLines_ ||
      (modules != profModuleNone &&
      this.activeProfilerModules_ == profModuleNone)) {
    this.forceGetLogLines_ = false;
    // Start to query log data.
    RemoteProfilerAgent.GetLogLines(this.logPosition_);
  }
  this.activeProfilerModules_ = modules;
  // Update buttons.
  WebInspector.setRecordingProfile(modules & profModules.PROFILER_MODULE_CPU);
};


/**
 * Handles a portion of a profiler log retrieved by GetLogLines call.
 * @param {number} pos Current position in log.
 * @param {string} log A portion of profiler log.
 */
devtools.ProfilerAgent.prototype.didGetLogLines_ = function(pos, log) {
  this.logPosition_ = pos;
  if (log.length > 0) {
    this.profilerProcessor_.processLogChunk(log);
  } else if (this.activeProfilerModules_ ==
      devtools.ProfilerAgent.ProfilerModules.PROFILER_MODULE_NONE) {
    // No new data and profiling is stopped---suspend log reading.
    return;
  }
  setTimeout(function() { RemoteProfilerAgent.GetLogLines(pos); }, 500);
};
