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
goog.provide('i18n.input.chrome.inputview.elements.content.DragButton');

goog.require('goog.a11y.aria.State');
goog.require('i18n.input.chrome.FloatingWindowDragger');
goog.require('i18n.input.chrome.inputview.elements.content.FloatingVKButton');

goog.scope(function() {



/**
 * The drag button.
 *
 * @param {string} id The id.
 * @param {!i18n.input.chrome.inputview.elements.ElementType} type The element
 *     type.
 * @param {string} iconCssClass The css class for the icon.
 * @param {goog.events.EventTarget=} opt_eventTarget The event target.
 * @constructor
 * @extends {i18n.input.chrome.inputview.elements.content.FloatingVKButton}
 */
i18n.input.chrome.inputview.elements.content.DragButton = function(id, type,
    iconCssClass, opt_eventTarget) {
  goog.base(this, id, type, iconCssClass, opt_eventTarget);

  this.pointerConfig.stopEventPropagation = false;
};
goog.inherits(i18n.input.chrome.inputview.elements.content.DragButton,
    i18n.input.chrome.inputview.elements.content.FloatingVKButton);
var DragButton = i18n.input.chrome.inputview.elements.content.DragButton;


/** @private {!i18n.input.chrome.FloatingWindowDragger} */
DragButton.prototype.dragger_;


/** @override */
DragButton.prototype.createDom = function() {
  goog.base(this, 'createDom');
  var ariaLabel = chrome.i18n.getMessage('DRAG_BUTTON');
  var elem = this.getElement();
  goog.a11y.aria.setState(/** @type {!Element} */ (elem),
      goog.a11y.aria.State.LABEL, ariaLabel);
};


/** @override */
DragButton.prototype.enterDocument = function() {
  goog.base(this, 'enterDocument');
  this.dragger_ = new i18n.input.chrome.FloatingWindowDragger(window,
      this.getElement());
};


});
