// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @typedef {Document|DocumentFragment|Element} */
var ProcessingRoot;

/**
 * @fileoverview This is a simple template engine inspired by JsTemplates
 * optimized for i18n.
 *
 * It currently supports three handlers:
 *
 *   * i18n-content which sets the textContent of the element.
 *
 *     <span i18n-content="myContent"></span>
 *
 *   * i18n-options which generates <option> elements for a <select>.
 *
 *     <select i18n-options="myOptionList"></select>
 *
 *   * i18n-values is a list of attribute-value or property-value pairs.
 *     Properties are prefixed with a '.' and can contain nested properties.
 *
 *     <span i18n-values="title:myTitle;.style.fontSize:fontSize"></span>
 *
 * This file is a copy of i18n_template.js, with minor tweaks to support using
 * load_time_data.js. It should replace i18n_template.js eventually.
 */

var i18nTemplate = (function() {
  /**
   * This provides the handlers for the templating engine. The key is used as
   * the attribute name and the value is the function that gets called for every
   * single node that has this attribute.
   * @type {!Object}
   */
  var handlers = {
    /**
     * This handler sets the textContent of the element.
     * @param {HTMLElement} element The node to modify.
     * @param {string} key The name of the value in the dictionary.
     * @param {LoadTimeData} dictionary The dictionary of strings to draw from.
     * @param {!Array<ProcessingRoot>} visited
     */
    'i18n-content': function(element, key, dictionary, visited) {
      element.textContent = dictionary.getString(key);
    },

    /**
     * This handler adds options to a <select> element.
     * @param {HTMLElement} select The node to modify.
     * @param {string} key The name of the value in the dictionary. It should
     *     identify an array of values to initialize an <option>. Each value,
     *     if a pair, represents [content, value]. Otherwise, it should be a
     *     content string with no value.
     * @param {LoadTimeData} dictionary The dictionary of strings to draw from.
     * @param {!Array<ProcessingRoot>} visited
     */
    'i18n-options': function(select, key, dictionary, visited) {
      var options = dictionary.getValue(key);
      options.forEach(function(optionData) {
        var option = typeof optionData == 'string' ?
            new Option(optionData) :
            new Option(optionData[1], optionData[0]);
        select.appendChild(option);
      });
    },

    /**
     * This is used to set HTML attributes and DOM properties. The syntax is:
     *   attributename:key;
     *   .domProperty:key;
     *   .nested.dom.property:key
     * @param {HTMLElement} element The node to modify.
     * @param {string} attributeAndKeys The path of the attribute to modify
     *     followed by a colon, and the name of the value in the dictionary.
     *     Multiple attribute/key pairs may be separated by semicolons.
     * @param {LoadTimeData} dictionary The dictionary of strings to draw from.
     * @param {!Array<ProcessingRoot>} visited
     */
    'i18n-values': function(element, attributeAndKeys, dictionary, visited) {
      var parts = attributeAndKeys.replace(/\s/g, '').split(/;/);
      parts.forEach(function(part) {
        if (!part)
          return;

        var attributeAndKeyPair = part.match(/^([^:]+):(.+)$/);
        if (!attributeAndKeyPair)
          throw new Error('malformed i18n-values: ' + attributeAndKeys);

        var propName = attributeAndKeyPair[1];
        var propExpr = attributeAndKeyPair[2];

        var value = dictionary.getValue(propExpr);

        // Allow a property of the form '.foo.bar' to assign a value into
        // element.foo.bar.
        if (propName[0] == '.') {
          var path = propName.slice(1).split('.');
          var targetObject = element;
          while (targetObject && path.length > 1) {
            targetObject = targetObject[path.shift()];
          }
          if (targetObject) {
            targetObject[path] = value;
            // In case we set innerHTML (ignoring others) we need to recursively
            // check the content.
            if (path == 'innerHTML') {
              for (var temp = element.firstElementChild; temp;
                   temp = temp.nextElementSibling) {
                processWithoutCycles(temp, dictionary, visited);
              }
            }
          }
        } else {
          element.setAttribute(propName, /** @type {string} */(value));
        }
      });
    }
  };

  var attributeNames = Object.keys(handlers);
  // Only use /deep/ when shadow DOM is supported. As of April 2015 iOS Chrome
  // doesn't support shadow DOM.
  var prefix = Element.prototype.createShadowRoot ? ':root /deep/ ' : '';
  var selector = prefix + '[' + attributeNames.join('],' + prefix + '[') + ']';

  /**
   * Processes a DOM tree with the {@code dictionary} map.
   * @param {ProcessingRoot} root The root of the DOM tree to process.
   * @param {LoadTimeData} dictionary The dictionary to draw from.
   */
  function process(root, dictionary) {
    processWithoutCycles(root, dictionary, []);
  }

  /**
   * Internal process() method that stops cycles while processing.
   * @param {ProcessingRoot} root
   * @param {LoadTimeData} dictionary
   * @param {!Array<ProcessingRoot>} visited Already visited roots.
   */
  function processWithoutCycles(root, dictionary, visited) {
    if (visited.indexOf(root) >= 0) {
      // Found a cycle. Stop it.
      return;
    }

    // Mark the node as visited before recursing.
    visited.push(root);

    var importLinks = root.querySelectorAll('link[rel=import]');
    for (var i = 0; i < importLinks.length; ++i) {
      var importLink = /** @type {!HTMLLinkElement} */(importLinks[i]);
      if (!importLink.import) {
        // Happens when a <link rel=import> is inside a <template>.
        // TODO(dbeam): should we log an error if we detect that here?
        continue;
      }
      processWithoutCycles(importLink.import, dictionary, visited);
    }

    var templates = root.querySelectorAll('template');
    for (var i = 0; i < templates.length; ++i) {
      var template = /** @type {HTMLTemplateElement} */(templates[i]);
      processWithoutCycles(template.content, dictionary, visited);
    }

    var firstElement = root instanceof Element ? root : root.querySelector('*');

    if (prefix) {
      // Prefixes skip root level elements. This is typically <html> but can
      // differ inside of DocumentFragments (i.e. <template>s). Process them
      // explicitly.
      for (var temp = firstElement; temp; temp = temp.nextElementSibling) {
        processElement(/** @type {Element} */(temp), dictionary, visited);
      }
    }

    var elements = root.querySelectorAll(selector);
    for (var element, i = 0; element = elements[i]; i++) {
      processElement(element, dictionary, visited);
    }

    if (firstElement)
      firstElement.setAttribute('i18n-processed', '');
  }

  /**
   * Run through various [i18n-*] attributes and do activate replacements.
   * @param {Element} element
   * @param {LoadTimeData} dictionary
   * @param {!Array<ProcessingRoot>} visited
   */
  function processElement(element, dictionary, visited) {
    for (var i = 0; i < attributeNames.length; i++) {
      var name = attributeNames[i];
      var attribute = element.getAttribute(name);
      if (attribute != null)
        handlers[name](element, attribute, dictionary, visited);
    }
  }

  return {
    process: process
  };
}());
