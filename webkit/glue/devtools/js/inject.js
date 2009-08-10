// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Javascript that is being injected into the inspectable page
 * while debugging.
 */
goog.provide('devtools.Injected');


/**
 * Main injected object.
 * @constructor.
 */
devtools.Injected = function() {
  /**
   * This cache contains mapping from object it to an object instance for
   * all results of the evaluation / console logs.
   */
  this.cachedConsoleObjects_ = {};

  /**
   * Last id for the cache above.
   */
  this.lastCachedConsoleObjectId_ = 1;
};


/**
 * Caches console object for subsequent calls to getConsoleObjectProperties.
 * @param {Object} obj Object to cache.
 * @return {Object} console object wrapper.
 */
devtools.Injected.prototype.wrapConsoleObject = function(obj) {
  var type = typeof obj;
  if ((type == 'object' && obj != null) || type == 'function') {
    var objId = '#consoleobj#' + this.lastCachedConsoleObjectId_++;
    this.cachedConsoleObjects_[objId] = obj;
    var result = { ___devtools_id : objId };
    result.___devtools_class_name = Object.describe(obj, true);
    // Loop below fills dummy object with properties for completion.
    for (var name in obj) {
      result[name] = '';
    }
    return result;
  }
  return obj;
};


/**
 * Caches console object for subsequent calls to getConsoleObjectProperties.
 * @param {Object} obj Object to cache.
 * @return {string} Console object wrapper serialized into a JSON string.
 */
devtools.Injected.prototype.serializeConsoleObject = function(obj) {
  var result = this.wrapConsoleObject(obj);
  return JSON.stringify(result,
      function (key, value) {
        if (value === undefined) {
         return 'undefined';
        }
        return value;
      });
};


/**
 * Dispatches given method with given args on the host object.
 * @param {string} method Method name.
 */
devtools.Injected.prototype.InspectorController = function(method, var_args) {
  var args = Array.prototype.slice.call(arguments, 1);
  return InspectorController[method].apply(InspectorController, args);
};


/**
 * Dispatches given method with given args on the InjectedScript.
 * @param {string} method Method name.
 */
devtools.Injected.prototype.InjectedScript = function(method, var_args) {
  var args = Array.prototype.slice.call(arguments, 1);
  var result = InjectedScript[method].apply(InjectedScript, args);
  return result;
};


// Plugging into upstreamed support.
InjectedScript._window = function() {
  return contentWindow;
};


InjectedScript._nodeForId = function(nodeId) {
  return DevToolsAgentHost.getNodeForId(nodeId);
};


InjectedScript._objectForId = function(id) {
  if (typeof id == 'number') {
    return DevToolsAgentHost.getNodeForId(id);
  }
  return devtools$$obj.cachedConsoleObjects_[id];
};


Object.type = function(obj, win)
{
    if (obj === null)
        return "null";

    var type = typeof obj;
    if (type !== "object" && type !== "function")
        return type;

    win = win || window;

    if (obj instanceof win.Node)
        return (obj.nodeType === undefined ? type : "node");
    if (obj instanceof win.String)
        return "string";
    if (obj instanceof win.Array)
        return "array";
    if (obj instanceof win.Boolean)
        return "boolean";
    if (obj instanceof win.Number)
        return "number";
    if (obj instanceof win.Date)
        return "date";
    if (obj instanceof win.RegExp)
        return "regexp";
    if (obj instanceof win.Error)
        return "error";
    return type;
}

// Temporarily moved into the injected context.
Object.hasProperties = function(obj)
{
    if (typeof obj === "undefined" || typeof obj === "null")
        return false;
    for (var name in obj)
        return true;
    return false;
}

Object.describe = function(obj, abbreviated)
{
    var type1 = Object.type(obj);
    var type2 = (obj == null) ? "null" : obj.constructor.name;

    switch (type1) {
    case "object":
    case "node":
        return type2;
    case "array":
        return "[" + obj.toString() + "]";
    case "string":
        if (obj.length > 100)
            return "\"" + obj.substring(0, 100) + "\u2026\"";
        return "\"" + obj + "\"";
    case "function":
        var objectText = String(obj);
        if (!/^function /.test(objectText))
            objectText = (type2 == "object") ? type1 : type2;
        else if (abbreviated)
            objectText = /.*/.exec(obj)[0].replace(/ +$/g, "");
        return objectText;
    case "regexp":
        return String(obj).replace(/([\\\/])/g, "\\$1").replace(/\\(\/[gim]*)$/, "$1").substring(1);
    default:
        return String(obj);
    }
}
