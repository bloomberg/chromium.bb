// Copyright 2015  The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Provides copied versions of Closure library functions. The
 *     functions in this file are modified to remove non-Chrome compatibility
 *     code.
 */
goog.provide('image.collections.extension.domextractor.DomUtils');

goog.require('image.collections.extension.domextractor.Size');


goog.scope(function() {
var DomUtils = image.collections.extension.domextractor.DomUtils;


/**
 * Inherit the prototype methods from one constructor into another.
 *
 * Usage:
 * <pre>
 * function ParentClass(a, b) { }
 * ParentClass.prototype.foo = function(a) { };
 *
 * function ChildClass(a, b, c) {
 *   ChildClass.base(this, 'constructor', a, b);
 * }
 * DomUtils.inherits(ChildClass, ParentClass);
 *
 * var child = new ChildClass('a', 'b', 'see');
 * child.foo(); // This works.
 * </pre>
 *
 * @param {Function} childCtor Child class.
 * @param {Function} parentCtor Parent class.
 */
image.collections.extension.domextractor.DomUtils.inherits =
    function(childCtor, parentCtor) {
  /** @constructor */
  function tempCtor() {};
  tempCtor.prototype = parentCtor.prototype;
  childCtor.prototype = new tempCtor();
  /** @override */
  childCtor.prototype.constructor = childCtor;

  /**
   * Calls superclass constructor/method.
   *
   * @param {!Object} me Should always be "this".
   * @param {string} methodName The method name to call. Calling
   *     superclass constructor can be done with the special string
   *     'constructor'.
   * @param {...*} var_args The arguments to pass to superclass
   *     method/constructor.
   * @return {*} The return value of the superclass method/constructor.
   */
  childCtor.base = function(me, methodName, var_args) {
    // Copying using loop to avoid deop due to passing arguments object to
    // function. This is faster in many JS engines as of late 2014.
    var args = new Array(arguments.length - 2);
    for (var i = 2; i < arguments.length; i++) {
      args[i - 2] = arguments[i];
    }
    return parentCtor.prototype[methodName].apply(me, args);
  };
};


/**
 * Map of tags whose content to ignore when calculating text length.
 * @const {!Object<string, number>}
 */
var TAGS_TO_IGNORE = {
  'SCRIPT': 1,
  'STYLE': 1,
  'HEAD': 1,
  'IFRAME': 1,
  'OBJECT': 1
};

/**
 * Map of tags which have predefined values with regard to whitespace.
 * @const {!Object<string, string>}
 */
var PREDEFINED_TAG_VALUES = {'IMG': ' ', 'BR': '\n'};

/** @const {number} */
var ELEMENT_NODE_TYPE = 1;

/** @const {number} */
var TEXT_NODE_TYPE = 3;

/** @const {number} */
var DOCUMENT_NODE_TYPE = 9;

/**
 * Regular expression that matches an HTML entity.
 * See also HTML5: Tokenization / Tokenizing character references.
 * @type {!RegExp}
 */
var HTML_ENTITY_PATTERN = /&([^;\s<&]+);?/g;



/**
 * Retrieves a computed style value of a node. It returns empty string if the
 * value cannot be computed (which will be the case in Internet Explorer) or
 * "none" if the property requested is an SVG one and it has not been
 * explicitly set (firefox and webkit).
 *
 * @param {!Element} element Element to get style of.
 * @param {string} property Property to get (camel-case).
 * @return {string} Style value.
 */
image.collections.extension.domextractor.DomUtils.getComputedStyle =
    function(element, property) {
  var doc = DomUtils.getOwnerDocument(element);
  if (doc.defaultView && doc.defaultView.getComputedStyle) {
    var styles = doc.defaultView.getComputedStyle(element, null);
    if (styles) {
      // element.style[..] is undefined for browser specific styles
      // as 'filter'.
      return styles[property] || styles.getPropertyValue(property) || '';
    }
  }

  return '';
};


/**
 * Gets the height and width of an element, even if its display is none.
 *
 * Specifically, this returns the height and width of the border box,
 * irrespective of the box model in effect.
 *
 * Note that this function does not take CSS transforms into account.
 * @param {!Element} element Element to get size of.
 * @return {!image.collections.extension.domextractor.Size} Object with
 *     width/height properties.
 */
image.collections.extension.domextractor.DomUtils.getSize = function(element) {
  if (DomUtils.getComputedStyle(element, 'display') != 'none') {
    return DomUtils.getSizeWithDisplay_(element);
  }

  var style = element.style;
  var originalDisplay = style.display;
  var originalVisibility = style.visibility;
  var originalPosition = style.position;

  style.visibility = 'hidden';
  style.position = 'absolute';
  style.display = 'inline';

  var retVal = DomUtils.getSizeWithDisplay_(element);

  style.display = originalDisplay;
  style.position = originalPosition;
  style.visibility = originalVisibility;

  return retVal;
};


/**
 * Gets the height and width of an element when the display is not none.
 * @param {!Element} element Element to get size of.
 * @return {!image.collections.extension.domextractor.Size} Object with
 *     width/height properties.
 * @private
 */
image.collections.extension.domextractor.DomUtils.getSizeWithDisplay_ =
    function(element) {
  var offsetWidth = element.offsetWidth;
  var offsetHeight = element.offsetHeight;
  var offsetsZero = !offsetWidth && !offsetHeight;
  if ((offsetWidth === undefined) || offsetsZero) {
    // Fall back to calling getBoundingClientRect when offsetWidth or
    // offsetHeight are not defined, or when they are zero.
    // This makes sure that we return for the correct size for SVG elements.
    var clientRect = element.getBoundingClientRect();
    return new image.collections.extension.domextractor.Size(
        clientRect.right - clientRect.left, clientRect.bottom - clientRect.top);
  }
  return new image.collections.extension.domextractor.Size(
      offsetWidth, offsetHeight);
};


/**
 * Returns the owner document for a node.
 * @param {!Node|!Window} node The node to get the document for.
 * @return {!Document} The document owning the node.
 */
image.collections.extension.domextractor.DomUtils.getOwnerDocument =
    function(node) {
  return /** @type {!Document} */ (node.nodeType == DOCUMENT_NODE_TYPE ?
      node : node.ownerDocument || node.document);
};


/**
 * Returns an element's parent, if it's an Element.
 * @param {Element} element The DOM element.
 * @return {Element} The parent, or null if not an Element.
 */
image.collections.extension.domextractor.DomUtils.getParentElement =
    function(element) {
  if (element.parentElement) {
    return element.parentElement;
  }
  var parent = element.parentNode;
  if (typeof parent == 'object' && parent.nodeType == ELEMENT_NODE_TYPE) {
    return /** @type {!Element} */ (parent);
  }
  return null;
};


/**
 * Returns the top coordinate of an element relative to the HTML document
 * @param {!Element} el Elements.
 * @return {number} The top coordinate.
 */
image.collections.extension.domextractor.DomUtils.getPageOffsetTop =
    function(el) {
  var doc = DomUtils.getOwnerDocument(el);
  if (el == doc.documentElement) {
    // viewport is always at 0,0 as that defined the coordinate system for this
    // function - this avoids special case checks in the code below
    return 0;
  }

  // Must add the scroll coordinates in to get the absolute page offset
  // of element since getBoundingClientRect returns relative coordinates to
  // the viewport.
  var documentScrollElement = doc.body || doc.documentElement;
  var win = doc.defaultView;
  var scrollOffset = win.pageYOffset || documentScrollElement.scrollTop;

  return el.getBoundingClientRect().top + scrollOffset;
};


/**
 * Returns the text content of the current node, without markup and invisible
 * symbols. New lines are stripped and whitespace is collapsed,
 * such that each character would be visible.
 *
 * @param {Node} node The node from which we are getting content.
 * @return {string} The text content.
 */
DomUtils.getTextContent = function(node) {
  var textContent;
  var buf = [];
  DomUtils.getTextContent_(node, buf);
  textContent = buf.join('');

  textContent = textContent.replace(/ +/g, ' ');
  if (textContent != ' ') {
    textContent = textContent.replace(/^\s*/, '');
  }

  return textContent;
};

/**
 * Recursive support function for text content retrieval.
 *
 * @param {Node} node The node from which we are getting content.
 * @param {Array<string>} buf string buffer.
 * @private
 */
image.collections.extension.domextractor.DomUtils.getTextContent_ =
    function(node, buf) {
  if (node.nodeName in TAGS_TO_IGNORE) {
    // ignore certain tags
  } else if (node.nodeType == TEXT_NODE_TYPE) {
    // Text node
    buf.push(String(node.nodeValue).replace(/(\r\n|\r|\n)/g, ''));
  } else if (node.nodeName in PREDEFINED_TAG_VALUES) {
    buf.push(PREDEFINED_TAG_VALUES[node.nodeName]);
  } else {
    var child = node.firstChild;
    while (child) {
      DomUtils.getTextContent_(child, buf);
      child = child.nextSibling;
    }
  }
};


/**
 * Unescapes an HTML string using a DOM to resolve non-XML, non-numeric
 * entities. This function is XSS-safe and whitespace-preserving.
 * @param {string} str The string to unescape.
 * @param {Document=} opt_document An optional document to use for creating
 *     elements. If this is not specified then the default window.document
 *     will be used.
 * @return {string} The unescaped {@code str} string.
 */
image.collections.extension.domextractor.DomUtils.unescapeEntitiesUsingDom =
    function(str, opt_document) {
  if (str.indexOf('&') == -1) {
    return str;
  }
  /** @type {!Object<string, string>} */
  var seen = {'&amp;': '&', '&lt;': '<', '&gt;': '>', '&quot;': '"'};
  var div;
  if (opt_document) {
    div = opt_document.createElement('div');
  } else {
    div = document.createElement('div');
  }
  // Match as many valid entity characters as possible. If the actual entity
  // happens to be shorter, it will still work as innerHTML will return the
  // trailing characters unchanged. Since the entity characters do not include
  // open angle bracket, there is no chance of XSS from the innerHTML use.
  // Since no whitespace is passed to innerHTML, whitespace is preserved.
  return str.replace(HTML_ENTITY_PATTERN, function(s, entity) {
    // Check for cached entity.
    var value = seen[s];
    if (value) {
      return value;
    }
    // Check for numeric entity.
    if (entity.charAt(0) == '#') {
      // Prefix with 0 so that hex entities (e.g. &#x10) parse as hex numbers.
      var n = Number('0' + entity.substr(1));
      if (!isNaN(n)) {
        value = String.fromCharCode(n);
      }
    }
    // Fall back to innerHTML otherwise.
    if (!value) {
      // Append a non-entity character to avoid a bug in Webkit that parses
      // an invalid entity at the end of innerHTML text as the empty string.
      div.innerHTML = s + ' ';
      // Then remove the trailing character from the result.
      value = div.firstChild.nodeValue.slice(0, -1);
    }
    // Cache and return.
    return seen[s] = value;
  });
};
});  // goog.scope
