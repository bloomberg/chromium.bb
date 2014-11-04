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
goog.provide('i18n.input.chrome.inputview.Accents');

goog.require('goog.dom');
goog.require('goog.style');


goog.scope(function() {
var Accents = i18n.input.chrome.inputview.Accents;


/**
 * The highlighted element.
 *
 * @type {Element}
 * @private
 */
Accents.highlightedItem_ = null;


/**
 * Gets the highlighted character.
 *
 * @return {string} The character.
 * @private
 */
Accents.getHighlightedAccent_ = function() {
  return Accents.highlightedItem_ ? Accents.highlightedItem_.textContent : '';
};


/**
 * Highlights the item according to the current coordinate of the finger.
 *
 * @param {number} x The x position of finger in screen coordinate system.
 * @param {number} y The y position of finger in screen coordinate system.
 * @private
 */
Accents.highlightItem_ = function(x, y) {
  // TODO(bshe): support multiple rows.
  var dom = goog.dom.getDomHelper();
  var child = dom.getFirstElementChild(dom.getElement('container'));
  while (child) {
    var coordinate = goog.style.getClientPosition(child);
    var size = goog.style.getSize(child);
    var screenCoordinate = {};
    screenCoordinate.x = coordinate.x + window.screenX;
    screenCoordinate.y = coordinate.y + window.screenY;

    if (screenCoordinate.x < x && (screenCoordinate.x + size.width) > x &&
        Accents.highlightedItem_ != child) {
      if (Accents.highlightedItem_) {
        Accents.highlightedItem_.classList.remove('highlight');
      }
      Accents.highlightedItem_ = child;
      Accents.highlightedItem_.classList.add('highlight');
      return;
    }
    // TODO(bshe): If y is off by 100, cancel the accented char selection.
    child = dom.getNextElementSibling(child);
  }
};


/**
 * Sets the accents which this window should display.
 *
 * @param {!Array.<string>} accents .
 * @private
 */
Accents.setAccents_ = function(accents) {
  var container = document.createElement('div');
  container.id = 'container';
  container.classList.add('accent-container');
  for (var i = 0; i < accents.length; i++) {
    var keyElem = document.createElement('div');
    var textDiv = document.createElement('div');
    textDiv.textContent = accents[i];
    keyElem.appendChild(textDiv);
    container.appendChild(keyElem);
  }
  document.body.appendChild(container);
};


goog.exportSymbol('accents.highlightedAccent', Accents.getHighlightedAccent_);
goog.exportSymbol('accents.highlightItem', Accents.highlightItem_);
goog.exportSymbol('accents.setAccents', Accents.setAccents_);

});  // goog.scope
