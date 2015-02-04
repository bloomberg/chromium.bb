// Copyright 2015 The ChromeOS IME Authors. All Rights Reserved.
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
goog.provide('i18n.input.chrome.inputview.elements.content.SelectView');

goog.require('goog.array');
goog.require('goog.dom');
goog.require('goog.dom.TagName');
goog.require('goog.dom.classlist');
goog.require('goog.math.Coordinate');
goog.require('goog.style');
goog.require('i18n.input.chrome.inputview.Accents');
goog.require('i18n.input.chrome.inputview.Css');
goog.require('i18n.input.chrome.inputview.elements.Element');
goog.require('i18n.input.chrome.inputview.elements.ElementType');
goog.require('i18n.input.chrome.inputview.util');


goog.scope(function() {
var Css = i18n.input.chrome.inputview.Css;
var ElementType = i18n.input.chrome.inputview.elements.ElementType;
var TagName = goog.dom.TagName;


/**
 * The view for triggering select mode.
 *
 * @param {goog.events.EventTarget=} opt_eventTarget The parent event target.
 * @constructor
 * @extends {i18n.input.chrome.inputview.elements.Element}
 */
i18n.input.chrome.inputview.elements.content.SelectView = function(
    opt_eventTarget) {
  goog.base(this, '', ElementType.SELECT_VIEW, opt_eventTarget);
};
goog.inherits(i18n.input.chrome.inputview.elements.content.SelectView,
    i18n.input.chrome.inputview.elements.Element);
var SelectView = i18n.input.chrome.inputview.elements.content.SelectView;


/**
 * The window that shows the left knob.
 *
 * @private {!Element}
 */
SelectView.prototype.left_;


/**
 * The window that shows the right knob.
 *
 * @private {!Element}
 */
SelectView.prototype.right_;


/**
 * The distance between finger to track view which will cancel the track
 * view.
 *
 * @private {number}
 */
SelectView.WIDTH_ = 25;


/** @override */
SelectView.prototype.createDom = function() {
  goog.base(this, 'createDom');

  var dom = this.getDomHelper();
  var elem = this.getElement();
  var knob = dom.createDom(TagName.DIV, undefined, dom.createTextNode('>'));
  var knobContainer = dom.createDom(TagName.DIV, undefined, knob);
  this.left_ = dom.createDom(TagName.DIV, Css.SELECT_KNOB_LEFT, knobContainer);
  dom.appendChild(this.getElement(), this.left_);

  knob = dom.createDom(TagName.DIV, undefined, dom.createTextNode('<'));
  knobContainer = dom.createDom(TagName.DIV, undefined, knob);
  this.right_ =
      dom.createDom(TagName.DIV, Css.SELECT_KNOB_RIGHT, knobContainer);
  dom.appendChild(this.getElement(), this.right_);
};


/** @override */
SelectView.prototype.resize = function(width, height) {
  goog.base(this, 'resize', width, height);
  this.left_ && goog.style.setStyle(this.left_, {
    'height': height + 'px',
    'width': SelectView.WIDTH_ + 'px'
  });
  this.right_ && goog.style.setStyle(this.right_, {
    'height': height + 'px',
    'width': SelectView.WIDTH_ + 'px',
    'left': width - SelectView.WIDTH_ + 'px'
  });

};


/** @override */
SelectView.prototype.disposeInternal = function() {
  goog.dispose(this.left_);
  goog.dispose(this.right_);

  goog.base(this, 'disposeInternal');
};


/** @override */
SelectView.prototype.setVisible = function(visible) {
  SelectView.base(this, 'setVisible', visible);
  goog.style.setElementShown(this.left_, visible);
  goog.style.setElementShown(this.right_, visible);
};
});  // goog.scope
