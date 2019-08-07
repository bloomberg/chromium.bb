// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Sanitizer which filters the html snippet with a set of whitelisted tags.
 */
class HtmlSanitizer {
  constructor() {
    // initialize set of whitelisted tags.
    this.allowedTags = new Set(['b', 'i', 'br', 'p', 'a', 'ul', 'li', 'div']);
  }

  /**
   * Sanitize the html snippet.
   * Only allow the tags in allowedTags.
   *
   * @param {string} content the html snippet to be sanitized.
   * @return {string} sanitized html snippet.
   *
   * @public
   */
  sanitizeHtml(content) {
    var doc = document.implementation.createHTMLDocument();
    var div = doc.createElement('div');
    div.innerHTML = content;
    return this.sanitizeNode_(doc, div).innerHTML;
  }

  /**
   * Sanitize the html node.
   *
   * @param {Document} doc document object for sanitize use.
   * @param {Element} node the DOM element to be sanitized.
   * @return {Element} sanitized DOM element.
   *
   * @private
   */
  sanitizeNode_(doc, node) {
    var name = node.nodeName.toLowerCase();
    if (name == '#text') {
      return node;
    }
    if (!this.allowedTags.has(name)) {
      return doc.createTextNode('');
    }

    var copy = doc.createElement(name);
    // Only allow 'href' attribute for tag 'a'.
    if (name == 'a' && node.attributes.length == 1 &&
        node.attributes.item(0).name == 'href') {
      copy.setAttribute('href', node.getAttribute('href'));
    }

    while (node.childNodes.length > 0) {
      var child = node.removeChild(node.childNodes[0]);
      copy.appendChild(this.sanitizeNode_(doc, child));
    }
    return copy;
  }
}
