// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Parses a very small subset of HTML.  This ensures that insecure HTML /
 * javascript cannot be injected into the new tab page.
 * @param {string} s The string to parse.
 * @param {Array<string>=} opt_extraTags Optional extra allowed tags.
 * @param {Object<function(Node, string):boolean>=} opt_extraAttrs
 *     Optional extra allowed attributes (all tags are run through these).
 * @throws {Error} In case of non supported markup.
 * @return {DocumentFragment} A document fragment containing the DOM tree.
 */
var parseHtmlSubset = (function() {
  'use strict';

  var allowedAttributes = {
    'href': function(node, value) {
      // Only allow a[href] starting with chrome:// and https://
      return node.tagName == 'A' &&
          (value.startsWith('chrome://') || value.startsWith('https://'));
    },
    'target': function(node, value) {
      // Only allow a[target='_blank'].
      // TODO(dbeam): are there valid use cases for target != '_blank'?
      return node.tagName == 'A' && value == '_blank';
    },
  };

  /**
   * Whitelist of tag names allowed in parseHtmlSubset.
   * @type {!Array<string>}
   * @const
   */
  var allowedTags = ['A', 'B', 'SPAN', 'STRONG'];

  /** @param {...Object} var_args Objects to merge. */
  function merge(var_args) {
    var clone = {};
    for (var i = 0; i < arguments.length; ++i) {
      if (typeof arguments[i] == 'object') {
        for (var key in arguments[i]) {
          if (arguments[i].hasOwnProperty(key))
            clone[key] = arguments[i][key];
        }
      }
    }
    return clone;
  }

  function walk(n, f) {
    f(n);
    for (var i = 0; i < n.childNodes.length; i++) {
      walk(n.childNodes[i], f);
    }
  }

  function assertElement(tags, node) {
    if (tags.indexOf(node.tagName) == -1)
      throw Error(node.tagName + ' is not supported');
  }

  function assertAttribute(attrs, attrNode, node) {
    var n = attrNode.nodeName;
    var v = attrNode.nodeValue;
    if (!attrs.hasOwnProperty(n) || !attrs[n](node, v))
      throw Error(node.tagName + '[' + n + '="' + v + '"] is not supported');
  }

  return function(s, opt_extraTags, opt_extraAttrs) {
    var extraTags = (opt_extraTags || []).map(function(str) {
      return str.toUpperCase();
    });
    var tags = allowedTags.concat(extraTags);
    var attrs = merge(allowedAttributes, opt_extraAttrs || {});

    var doc = document.implementation.createHTMLDocument('');
    var r = doc.createRange();
    r.selectNode(doc.body);
    // This does not execute any scripts because the document has no view.
    var df = r.createContextualFragment(s);
    walk(df, function(node) {
      switch (node.nodeType) {
        case Node.ELEMENT_NODE:
          assertElement(tags, node);
          var nodeAttrs = node.attributes;
          for (var i = 0; i < nodeAttrs.length; ++i) {
            assertAttribute(attrs, nodeAttrs[i], node);
          }
          break;

        case Node.COMMENT_NODE:
        case Node.DOCUMENT_FRAGMENT_NODE:
        case Node.TEXT_NODE:
          break;

        default:
          throw Error('Node type ' + node.nodeType + ' is not supported');
      }
    });
    return df;
  };
})();
