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
goog.provide('i18n.input.chrome.inputview.elements.content.AltDataView');

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
var ElementType = i18n.input.chrome.inputview.elements.ElementType;


/**
 * Converts cooridnate in the keyboard window to coordinate in screen.
 *
 * @param {goog.math.Coordinate} coordinate The coordinate in keyboard window.
 * @return {goog.math.Coordinate} The cooridnate in screen.
 */
function convertToScreenCoordinate(coordinate) {
  var screenCoordinate = coordinate.clone();
  // This is a hack. Ideally, we should be able to use window.screenX/Y to get
  // cooridnate of the top left corner of VK window. But VK window is special
  // and the values are set to 0 all the time. We should fix the problem in
  // Chrome.
  screenCoordinate.y += screen.height - window.innerHeight;
  return screenCoordinate;
};


/**
 * The view for alt data.
 *
 * @param {goog.events.EventTarget=} opt_eventTarget The parent event target.
 * @constructor
 * @extends {i18n.input.chrome.inputview.elements.Element}
 */
i18n.input.chrome.inputview.elements.content.AltDataView = function(
    opt_eventTarget) {
  goog.base(this, '', ElementType.ALTDATA_VIEW, opt_eventTarget);

  /**
   * The alternative elements.
   *
   * @type {!Array.<!Element>}
   * @private
   */
  this.altdataElements_ = [];

  /**
   * The window that shows accented characters.
   *
   * @type {Object}
   * @private
   */
  this.altdataWindow_ = null;
};
goog.inherits(i18n.input.chrome.inputview.elements.content.AltDataView,
    i18n.input.chrome.inputview.elements.Element);
var AltDataView = i18n.input.chrome.inputview.elements.content.AltDataView;


/**
 * The padding between the alt data view and the key.
 *
 * @type {number}
 * @private
 */
AltDataView.PADDING_ = 6;


/**
 * The URL of the window which displays accented characters.
 *
 * @type {string}
 * @private
 */
AltDataView.ACCENTS_WINDOW_URL_ = 'imewindows/accents.html';


/**
 * Index of highlighted accent. Use this index to represent no highlighted
 * accent.
 *
 * @type {number}
 * @private
 */
AltDataView.INVALIDINDEX_ = -1;


/**
 * The distance between finger to altdata view which will cancel the altdata
 * view.
 *
 * @type {number}
 * @private
 */
AltDataView.FINGER_DISTANCE_TO_CANCEL_ALTDATA_ = 100;


/**
 * The cover element.
 * Note: The reason we use a separate cover element instead of the view is
 * because of the opacity. We can not reassign the opacity in child element.
 *
 * @type {!Element}
 * @private
 */
AltDataView.prototype.coverElement_;


/**
 * The index of the alternative element which is highlighted.
 *
 * @type {number}
 * @private
 */
AltDataView.prototype.highlightIndex_ = AltDataView.INVALIDINDEX_;


/**
 * The key which trigger this alternative data view.
 *
 * @type {!i18n.input.chrome.inputview.elements.content.SoftKey}
 */
AltDataView.prototype.triggeredBy;


/**
 * True if create IME window to show accented characters.
 *
 * @type {boolean}
 * @private
 */
AltDataView.prototype.enableIMEWindow_ = false;


/** @override */
AltDataView.prototype.createDom = function() {
  goog.base(this, 'createDom');

  var dom = this.getDomHelper();
  var elem = this.getElement();
  goog.dom.classlist.add(elem, i18n.input.chrome.inputview.Css.ALTDATA_VIEW);
  this.coverElement_ = dom.createDom(goog.dom.TagName.DIV,
      i18n.input.chrome.inputview.Css.ALTDATA_COVER);
  dom.appendChild(document.body, this.coverElement_);
  goog.style.setElementShown(this.coverElement_, false);

  this.coverElement_['view'] = this;
};


/** @override */
AltDataView.prototype.enterDocument = function() {
  goog.base(this, 'enterDocument');

  goog.style.setElementShown(this.getElement(), false);
};


/**
 * Shows the alt data viwe.
 *
 * @param {!i18n.input.chrome.inputview.elements.content.SoftKey} key The key
 *     triggerred this altdata view.
 * @param {boolean} isRTL Whether to show the key characters as RTL.
 * @param {boolean} enableIMEWindow Whether to enable IME window for accented
 *     characters.
 */
AltDataView.prototype.show = function(key, isRTL, enableIMEWindow) {
  this.triggeredBy = key;
  this.enableIMEWindow_ = enableIMEWindow;
  var coordinate = goog.style.getClientPosition(key.getElement());
  var x = coordinate.x;
  var y = coordinate.y;
  var width = key.availableWidth;
  var height = key.availableHeight;
  var characters;
  if (key.type == ElementType.CHARACTER_KEY) {
    key = /** @type {!i18n.input.chrome.inputview.elements.content.
        CharacterKey} */ (key);
    characters = key.getAltCharacters();
  } else if (key.type == ElementType.COMPACT_KEY) {
    key = /** @type {!i18n.input.chrome.inputview.elements.content.
        CompactKey} */ (key);
    characters = key.getMoreCharacters();
    if (key.hintText) {
      goog.array.insertAt(characters, key.hintText, 0);
    }
  }
  if (!characters || characters.length == 0) {
    return;
  }

  goog.style.setElementShown(this.getElement(), true);
  this.getDomHelper().removeChildren(this.getElement());
  // The total width of the characters + the separators, every separator has
  // width = 1.
  var altDataWidth = width * characters.length;

  var left = coordinate.x;
  if ((left + altDataWidth) > screen.width) {
    // If no enough space at the right, then make it to the left.
    left = coordinate.x + width - altDataWidth;
    characters.reverse();
  }

  var elemTop = coordinate.y - height - AltDataView.PADDING_;

  if (this.enableIMEWindow_) {
    var screenCoordinate = convertToScreenCoordinate(
        new goog.math.Coordinate(left, elemTop));
    if (this.altdataWindow_)
      this.altdataWindow_.close();
    var self = this;
    var screenBounds = goog.object.create('left', screenCoordinate.x,
        'top', screenCoordinate.y, 'width', altDataWidth, 'height', height);
    inputview.createWindow(
        chrome.runtime.getURL(AltDataView.ACCENTS_WINDOW_URL_),
        goog.object.create('outerBounds', screenBounds, 'frame', 'none',
            'hidden', true),
        function(newWindow) {
          self.altdataWindow_ = newWindow;
          var contentWindow = self.altdataWindow_.contentWindow;
          contentWindow.addEventListener('load', function() {
            contentWindow.accents.setAccents(characters);
            contentWindow.accents.highlightItem(
                Math.ceil(coordinate.x + width / 2),
                Math.ceil(coordinate.y + height / 2));
            self.altdataWindow_.show();
          });
        });
  } else {
    if (elemTop < 0) {
      // If no enough top space, then display below the key.
      elemTop = coordinate.y + height + AltDataView.PADDING_;
    }

    for (var i = 0; i < characters.length; i++) {
      var keyElem = this.addKey_(characters[i], isRTL);
      goog.style.setSize(keyElem, width, height);
      this.altdataElements_.push(keyElem);
      if (i != characters.length - 1) {
        this.addSeparator_(height);
      }
    }
    goog.style.setPosition(this.getElement(), left, elemTop);
    this.highlightItem(Math.ceil(coordinate.x + width / 2),
                       Math.ceil(coordinate.y + height / 2));
  }

  goog.style.setElementShown(this.coverElement_, true);
  this.triggeredBy.setHighlighted(true);
};


/**
 * Hides the alt data view.
 */
AltDataView.prototype.hide = function() {
  if (this.enableIMEWindow_) {
    this.altdataWindow_.close();
    this.altdataWindow_ = null;
  } else {
    this.altdataElements_ = [];
  }
  this.triggeredBy.setHighlighted(false);
  goog.style.setElementShown(this.getElement(), false);
  goog.style.setElementShown(this.coverElement_, false);
  this.highlightIndex_ = AltDataView.INVALIDINDEX_;
};


/**
 * Highlights the item according to the current coordinate of the finger.
 *
 * @param {number} x .
 * @param {number} y .
 */
AltDataView.prototype.highlightItem = function(x, y) {
  if (this.enableIMEWindow_) {
    if (this.altdataWindow_) {
      var screenCoordinate = convertToScreenCoordinate(
          new goog.math.Coordinate(x, y));
      this.altdataWindow_.contentWindow.accents.highlightItem(
          screenCoordinate.x, screenCoordinate.y);
    }
  } else {
    for (var i = 0; i < this.altdataElements_.length; i++) {
      var elem = this.altdataElements_[i];
      var coordinate = goog.style.getClientPosition(elem);
      var size = goog.style.getSize(elem);
      if (coordinate.x < x && (coordinate.x + size.width) > x) {
        this.highlightIndex_ = i;
        this.clearAllHighlights_();
        this.setElementBackground_(elem, true);
      }
      var verticalDist = Math.min(Math.abs(y - coordinate.y),
          Math.abs(y - coordinate.y - size.height));
      if (verticalDist > AltDataView.
          FINGER_DISTANCE_TO_CANCEL_ALTDATA_) {
        this.hide();
        return;
      }
    }
  }
};


/**
 * Clears all the highlights.
 *
 * @private
 */
AltDataView.prototype.clearAllHighlights_ =
    function() {
  for (var i = 0; i < this.altdataElements_.length; i++) {
    this.setElementBackground_(this.altdataElements_[i], false);
  }
};


/**
 * Sets the background style of the element.
 *
 * @param {!Element} element The element.
 * @param {boolean} highlight True to highlight the element.
 * @private
 */
AltDataView.prototype.setElementBackground_ =
    function(element, highlight) {
  if (highlight) {
    goog.dom.classlist.add(element, i18n.input.chrome.inputview.Css.
        ELEMENT_HIGHLIGHT);
  } else {
    goog.dom.classlist.remove(element, i18n.input.chrome.inputview.Css.
        ELEMENT_HIGHLIGHT);
  }
};


/**
 * Gets the highlighted character.
 *
 * @return {string} The character.
 */
AltDataView.prototype.getHighlightedCharacter = function() {
  if (this.enableIMEWindow_) {
    return this.altdataWindow_.contentWindow.accents.highlightedAccent();
  } else {
    return goog.dom.getTextContent(this.altdataElements_[this.highlightIndex_]);
  }
};


/**
 * Adds a alt data key into the view.
 *
 * @param {string} character The alt character.
 * @param {boolean} isRTL Whether to show the character as RTL.
 * @return {!Element} The create key element.
 * @private
 */
AltDataView.prototype.addKey_ = function(character, isRTL) {
  var dom = this.getDomHelper();
  var keyElem = dom.createDom(goog.dom.TagName.DIV,
      i18n.input.chrome.inputview.Css.ALTDATA_KEY,
      i18n.input.chrome.inputview.util.getVisibleCharacter(character));
  keyElem.style.direction = isRTL ? 'rtl' : 'ltr';
  dom.appendChild(this.getElement(), keyElem);
  return keyElem;
};


/**
 * Adds a separator.
 *
 * @param {number} height .
 * @private
 */
AltDataView.prototype.addSeparator_ = function(height) {
  var dom = this.getDomHelper();
  var tableCell = dom.createDom(goog.dom.TagName.DIV,
      i18n.input.chrome.inputview.Css.TABLE_CELL);
  tableCell.style.height = height + 'px';
  var separator = dom.createDom(goog.dom.TagName.DIV,
      i18n.input.chrome.inputview.Css.ALTDATA_SEPARATOR);
  dom.appendChild(tableCell, separator);
  dom.appendChild(this.getElement(), tableCell);
};


/**
 * Gets the cover element.
 *
 * @return {!Element} The cover element.
 */
AltDataView.prototype.getCoverElement = function() {
  return this.coverElement_;
};


/** @override */
AltDataView.prototype.resize = function(width, height) {
  goog.base(this, 'resize', width, height);

  goog.style.setSize(this.coverElement_, width, height);
};


/** @override */
AltDataView.prototype.disposeInternal = function() {
  this.getElement()['view'] = null;

  goog.base(this, 'disposeInternal');
};

});  // goog.scope
