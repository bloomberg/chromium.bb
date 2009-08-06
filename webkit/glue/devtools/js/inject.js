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
 * Returns object for given id. This can be either node wrapper for
 * integer ids or evaluation results that recide in cached console
 * objects cache for arbitrary keys.
 * @param {number|string} id Id to get object for.
 * @return {Object} resolved object.
 */
devtools.Injected.prototype.getObjectForId_ = function(id) {
  if (typeof id == 'number') {
    return DevToolsAgentHost.getNodeForId(id);
  }
  return this.cachedConsoleObjects_[id];
};


/**
 * Returns array of properties for a given node on a given path.
 * @param {number} nodeId Id of node to get prorotypes for.
 * @param {Array.<string>} path Path to the nested object.
 * @param {number} protoDepth Depth of the actual proto to inspect.
 * @return {Array.<Object>} Array where each property is represented
 *     by the four entries [{string} type, {string} name, {Object} value,
 *     {string} objectClassNameOrFunctionSignature].
 */
devtools.Injected.prototype.getProperties =
    function(nodeId, path, protoDepth) {
  var result = [];
  var obj = this.getObjectForId_(nodeId);
  if (!obj) {
    return [];
  }

  // Follow the path.
  for (var i = 0; obj && i < path.length; ++i) {
    obj = obj[path[i]];
  }

  if (!obj) {
    return [];
  }

  // Get to the necessary proto layer.
  for (var i = 0; obj && i < protoDepth; ++i) {
    obj = obj.__proto__;
  }

  if (!obj) {
    return [];
  }

  // Go over properties, prepare results.
  for (var name in obj) {
    if (protoDepth != -1 && 'hasOwnProperty' in obj &&
       !obj.hasOwnProperty(name)) {
      continue;
    }
    if (obj['__lookupGetter__'] && obj.__lookupGetter__(name)) {
      continue;
    }

    var value;
    var type;
    try {
      value = obj[name];
      type = typeof value;
    } catch (e) {
      value = 'Exception: ' + e.toString();
      type = 'string';
    }
    result.push(type);
    result.push(name);
    if (type == 'string') {
      var str = value;
      result.push(str.length > 99 ? str.substr(0, 99) + '...' : str);
      result.push(undefined);
    } else if (type == 'function') {
      result.push(undefined);
      var str = Function.prototype.toString.call(value);
      // Cut function signature (everything before first ')').
      var signatureLength = str.search(/\)/);
      str = str.substr(0, signatureLength + 1);
      // Collapse each group of consecutive whitespaces into one whitespaces
      // and add body brackets.
      str = str.replace(/\s+/g, ' ') + ' {}';
      result.push(str);
    } else if (type == 'object') {
      result.push(undefined);
      result.push(this.getClassName_(value));
    } else {
      result.push(value);
      result.push(undefined);
    }
  }
  return result;
};


/**
 * Returns array of prototypes for a given node.
 * @param {number} nodeId Id of node to get prorotypes for.
 * @return {Array<string>} Array of proto names.
 */
devtools.Injected.prototype.getPrototypes = function(nodeId) {
  var node = DevToolsAgentHost.getNodeForId(nodeId);
  if (!node) {
    return [];
  }

  var result = [];
  for (var prototype = node; prototype; prototype = prototype.__proto__) {
    result.push(this.getClassName_(prototype));
  }
  return result;
};


/**
 * @param {Object|Function|null} value An object whose class name to return.
 * @return {string} The value class name.
 */
devtools.Injected.prototype.getClassName_ = function(value) {
  return (value == null) ? 'null' : value.constructor.name;
};


/**
 * Taken from utilities.js as is for injected evaluation.
 */
devtools.Injected.prototype.trimWhitespace_ = function(str) {
  return str.replace(/^[\s\xA0]+|[\s\xA0]+$/g, '');
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
    result.___devtools_class_name = this.getClassName_(obj);
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


// Following methods are here temporarily, until they are refactored into
// InjectedScript.js in WebKit.
function getStyleTextWithShorthands(style)
{
    var cssText = "";
    var foundProperties = {};
    for (var i = 0; i < style.length; ++i) {
        var individualProperty = style[i];
        var shorthandProperty = style.getPropertyShorthand(individualProperty);
        var propertyName = (shorthandProperty || individualProperty);

        if (propertyName in foundProperties)
            continue;

        if (shorthandProperty) {
            var value = getShorthandValue(style, shorthandProperty);
            var priority = getShorthandPriority(style, shorthandProperty);
        } else {
            var value = style.getPropertyValue(individualProperty);
            var priority = style.getPropertyPriority(individualProperty);
        }

        foundProperties[propertyName] = true;

        cssText += propertyName + ": " + value;
        if (priority)
            cssText += " !" + priority;
        cssText += "; ";
    }

    return cssText;
}


function getShorthandValue(style, shorthandProperty)
{
    var value = style.getPropertyValue(shorthandProperty);
    if (!value) {
        // Some shorthands (like border) return a null value, so compute a shorthand value.
        // FIXME: remove this when http://bugs.webkit.org/show_bug.cgi?id=15823 is fixed.

        var foundProperties = {};
        for (var i = 0; i < style.length; ++i) {
            var individualProperty = style[i];
            if (individualProperty in foundProperties || style.getPropertyShorthand(individualProperty) !== shorthandProperty)
                continue;

            var individualValue = style.getPropertyValue(individualProperty);
            if (style.isPropertyImplicit(individualProperty) || individualValue === "initial")
                continue;

            foundProperties[individualProperty] = true;

            if (!value)
                value = "";
            else if (value.length)
                value += " ";
            value += individualValue;
        }
    }
    return value;
}


function getShorthandPriority(style, shorthandProperty)
{
    var priority = style.getPropertyPriority(shorthandProperty);
    if (!priority) {
        for (var i = 0; i < style.length; ++i) {
            var individualProperty = style[i];
            if (style.getPropertyShorthand(individualProperty) !== shorthandProperty)
                continue;
            priority = style.getPropertyPriority(individualProperty);
            break;
        }
    }
    return priority;
}


function getLonghandProperties(style, shorthandProperty)
{
    var properties = [];
    var foundProperties = {};

    for (var i = 0; i < style.length; ++i) {
        var individualProperty = style[i];
        if (individualProperty in foundProperties || style.getPropertyShorthand(individualProperty) !== shorthandProperty)
            continue;
        foundProperties[individualProperty] = true;
        properties.push(individualProperty);
    }

    return properties;
}


function getUniqueStyleProperties(style)
{
    var properties = [];
    var foundProperties = {};

    for (var i = 0; i < style.length; ++i) {
        var property = style[i];
        if (property in foundProperties)
            continue;
        foundProperties[property] = true;
        properties.push(property);
    }

    return properties;
}
