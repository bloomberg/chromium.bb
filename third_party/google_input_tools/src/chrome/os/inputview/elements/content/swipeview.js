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
goog.provide('i18n.input.chrome.inputview.elements.content.SwipeView');

goog.require('goog.array');
goog.require('goog.dom');
goog.require('goog.dom.TagName');
goog.require('goog.dom.classlist');
goog.require('goog.events.EventType');
goog.require('goog.math.Coordinate');
goog.require('goog.style');
goog.require('i18n.input.chrome.inputview.Accents');
goog.require('i18n.input.chrome.inputview.Css');
goog.require('i18n.input.chrome.inputview.SwipeDirection');
goog.require('i18n.input.chrome.inputview.elements.Element');
goog.require('i18n.input.chrome.inputview.elements.ElementType');
goog.require('i18n.input.chrome.inputview.events.EventType');
goog.require('i18n.input.chrome.inputview.events.KeyCodes');
goog.require('i18n.input.chrome.inputview.handler.PointerHandler');
goog.require('i18n.input.chrome.inputview.util');
goog.require('i18n.input.chrome.message.ContextType');


goog.scope(function() {
var ContextType = i18n.input.chrome.message.ContextType;
var Css = i18n.input.chrome.inputview.Css;
var ElementType = i18n.input.chrome.inputview.elements.ElementType;
var EventType = i18n.input.chrome.inputview.events.EventType;
var KeyCodes = i18n.input.chrome.inputview.events.KeyCodes;
var content = i18n.input.chrome.inputview.elements.content;
var util = i18n.input.chrome.inputview.util;


/**
 * The view for alt data.
 *
 * @param {!i18n.input.chrome.inputview.Adapter} adapter .
 * @param {goog.events.EventTarget=} opt_eventTarget The parent event target.
 * @constructor
 * @extends {i18n.input.chrome.inputview.elements.Element}
 */
i18n.input.chrome.inputview.elements.content.SwipeView = function(
    adapter, opt_eventTarget) {
  i18n.input.chrome.inputview.elements.content.SwipeView.base(
      this, 'constructor', '', ElementType.SWIPE_VIEW, opt_eventTarget);

  /**
   * The inputview adapter.
   *
   * @private {!i18n.input.chrome.inputview.Adapter}
   */
  this.adapter_ = adapter;


  /**
   * The swipe elements.
   *
   * @private {!Array.<!Element>}
   */
  this.trackElements_ = [];

  /**
   * The window that shows the swipe field.
   *
   * @private {Object}
   */
  this.trackWindow_ = null;

  /**
   * The text before the current focus.
   *
   * @private {string}
   */
  this.surroundingText_ = '';

  /**
   * The index of focus in the surrounding text.
   *
   * @private {number}
   */
  this.surroundingTextFocus_ = 0;

  /**
   * The index of the anchor in the surrounding text.
   *
   * @private {number}
   */
  this.surroundingTextAnchor_ = 0;

  /**
   * Recent words that have been deleted.
   *
   * @private {Array.<string>}
   */
  this.deletedWords_ = [];

  /**
   * The pointer handler.
   *
   * @type {!i18n.input.chrome.inputview.handler.PointerHandler}
   * @private
   */
  this.pointerHandler_ = new i18n.input.chrome.inputview.handler.
      PointerHandler();

};
goog.inherits(i18n.input.chrome.inputview.elements.content.SwipeView,
    i18n.input.chrome.inputview.elements.Element);
var SwipeView = i18n.input.chrome.inputview.elements.content.SwipeView;


/**
 * The number of swipe elements.
 *
 * @type {number}
 * @private
 */
SwipeView.LENGTH_ = 7;


/**
 * Index of highlighted accent. Use this index to represent no highlighted
 * accent.
 *
 * @type {number}
 * @private
 */
SwipeView.INVALIDINDEX_ = -1;


/**
 * The distance between finger to track view which will cancel the track
 * view.
 *
 * @type {number}
 * @private
 */
SwipeView.FINGER_DISTANCE_TO_CANCEL_SWIPE_ = 20;


/**
 * The cover element.
 * Note: The reason we use a separate cover element instead of the view is
 * because of the opacity. We can not reassign the opacity in child element.
 *
 * @type {!Element}
 * @private
 */
SwipeView.prototype.coverElement_;


/**
 * The index of the alternative element which is highlighted.
 *
 * @type {number}
 * @private
 */
SwipeView.prototype.highlightIndex_ = SwipeView.INVALIDINDEX_;


/**
 * The key which trigger this alternative data view.
 *
 * @type {!i18n.input.chrome.inputview.elements.content.SoftKey}
 */
SwipeView.prototype.triggeredBy;


/**
 * Whether finger movement is being tracked.
 *
 * @type {boolean}
 * @private
 */
SwipeView.prototype.tracking_ = false;


/**
 * Whether to deploy the tracker on swipe events.
 *
 * @type {boolean}
 * @private
 */
SwipeView.prototype.armed_ = false;


/**
 * Whether the tracker will be deployed on future swipe events.
 *
 * @return {boolean}
 */
SwipeView.prototype.isArmed = function() {
  return this.armed_;
};


/**
 * Callback when surrounding text is changed.
 *
 * @param {!i18n.input.chrome.inputview.events.SurroundingTextChangedEvent} e .
 * @private
 */
SwipeView.prototype.onSurroundingTextChanged_ = function(e) {
  if (this.adapter_.isPasswordBox()) {
    this.surroundingText_ = '';
    this.surroundingTextAnchor_ = this.surroundingTextFocus_ = 0;
    return;
  }

  this.surroundingTextAnchor_ = e.anchor;
  this.surroundingTextFocus_ = e.focus;

  var text = e.text || '';
  var oldText = this.surroundingText_;
  var diff = '';
  if (util.isLetterDelete(oldText, text)) {
    diff = oldText.slice(-1);
  } else if (util.isLetterRestore(oldText, text)) {
    // Letter restore.
    // Handle blink bug where ctrl+delete deletes a space and inserts
    // a &nbsp.
    // Convert &nbsp to ' ' and remove from delete words since blink
    // did a minirestore for us.
    var letter = text[text.length - 1];
    if (letter == String.fromCharCode(160) || letter == ' ') {
      var lastDelete = this.deletedWords_.pop();
      var firstChar = lastDelete && lastDelete[0] || '';
      if (firstChar == String.fromCharCode(160) || firstChar == ' ') {
        this.deletedWords_.push(lastDelete.slice(1));
      }
    }
  } else if (e.text.length == 100 || oldText.length == 100) {
    // Check if a word was deleted from oldText.
      var candidate = oldText.trim().split(' ').pop();
      if (util.isPossibleDelete(oldText, text, candidate)) {
        var location = oldText.lastIndexOf(candidate);
        var intersectingText = oldText.slice(0, location);
        diff = oldText.slice(location);
      }
  } else {
    diff = oldText.substring(text.length);
  }
  if (diff) {
    this.deletedWords_.push(diff);
  // Do not reset while swiping.
  } else if (!this.isVisible()) {
    this.deletedWords_ = [];
  }
  this.surroundingText_ = text;
};


/**
 * Handles the swipe action.
 *
 * @param {!i18n.input.chrome.inputview.events.SwipeEvent} e The swipe event.
 * @private
 */
SwipeView.prototype.handleSwipeAction_ = function(e) {
  var direction = e.direction;
  if (this.isVisible()) {
    if (e.view.type == ElementType.BACKSPACE_KEY) {
      // Cache whether we were tracking.
      var alreadyTracking = this.tracking_;
      var changed = this.highlightItem(e.x, e.y);
      // Did not move segments.
      if (!changed) {
        // First gesture.
        if (!alreadyTracking) {
          // All previous deletions count as one now.
          this.deletedWords_.reverse();
          var word = this.deletedWords_.join('');
          this.deletedWords_ = [word];
          // Swiped right, cancel the deletion.
          if (direction & i18n.input.chrome.inputview.SwipeDirection.RIGHT) {
            word = this.deletedWords_.pop();
              if (word)
                this.adapter_.commitText(word);
          }
        }
        return;
      }

      if (direction & i18n.input.chrome.inputview.SwipeDirection.LEFT) {
        this.adapter_.sendKeyDownAndUpEvent(
            '\u0008', KeyCodes.BACKSPACE, undefined, undefined, {
              ctrl: true,
              shift: false
        });
      } else if (direction & i18n.input.chrome.inputview.SwipeDirection.RIGHT) {
        var word = this.deletedWords_.pop();
        if (word)
          this.adapter_.commitText(word);
        // Restore text we deleted before the track came up, but part of the
        // same gesture.
        if (this.isAtOrigin()) {
          word = this.deletedWords_.pop();
          if (word)
            this.adapter_.commitText(word);
        }
      }
      return;
    }
    if (e.view.type == ElementType.SELECT_VIEW) {
      // Cache whether we were tracking as highlight may change this.
      var alreadyTracking = this.tracking_;
      var changed = this.highlightItem(e.x, e.y);
      // First finger movement is onto the blank track. Ignore.
      if (!alreadyTracking)
        return;
      if (!changed)
        return;
      var index = this.getTrackIndex();
      if (index == -1) {
        console.error('Invalid track index.');
        return;
      }
      var selectWord = index % 2 == 1;
      var code;
      if (direction & i18n.input.chrome.inputview.SwipeDirection.LEFT) {
        code = KeyCodes.ARROW_LEFT;
      } else if (direction & i18n.input.chrome.inputview.SwipeDirection.RIGHT) {
        code = KeyCodes.ARROW_RIGHT;
      } else {
        return;
      }
      // If anchor == focus we are either at the end or the start of the word
      // and no selection is in place.
      if (this.surroundingTextAnchor_ == this.surroundingTextFocus_) {
        // Do not move carat at all, as this will either have no effect or cause
        // us to splice the word.
        if (!selectWord) {
         return;
        }
      }
      this.adapter_.sendKeyDownAndUpEvent(
          '', code, undefined, undefined, {
              ctrl: selectWord,
              shift: selectWord
      });
      return;
    }
    return;
  }

  // User swiped on backspace key before swipeview was visible.
  if (e.view.type == ElementType.BACKSPACE_KEY) {
    if (!this.armed_) {
      // Prevents reshowing the track after it is hidden as part of the same
      // finger movement.
      return;
    }
    if (e.direction & i18n.input.chrome.inputview.SwipeDirection.LEFT) {
      var key = /** @type {!content.FunctionalKey} */ (e.view);
      // Equiv to a longpress.
      this.showDeletionTrack(key);
    }
    return;
  }
};


/**
 * Handles the pointer action.
 *
 * @param {!i18n.input.chrome.inputview.events.PointerEvent} e .
 * @private
 */
SwipeView.prototype.handlePointerAction_ = function(e) {
  if (!e.view) {
    return;
  }
  switch (e.view.type) {
    case ElementType.BACKSPACE_KEY:
      var key = /** @type {!content.FunctionalKey} */ (e.view);
      if (e.type == EventType.POINTER_DOWN) {
        if (this.adapter_.contextType != ContextType.URL) {
          this.armed_ = true;
        }
        this.deletedWords_ = [];
      } else if (e.type == EventType.POINTER_UP || e.type == EventType.
          POINTER_OUT) {
        if (!this.isVisible()) {
          this.armed_ = false;
        }
      } else if (e.type == EventType.LONG_PRESS) {
        if (this.adapter_.isGestureDeletionEnabled()) {
          this.showDeletionTrack(key);
        }
      }
      break;
    case ElementType.SWIPE_VIEW:
      if (e.type == EventType.POINTER_DOWN &&
          e.target == this.getCoverElement()) {
        this.hide();
      } else if (e.type == EventType.POINTER_UP ||
                 e.type == EventType.POINTER_OUT) {
        this.hide();
        // Reset the deleted words.
        this.deletedWords_ = [];
      }
      break;
    case ElementType.SELECT_VIEW:
      if (e.type == EventType.POINTER_DOWN) {
        this.showSelectionTrack(e.x, e.y);
      }
      if (e.type == EventType.POINTER_UP) {
        this.hide();
      }
      break;
  }
};

/** @override */
SwipeView.prototype.createDom = function() {
  goog.base(this, 'createDom');

  var dom = this.getDomHelper();
  var elem = this.getElement();
  goog.dom.classlist.add(elem, i18n.input.chrome.inputview.Css.SWIPE_VIEW);
  this.coverElement_ = dom.createDom(goog.dom.TagName.DIV,
      i18n.input.chrome.inputview.Css.TRACK_COVER);
  dom.appendChild(document.body, this.coverElement_);
  goog.style.setElementShown(this.coverElement_, false);

  this.coverElement_['view'] = this;
};


/** @override */
SwipeView.prototype.enterDocument = function() {
  goog.base(this, 'enterDocument');
  this.getHandler().
    listen(this.adapter_,
        i18n.input.chrome.inputview.events.EventType.
            SURROUNDING_TEXT_CHANGED,
        this.onSurroundingTextChanged_).
    listen(this.pointerHandler_, [
      EventType.SWIPE], this.handleSwipeAction_).
    listen(this.pointerHandler_, [
          EventType.LONG_PRESS,
          EventType.POINTER_UP,
          EventType.POINTER_DOWN,
          EventType.POINTER_OUT], this.handlePointerAction_);
  goog.style.setElementShown(this.getElement(), false);
};


/**
 * Shows the swipe tracker.
 *
 * @param {number} x
 * @param {number} y
 * @param {number} width The width of a key.
 * @param {number} height The height of a key.
 * @param {number} firstTrackWidth The width of the first key.
 * @param {number} firstSegmentWidth The width of the first buffer segment.
 * @param {string=} opt_character Characters on each key.
 * @param {Css=} opt_css Optional icon css class.
 * @private
 */
SwipeView.prototype.showDeletionTrack_ = function(x, y, width, height,
    firstTrackWidth, firstSegmentWidth, opt_character, opt_css) {
  this.tracking_ = false;
  goog.style.setElementShown(this.getElement(), true);
  this.getDomHelper().removeChildren(this.getElement());
  // Each key except last has a separator.
  var totalWidth = ((2 * SwipeView.LENGTH_) - 3) * width;
  totalWidth += firstTrackWidth + firstSegmentWidth;

  this.ltr = true;
  this.highlightIndex_ = 0;
  if ((x + totalWidth) > screen.width) {
    // If not enough space at the right, then make it to the left.
    x -= totalWidth;
    this.ltr = false;
    this.highlightIndex_ = SwipeView.LENGTH_ - 1;
  }
  if (firstTrackWidth == 0) {
    this.highlightIndex_ = SwipeView.INVALIDINDEX_;
  }
  var ltr = this.ltr;
  var isFirstSegment = function(i) {
    return ltr ? i == 0 : i == SwipeView.LENGTH_ - 2;
  };
  var isFirstTrackPiece = function(i) {
    return ltr ? i == 0 : i == SwipeView.LENGTH_ - 1;
  };
  for (var i = 0; i < SwipeView.LENGTH_; i++) {
    var trackWidth = isFirstTrackPiece(i) ? firstTrackWidth : width;
    if (trackWidth != 0) {
      var keyElem = this.addKey_(opt_character, opt_css);
      goog.style.setSize(keyElem, trackWidth, height);
      this.trackElements_.push(keyElem);
    }

    if (i != (SwipeView.LENGTH_ - 1)) {
      var segmentWidth = isFirstSegment(i) ? firstSegmentWidth : width;
      this.addSeparator_(segmentWidth, height);
    }
  }
  goog.style.setPosition(this.getElement(), x, y);
  // Highlight selected element if it's index is valid.
  if (this.highlightIndex_ != SwipeView.INVALIDINDEX_) {
    var elem = this.trackElements_[this.highlightIndex_];
    this.setElementBackground_(elem, true);
  }
  goog.style.setElementShown(this.coverElement_, true);
  this.triggeredBy && this.triggeredBy.setHighlighted(true);
};


/**
 *
 * Shows the swipe tracker.
 * @param {number} x
 * @param {number} y
 * @param {number} width The width of a key.
 * @param {number} height The height of a key.
 * @param {string} character Characters on each key.
 * @private
 */
SwipeView.prototype.showSelectionTrack_ = function(x, y, width, height,
    character) {
  this.tracking_ = false;
  goog.style.setElementShown(this.getElement(), true);
  this.getDomHelper().removeChildren(this.getElement());
  // Each key has a separator.
  var totalWidth = ((2 * SwipeView.LENGTH_)) * width;

  this.ltr = true;
  this.highlightIndex_ = SwipeView.INVALIDINDEX_;
  if ((x + totalWidth) > screen.width) {
    // If not enough space at the right, then make it to the left.
    x -= totalWidth;
    this.ltr = false;
  }

  for (var i = 0; i < SwipeView.LENGTH_; i++) {
    var keyElem;
    if (!this.ltr) {
      keyElem = this.addKey_(character);
      goog.style.setSize(keyElem, width, height);
      this.trackElements_.push(keyElem);
    }

    keyElem = this.addSeparator_(width, height);
    goog.style.setSize(keyElem, width, height);
    this.trackElements_.push(keyElem);

    if (this.ltr) {
      keyElem = this.addKey_(character);
      goog.style.setSize(keyElem, width, height);
      this.trackElements_.push(keyElem);
    }
  }
  goog.style.setPosition(this.getElement(), x, y);
  goog.style.setElementShown(this.coverElement_, true);
  this.triggeredBy && this.triggeredBy.setHighlighted(true);
};


/**
 * Shows the alt data view.
 *
 * @param {!i18n.input.chrome.inputview.elements.content.SoftKey} key
 *   The key triggered this track view.
 */
SwipeView.prototype.showDeletionTrack = function(key) {
  this.triggeredBy = key;
  var coordinate = goog.style.getClientPosition(key.getElement());
  if (key.type == ElementType.BACKSPACE_KEY) {
    this.showDeletionTrack_(
      coordinate.x + key.availableWidth,
      coordinate.y,
      70,
      key.availableHeight,
      key.availableWidth,
      100,
      undefined,
      Css.BACKSPACE_ICON);
  }
};


/**
 * Shows the selection track.
 *
 * @param {number} x
 * @param {number} y
 */
SwipeView.prototype.showSelectionTrack = function(x, y) {
  var ltr = x <= (screen.width / 2);

  this.showSelectionTrack_(
    ltr ? 0 : screen.width,
    Math.max(y - 35, 35), // Center track on finger press but force containment.
    70,
    70,
    x > (screen.width / 2) ? '<' : '>');
};


/**
 * Hides the alt data view.
 */
SwipeView.prototype.hide = function() {
  this.armed_ = false;
  this.trackElements_ = [];
  this.tracking_ = false;
  if (this.triggeredBy)
    this.triggeredBy.setHighlighted(false);
  goog.style.setElementShown(this.getElement(), false);
  goog.style.setElementShown(this.coverElement_, false);
  this.highlightIndex_ = SwipeView.INVALIDINDEX_;
};


/**
 * Whether the current track counter is at the first element.
 *
 * @return {boolean}
 */
SwipeView.prototype.isAtOrigin = function() {
  return this.ltr ? this.highlightIndex_ == 0 :
      this.highlightIndex_ == SwipeView.LENGTH_ - 1;
};


/**
 * Highlights the item according to the current coordinate of the finger.
 *
 * @param {number} x .
 * @param {number} y .
 * @return {boolean} Whether it passed into a new segment.
 */
SwipeView.prototype.highlightItem = function(x, y) {
  var previousIndex = this.highlightIndex_;
  for (var i = 0; i < this.trackElements_.length; i++) {
    var elem = this.trackElements_[i];
    var coordinate = goog.style.getClientPosition(elem);
    var size = goog.style.getSize(elem);
    if (coordinate.x < x && (coordinate.x + size.width) > x) {
      this.highlightIndex_ = i;
      this.clearAllHighlights_();
      this.setElementBackground_(elem, true);
    }
  }
  this.tracking_ = this.tracking_ || (previousIndex != this.highlightIndex_);
  return (previousIndex != this.highlightIndex_);
};


/**
 * Clears all the highlights.
 *
 * @private
 */
SwipeView.prototype.clearAllHighlights_ =
    function() {
  for (var i = 0; i < this.trackElements_.length; i++) {
    this.setElementBackground_(this.trackElements_[i], false);
  }
};


/**
 * Sets the background style of the element.
 *
 * @param {!Element} element The element.
 * @param {boolean} highlight True to highlight the element.
 * @private
 */
SwipeView.prototype.setElementBackground_ =
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
 * Adds a alt data key into the view.
 *
 * @param {string=} opt_character The character.
 * @param {Css=} opt_icon_css
 * @return {!Element} The create key element.
 * @private
 */
SwipeView.prototype.addKey_ = function(opt_character, opt_icon_css) {
  var dom = this.getDomHelper();
  var character = opt_character &&
      i18n.input.chrome.inputview.util.getVisibleCharacter(opt_character);
  var keyElem;
  if (character)
    keyElem = dom.createDom(goog.dom.TagName.DIV, Css.SWIPE_KEY, character);
  else
    keyElem = dom.createDom(goog.dom.TagName.DIV, Css.SWIPE_KEY);
  if (opt_icon_css) {
    var child = dom.createDom(goog.dom.TagName.DIV, opt_icon_css);
    dom.appendChild(keyElem, child);
  }
  dom.appendChild(this.getElement(), keyElem);
  return keyElem;
};


/**
 * Adds a separator.
 *
 * @param {number} width .
 * @param {number} height .
 * @return {Element}
 * @private
 */
SwipeView.prototype.addSeparator_ = function(width, height) {
  var dom = this.getDomHelper();
  var tableCell = dom.createDom(goog.dom.TagName.DIV,
      i18n.input.chrome.inputview.Css.TABLE_CELL);
  goog.style.setSize(tableCell, width + 'px', height + 'px');
  goog.dom.classlist.add(tableCell, Css.SWIPE_SEPARATOR);
  dom.appendChild(this.getElement(), tableCell);
  return tableCell;
};


/**
 * Gets the cover element.
 *
 * @return {!Element} The cover element.
 */
SwipeView.prototype.getCoverElement = function() {
  return this.coverElement_;
};


/**
 * The current index.
 * @return {number}
 */
SwipeView.prototype.getTrackIndex = function() {
  if (this.highlightIndex_ == SwipeView.INVALIDINDEX_)
    return SwipeView.INVALIDINDEX_;
  if (this.ltr)
    return this.highlightIndex_;
  return this.trackElements_.length - this.highlightIndex_ - 1;
};


/** @override */
SwipeView.prototype.resize = function(width, height) {
  goog.base(this, 'resize', width, height);

  goog.style.setSize(this.coverElement_, width, height);
};


/**
 * Resets the swipeview.
 *
 */
SwipeView.prototype.reset = function() {
  this.deletedWords_ = [];
  this.surroundingText_ = '';
  this.hide();
};


/** @override */
SwipeView.prototype.disposeInternal = function() {
  goog.dispose(this.pointerHandler_);

  goog.base(this, 'disposeInternal');
};

});  // goog.scope
