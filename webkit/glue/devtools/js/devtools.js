// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Tools is a main class that wires all components of the
 * DevTools frontend together. It is also responsible for overriding existing
 * WebInspector functionality while it is getting upstreamed into WebCore.
 */
goog.provide('devtools.Tools');

goog.require('devtools.DebuggerAgent');


/**
 * Dispatches raw message from the host.
 * @param {string} remoteName
 * @prama {string} methodName
 * @param {Object} msg Message to dispatch.
 */
devtools.dispatch = function(remoteName, methodName, msg) {
  remoteName = 'Remote' + remoteName.substring(0, remoteName.length - 8);
  var agent = window[remoteName];
  if (!agent) {
    debugPrint('No remote agent "' + remoteName + '" found.');
    return;
  }
  var method = agent[methodName];
  if (!method) {
    debugPrint('No method "' + remoteName + '.' + methodName + '" found.');
    return;
  }
  method.apply(this, msg);
};


devtools.ToolsAgent = function() {
  RemoteToolsAgent.DidExecuteUtilityFunction =
      devtools.Callback.processCallback;
  RemoteToolsAgent.FrameNavigate =
      goog.bind(this.frameNavigate_, this);
  RemoteToolsAgent.DispatchOnClient =
      goog.bind(this.dispatchOnClient_, this);
  RemoteToolsAgent.SetResourcesPanelEnabled =
      goog.bind(this.setResourcesPanelEnabled_, this);
  this.debuggerAgent_ = new devtools.DebuggerAgent();
};


/**
 * Resets tools agent to its initial state.
 */
devtools.ToolsAgent.prototype.reset = function() {
  DevToolsHost.reset();
  this.debuggerAgent_.reset();
};


/**
 * @param {string} script Script exression to be evaluated in the context of the
 *     inspected page.
 * @param {function(Object|string, boolean):undefined} opt_callback Function to call
 *     with the result.
 */
devtools.ToolsAgent.prototype.evaluateJavaScript = function(script,
    opt_callback) {
  InspectorController.evaluate(script, opt_callback || function() {});
};


/**
 * @return {devtools.DebuggerAgent} Debugger agent instance.
 */
devtools.ToolsAgent.prototype.getDebuggerAgent = function() {
  return this.debuggerAgent_;
};


/**
 * @param {string} url Url frame navigated to.
 * @see tools_agent.h
 * @private
 */
devtools.ToolsAgent.prototype.frameNavigate_ = function(url) {
  this.reset();
  // Do not reset Profiles panel.
  var profiles = null;
  if ('profiles' in WebInspector.panels) {
    profiles = WebInspector.panels['profiles'];
    delete WebInspector.panels['profiles'];
  }
  WebInspector.reset();
  if (profiles != null) {
    WebInspector.panels['profiles'] = profiles;
  }
};


/**
 * @param {string} message Serialized call to be dispatched on WebInspector.
 * @private
 */
devtools.ToolsAgent.prototype.dispatchOnClient_ = function(message) {
  WebInspector.dispatch.apply(WebInspector, JSON.parse(message));
};


/**
 * Evaluates js expression.
 * @param {string} expr
 */
devtools.ToolsAgent.prototype.evaluate = function(expr) {
  RemoteToolsAgent.evaluate(expr);
};


/**
 * Enables / disables resource tracking.
 * @param {boolean} enabled Sets tracking status.
 * @param {boolean} always Determines whether tracking status should be sticky.
 */
devtools.ToolsAgent.prototype.setResourceTrackingEnabled = function(enabled,
    always) {
  RemoteToolsAgent.SetResourceTrackingEnabled(enabled, always);
};


/**
 * Enables / disables resources panel in the ui.
 * @param {boolean} enabled New panel status.
 */
devtools.ToolsAgent.prototype.setResourcesPanelEnabled_ = function(enabled) {
  InspectorController.resourceTrackingEnabled_ = enabled;
  // TODO(pfeldman): Extract this upstream.
  var panel = WebInspector.panels.resources;
  if (enabled) {
    panel.enableToggleButton.title =
        WebInspector.UIString("Resource tracking enabled. Click to disable.");
    panel.enableToggleButton.toggled = true;
    panel.largerResourcesButton.visible = true;
    panel.sortingSelectElement.visible = false;
    panel.panelEnablerView.visible = false;
  } else {
    panel.enableToggleButton.title =
        WebInspector.UIString("Resource tracking disabled. Click to enable.");
    panel.enableToggleButton.toggled = false;
    panel.largerResourcesButton.visible = false;
    panel.sortingSelectElement.visible = false;
    panel.panelEnablerView.visible = true;
  }
};


/**
 * Prints string  to the inspector console or shows alert if the console doesn't
 * exist.
 * @param {string} text
 */
function debugPrint(text) {
  var console = WebInspector.console;
  if (console) {
    console.addMessage(new WebInspector.ConsoleMessage(
        WebInspector.ConsoleMessage.MessageSource.JS,
        WebInspector.ConsoleMessage.MessageType.Log,
        WebInspector.ConsoleMessage.MessageLevel.Log,
        1, 'chrome://devtools/<internal>', undefined, -1, text));
  } else {
    alert(text);
  }
}


/**
 * Global instance of the tools agent.
 * @type {devtools.ToolsAgent}
 */
devtools.tools = null;


var context = {};  // Used by WebCore's inspector routines.


///////////////////////////////////////////////////////////////////////////////
// Here and below are overrides to existing WebInspector methods only.
// TODO(pfeldman): Patch WebCore and upstream changes.
var oldLoaded = WebInspector.loaded;
WebInspector.loaded = function() {
  devtools.tools = new devtools.ToolsAgent();
  devtools.tools.reset();

  Preferences.ignoreWhitespace = false;
  oldLoaded.call(this);

  // Hide dock button on Mac OS.
  // TODO(pfeldman): remove once Mac OS docking is implemented.
  if (InspectorController.platform().indexOf('mac') == 0) {
    document.getElementById('dock-status-bar-item').addStyleClass('hidden');
  }

  // Mute refresh action.
  document.addEventListener("keydown", function(event) {
    if (event.keyIdentifier == 'F5') {
      event.preventDefault();
    } else if (event.keyIdentifier == 'U+0052' /* 'R' */ &&
        (event.ctrlKey || event.metaKey)) {
      event.preventDefault();
    }
  }, true);

  DevToolsHost.loaded();
};


var webkitUpdateChildren =
    WebInspector.ElementsTreeElement.prototype.updateChildren;


/**
 * This override is necessary for adding script source asynchronously.
 * @override
 */
WebInspector.ScriptView.prototype.setupSourceFrameIfNeeded = function() {
  if (!this._frameNeedsSetup) {
    return;
  }

  this.attach();

  if (this.script.source) {
    this.didResolveScriptSource_();
  } else {
    var self = this;
    devtools.tools.getDebuggerAgent().resolveScriptSource(
        this.script.sourceID,
        function(source) {
          self.script.source = source ||
              WebInspector.UIString('<source is not available>');
          self.didResolveScriptSource_();
        });
  }
};


/**
 * Performs source frame setup when script source is aready resolved.
 */
WebInspector.ScriptView.prototype.didResolveScriptSource_ = function() {
  if (!InspectorController.addSourceToFrame(
      "text/javascript", this.script.source, this.sourceFrame.element)) {
    return;
  }

  delete this._frameNeedsSetup;

  this.sourceFrame.addEventListener(
      "syntax highlighting complete", this._syntaxHighlightingComplete, this);
  this.sourceFrame.syntaxHighlightJavascript();
};


/**
 * @param {string} type Type of the the property value('object' or 'function').
 * @param {string} className Class name of the property value.
 * @constructor
 */
WebInspector.UnresolvedPropertyValue = function(type, className) {
  this.type = type;
  this.className = className;
};


/**
 * Replace WebKit method with our own implementation to use our call stack
 * representation. Original method uses Object.prototype.toString.call to
 * learn if scope object is a JSActivation which doesn't work in Chrome.
 */
WebInspector.ScopeChainSidebarPane.prototype.update = function(callFrame) {
  this.bodyElement.removeChildren();

  this.sections = [];
  this.callFrame = callFrame;

  if (!callFrame) {
      var infoElement = document.createElement('div');
      infoElement.className = 'info';
      infoElement.textContent = WebInspector.UIString('Not Paused');
      this.bodyElement.appendChild(infoElement);
      return;
  }

  var scopeChain = callFrame.scopeChain;
  var ScopeType = devtools.DebuggerAgent.ScopeType;
  for (var i = 0; i < scopeChain.length; ++i) {
    var scopeObject = scopeChain[i];
    var thisObject = null;
    var title;
    switch(scopeObject.type) {
      case ScopeType.Global:
        title = WebInspector.UIString('Global');
        break;
      case ScopeType.Local:
        title = WebInspector.UIString('Local');
        thisObject = callFrame.thisObject;
        break;
      case ScopeType.With:
        title = WebInspector.UIString('With Block');
        break;
      case ScopeType.Closure:
        title = WebInspector.UIString('Closure');
        break;
      default:
        title = WebInspector.UIString('<Unknown scope type>');
    }

    var section = new WebInspector.ScopeChainPropertiesSection(
        scopeObject, title, thisObject);
    section.editInSelectedCallFrameWhenPaused = true;
    section.pane = this;

    // Only first scope is expanded by default(if it's not a global one).
    section.expanded = (i == 0) && (scopeObject.type != ScopeType.Global);

    this.sections.push(section);
    this.bodyElement.appendChild(section.element);
  }
};


/**
 * Our basic implementation of ObjectPropertiesSection for debugger object
 * represented in console.
 * @constructor
 */
WebInspector.ConsoleObjectPropertiesSection = function(object, title,
    extraProperties) {
  WebInspector.ObjectPropertiesSection.call(this, object, title,
      null /* subtitle */, null /* emptyPlaceholder */,
      true /* ignoreHasOwnProperty */, null /* extraProperties */,
      WebInspector.DebuggedObjectTreeElement);
};
goog.inherits(WebInspector.ConsoleObjectPropertiesSection,
    WebInspector.ObjectPropertiesSection);


/**
 * @override
 */
WebInspector.ConsoleObjectPropertiesSection.prototype.onpopulate = function() {
  devtools.tools.getDebuggerAgent().resolveChildren(
      this.object,
      goog.bind(this.didResolveChildren_, this));
};


/**
 * @param {Object} object
 */
WebInspector.ConsoleObjectPropertiesSection.prototype.didResolveChildren_ =
    function(object) {
  WebInspector.DebuggedObjectTreeElement.addResolvedChildren(
      object,
      this.propertiesTreeOutline,
      this.treeElementConstructor);
};


/**
 * Our implementation of ObjectPropertiesSection for scope variables.
 * @constructor
 */
WebInspector.ScopeChainPropertiesSection = function(object, title, thisObject) {
  WebInspector.ObjectPropertiesSection.call(this, object, title,
      null /* subtitle */, null /* emptyPlaceholder */,
      true /* ignoreHasOwnProperty */, null /* extraProperties */,
      WebInspector.DebuggedObjectTreeElement);
  this.thisObject_ = thisObject;
};
goog.inherits(WebInspector.ScopeChainPropertiesSection,
    WebInspector.ObjectPropertiesSection);


/**
 * @override
 */
WebInspector.ScopeChainPropertiesSection.prototype.onpopulate = function() {
  devtools.tools.getDebuggerAgent().resolveScope(
      this.object,
      goog.bind(this.didResolveChildren_, this));
};

/**
 * @param {Object} object
 */
WebInspector.ScopeChainPropertiesSection.prototype.didResolveChildren_ =
    function(object) {
  // Add this to the properties list if it's specified.
  if (this.thisObject_) {
    object.resolvedValue['this'] = this.thisObject_;
  }
  WebInspector.DebuggedObjectTreeElement.addResolvedChildren(
      object,
      this.propertiesTreeOutline,
      this.treeElementConstructor);
};


/**
 * Custom implementation of TreeElement that asynchronously resolves children
 * using the debugger agent.
 * @constructor
 */
WebInspector.DebuggedObjectTreeElement = function(property) {
  WebInspector.ScopeVariableTreeElement.call(this, property);
}
WebInspector.DebuggedObjectTreeElement.inherits(
    WebInspector.ScopeVariableTreeElement);


/**
 * @override
 */
WebInspector.DebuggedObjectTreeElement.prototype.onpopulate =
    function() {
  var obj = this.property.value.objectId;
  devtools.tools.getDebuggerAgent().resolveChildren(obj,
      goog.bind(this.didResolveChildren_, this), false /* no intrinsic */ );
};


/**
 * Callback function used with the resolveChildren.
 */
WebInspector.DebuggedObjectTreeElement.prototype.didResolveChildren_ =
    function(object) {
  this.removeChildren();
  WebInspector.DebuggedObjectTreeElement.addResolvedChildren(
      object,
      this,
      this.treeOutline.section.treeElementConstructor);
};


/**
 * Utility function used to populate children list of tree element representing
 * debugged object with values resolved through the debugger agent.
 * @param {Object} resolvedObject Object whose properties have been resolved.
 * @param {Element} treeElementContainer Container fot the HTML elements
 *     representing the resolved properties.
 * @param {function(object, string):Element} treeElementConstructor
 */
WebInspector.DebuggedObjectTreeElement.addResolvedChildren = function(
    resolvedObject, treeElementContainer, treeElementConstructor) {
  var object = resolvedObject.resolvedValue;
  var names = Object.sortedProperties(object);
  for (var i = 0; i < names.length; i++) {
    var childObject = object[names[i]];
    var property = {};
    property.name = names[i];
    property.value = new WebInspector.ObjectProxy(childObject, [], 0,
        Object.describe(childObject, true),
        typeof childObject == 'object' && Object.hasProperties(childObject));
    treeElementContainer.appendChild(new treeElementConstructor(property));
  }
};


/**
 * This function overrides standard searchableViews getters to perform search
 * only in the current view (other views are loaded asynchronously, no way to
 * search them yet).
 */
WebInspector.searchableViews_ = function() {
  var views = [];
  const visibleView = this.visibleView;
  if (visibleView && visibleView.performSearch) {
    views.push(visibleView);
  }
  return views;
};


/**
 * @override
 */
WebInspector.ResourcesPanel.prototype.__defineGetter__(
    'searchableViews',
    WebInspector.searchableViews_);


/**
 * @override
 */
WebInspector.ScriptsPanel.prototype.__defineGetter__(
    'searchableViews',
    WebInspector.searchableViews_);


WebInspector.ScriptsPanel.prototype.doEvalInCallFrame =
    function(callFrame, expression, callback) {
  if (!expression) {
    // Empty expression should eval to scope roots for completions to work.
    devtools.CallFrame.getVariablesInScopeAsync(callFrame, callback);
    return;
  }
  devtools.CallFrame.doEvalInCallFrame(callFrame, expression, callback);
};


(function() {
  var oldShow = WebInspector.ScriptsPanel.prototype.show;
  WebInspector.ScriptsPanel.prototype.show =  function() {
    devtools.tools.getDebuggerAgent().initUI();
    this.enableToggleButton.visible = false;
    oldShow.call(this);
  };
})();


// As columns in data grid can't be changed after initialization,
// we need to intercept the constructor and modify columns upon creation.
(function InterceptDataGridForProfiler() {
   var originalDataGrid = WebInspector.DataGrid;
   WebInspector.DataGrid = function(columns) {
     if (('average' in columns) && ('calls' in columns)) {
       delete columns['average'];
       delete columns['calls'];
     }
     return new originalDataGrid(columns);
   };
})();


// WebKit's profiler displays milliseconds with high resolution (shows
// three digits after the decimal point). We never have such resolution,
// as our minimal sampling rate is 1 ms. So we are disabling high resolution
// to avoid visual clutter caused by meaningless ".000" parts.
(function InterceptTimeDisplayInProfiler() {
   var originalDataGetter =
       WebInspector.ProfileDataGridNode.prototype.__lookupGetter__('data');
   WebInspector.ProfileDataGridNode.prototype.__defineGetter__('data',
     function() {
       var oldNumberSecondsToString = Number.secondsToString;
       Number.secondsToString = function(seconds, formatterFunction) {
         return oldNumberSecondsToString(seconds, formatterFunction, false);
       };
       var data = originalDataGetter.call(this);
       Number.secondsToString = oldNumberSecondsToString;
       return data;
     });
})();


(function InterceptProfilesPanelEvents() {
  var oldShow = WebInspector.ProfilesPanel.prototype.show;
  WebInspector.ProfilesPanel.prototype.show = function() {
    devtools.tools.getDebuggerAgent().initializeProfiling();
    this.enableToggleButton.visible = false;
    oldShow.call(this);
    // Show is called on every show event of a panel, so
    // we only need to intercept it once.
    WebInspector.ProfilesPanel.prototype.show = oldShow;
  };
})();


/**
 * @override
 * TODO(pfeldman): Add l10n.
 */
WebInspector.UIString = function(string) {
  return String.vsprintf(string, Array.prototype.slice.call(arguments, 1));
};


// There is no clear way of setting frame title yet. So sniffing main resource
// load.
(function OverrideUpdateResource() {
  var originalUpdateResource = WebInspector.updateResource;
  WebInspector.updateResource = function(identifier, payload) {
    originalUpdateResource.call(this, identifier, payload);
    var resource = this.resources[identifier];
    if (resource && resource.mainResource && resource.finished) {
      document.title =
          WebInspector.UIString('Developer Tools - %s', resource.url);
    }
  };
})();


// There is no clear way of rendering class name for scope variables yet.
(function OverrideObjectDescribe() {
  var oldDescribe = Object.describe;
  Object.describe = function(obj, abbreviated) {
    if (obj instanceof WebInspector.UnresolvedPropertyValue) {
      return obj.className;
    }

    var result = oldDescribe.call(Object, obj, abbreviated);
    if (result == 'Object' && obj.className) {
      return obj.className;
    }
    return result;
  };
})();


// Highlight extension content scripts in the scripts list.
(function () {
  var original = WebInspector.ScriptsPanel.prototype._addScriptToFilesMenu;
  WebInspector.ScriptsPanel.prototype._addScriptToFilesMenu = function(script) {
    var result = original.apply(this, arguments);
    var debuggerAgent = devtools.tools.getDebuggerAgent();
    var type = debuggerAgent.getScriptContextType(script.sourceID);
    var option = script.filesSelectOption;
    if (type == 'injected' && option) {
      option.addStyleClass('injected');
    }
    return result;
  };
})();


WebInspector.ConsoleView.prototype._formatobject = function(object, elem) {
  var section;
  if (object.handle && object.className) {
    object.ref = object.handle;
    var className = object.className;
    section = new WebInspector.ConsoleObjectPropertiesSection(object,
        className);
    section.pane = {
      callFrame: {
        _expandedProperties : { className : '' }
      }
    };
  } else {
    section = new WebInspector.ObjectPropertiesSection(
        new WebInspector.ObjectProxy(object.___devtools_id),
        object.___devtools_class_name);
  }
  elem.appendChild(section.element);
};


/** Pending WebKit upstream by apavlov). Fixes iframe vs drag problem. */
(function() {
  var originalDragStart = WebInspector.elementDragStart;
  WebInspector.elementDragStart = function(element) {
    var glassPane = document.createElement("div");
    glassPane.style.cssText =
        'position:absolute;width:100%;height:100%;opacity:0;z-index:1';
    glassPane.id = 'glass-pane-for-drag';
    element.parentElement.appendChild(glassPane);

    originalDragStart.apply(this, arguments);
  };

  var originalDragEnd = WebInspector.elementDragEnd;
  WebInspector.elementDragEnd = function() {
    originalDragEnd.apply(this, arguments);

    var glassPane = document.getElementById('glass-pane-for-drag');
    glassPane.parentElement.removeChild(glassPane);
  };
})();


(function() {
  var originalCreatePanels = WebInspector._createPanels;
  WebInspector._createPanels = function() {
    originalCreatePanels.apply(this, arguments);
    this.panels.heap = new WebInspector.HeapProfilerPanel();
  };
})();
