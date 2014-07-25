// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * TextMeasure constructor.
 *
 * TextMeasure is a measure for text that returns the width of text.  This
 * class has a dummy span element. When measuring the width of text, it sets
 * the text to the element and obtains the element's size by
 * getBoundingClientRect.
 *
 * @constructor
 * @param {HTMLElement} element Element that has styles of measured text. The
 *     width of text is measures like as it is rendered in this element.
 */
var TextMeasure = function(element) {
  this.dummySpan_ = createElementWithClassName('span', 'text-measure');
  var styles = window.getComputedStyle(element, '');
  for (var i = 0; i < TextMeasure.STYLES_TO_BE_COPYED.length; i++) {
    var name = TextMeasure.STYLES_TO_BE_COPYED[i];
    this.dummySpan_.style[name] = styles[name];
  }
  element.ownerDocument.body.appendChild(this.dummySpan_);
  Object.seal(this);
};

/**
 * Style names to be copied to the measured dummy span.
 * @type {Array.<String>}
 * @const
 */
TextMeasure.STYLES_TO_BE_COPYED = Object.freeze([
  'fontSize',
  'fontStyle',
  'fontWeight',
  'fontFamily',
  'letterSpacing'
]);

/**
 * Measures the width of text.
 *
 * @param {string} text Text that is measured the width.
 * @return {number} Width of the specified text.
 */
TextMeasure.prototype.getWidth = function(text) {
  this.dummySpan_.innerText = text;
  var rect = this.dummySpan_.getBoundingClientRect();
  return rect ? rect.width : 0;
};
