// Copyright 2014 The ChromeOS IME Authors. All Rights Reserved.
// limitations under the License.
// See the License for the specific language governing permissions and
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// distributed under the License is distributed on an "AS-IS" BASIS,
// Unless required by applicable law or agreed to in writing, software
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// You may obtain a copy of the License at
// you may not use this file except in compliance with the License.
// Licensed under the Apache License, Version 2.0 (the "License");
//
goog.provide('i18n.input.chrome.inputview.elements.layout.HandwritingLayout');

goog.require('goog.dom.classlist');
goog.require('i18n.input.chrome.inputview.Css');
goog.require('i18n.input.chrome.inputview.GlobalFlags');
goog.require('i18n.input.chrome.inputview.elements.Element');
goog.require('i18n.input.chrome.inputview.elements.ElementType');
goog.require('i18n.input.chrome.inputview.elements.Weightable');


goog.scope(function() {



/**
 * The linear layout.
 *
 * @param {string} id The id.
 * @param {goog.events.EventTarget=} opt_eventTarget The event target.
 * @constructor
 * @extends {i18n.input.chrome.inputview.elements.Element}
 * @implements {i18n.input.chrome.inputview.elements.Weightable}
 */
i18n.input.chrome.inputview.elements.layout.HandwritingLayout = function(id,
    opt_eventTarget) {
  goog.base(this, id, i18n.input.chrome.inputview.elements.ElementType.
      HANDWRITING_LAYOUT, opt_eventTarget);
};
goog.inherits(i18n.input.chrome.inputview.elements.layout.HandwritingLayout,
    i18n.input.chrome.inputview.elements.Element);
var HandwritingLayout =
    i18n.input.chrome.inputview.elements.layout.HandwritingLayout;


/**
 * The height in weight unit.
 *
 * @type {number}
 * @private
 */
HandwritingLayout.prototype.heightInWeight_ = 0;


/**
 * The width in weight unit.
 *
 * @type {number}
 * @private
 */
HandwritingLayout.prototype.widthInWeight_ = 0;


/** @override */
HandwritingLayout.prototype.createDom = function() {
  goog.base(this, 'createDom');

  goog.dom.classlist.add(this.getElement(), i18n.input.chrome.inputview.Css.
      HANDWRITING_LAYOUT);
};


/** @override */
HandwritingLayout.prototype.enterDocument = function() {
  goog.base(this, 'enterDocument');

  this.calculate_();
};


/**
 * Gets the first child.
 *
 * @private
 */
HandwritingLayout.prototype.calculate_ = function() {
  var child = /** @type {i18n.input.chrome.inputview.elements.Weightable} */ (
      this.getChildAt(0));
  this.heightInWeight_ = child.getHeightInWeight();
  this.widthInWeight_ = child.getWidthInWeight();
};


/** @override */
HandwritingLayout.prototype.getHeightInWeight = function() {
  return this.heightInWeight_;
};


/** @override */
HandwritingLayout.prototype.getWidthInWeight = function() {
  return this.widthInWeight_;
};


/** @override */
HandwritingLayout.prototype.resize = function(width, height) {
  goog.base(this, 'resize', width, height);
  for (var i = 0; i < this.getChildCount(); i++) {
    var child = /** @type {i18n.input.chrome.inputview.elements.Element} */ (
        this.getChildAt(i));
    child.resize(
        Math.ceil(width * child.getWidthInWeight() / this.widthInWeight_),
        Math.ceil(height * child.getHeightInWeight() / this.heightInWeight_));
    if (!i18n.input.chrome.inputview.GlobalFlags.isQPInputView) {
      // 85/140 = 0.6
      child.getElement().style.top =
          Math.ceil(height * 0.6 / this.heightInWeight_);
    }
  }
};
});  // goog.scope
