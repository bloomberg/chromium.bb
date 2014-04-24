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
  var doc = element.ownerDocument;
  this.dummySpan_ = doc.createElement('span');
  this.dummySpan_ = doc.getElementsByTagName('body')[0].
                        appendChild(this.dummySpan_);
  this.dummySpan_.style.position = 'absolute';
  this.dummySpan_.style.visibility = 'hidden';
  var styles = window.getComputedStyle(element, '');
  var stylesToBeCopied = [
    'fontSize',
    'fontStyle',
    'fontWeight',
    'fontFamily',
    'letterSpacing'
  ];
  for (var i = 0; i < stylesToBeCopied.length; i++) {
    this.dummySpan_.style[stylesToBeCopied[i]] = styles[stylesToBeCopied[i]];
  }
  Object.seal(this);
};

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
