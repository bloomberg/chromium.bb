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
goog.provide('i18n.input.chrome.inputview.elements.content.EmojiView');

goog.require('goog.array');
goog.require('goog.dom.classlist');
goog.require('i18n.input.chrome.inputview.Css');
goog.require('i18n.input.chrome.inputview.SpecNodeName');
goog.require('i18n.input.chrome.inputview.elements.ElementType');
goog.require('i18n.input.chrome.inputview.elements.content.KeysetView');
goog.require('i18n.input.chrome.inputview.events.EventType');
goog.require('i18n.input.chrome.inputview.handler.PointerHandler');


goog.scope(function() {
var ElementType = i18n.input.chrome.inputview.elements.ElementType;
var EventType = i18n.input.chrome.inputview.events.EventType;
var KeysetView = i18n.input.chrome.inputview.elements.content.KeysetView;
var PointerHandler = i18n.input.chrome.inputview.handler.PointerHandler;
var Css = i18n.input.chrome.inputview.Css;
var SpecNodeName = i18n.input.chrome.inputview.SpecNodeName;
var ExtendedLayout = i18n.input.chrome.inputview.elements.layout.ExtendedLayout;
var PageIndicator = i18n.input.chrome.inputview.elements.content.PageIndicator;



/**
 * The emoji view.
 *
 * @param {!Object} keyData The data includes soft key definition and key
 *     mapping.
 * @param {!Object} layoutData The layout definition.
 * @param {string} keyboardCode The keyboard code.
 * @param {string} languageCode The language code.
 * @param {!i18n.input.chrome.inputview.Model} model The model.
 * @param {string} name The Input Tool name.
 * @param {!goog.events.EventTarget=} opt_eventTarget .
 * @param {i18n.input.chrome.inputview.Adapter=} opt_adapter .
 * @constructor
 * @extends {KeysetView}
 */
i18n.input.chrome.inputview.elements.content.EmojiView = function(keyData,
    layoutData, keyboardCode, languageCode, model, name, opt_eventTarget,
    opt_adapter) {
  i18n.input.chrome.inputview.elements.content.EmojiView.base(this,
      'constructor', keyData, layoutData, keyboardCode, languageCode, model,
      name, opt_eventTarget, opt_adapter);

  /**
   * The number of keys per emoji page.
   *
   * @private {number}
   */
  this.keysPerPage_ = 27;

  /**
   * The number of tabbar keys.
   *
   * @private {number}
   */
  this.totalTabbars_ = keyData[SpecNodeName.TEXT].length;

  /**
   * The first page offset of each category.
   *
   * @private {!Array.<number>}
   */
  this.pageOffsets_ = [];

  /**
   * The number of pages for each category.
   *
   * @private {!Array.<number>}
   */
  this.pagesInCategory_ = [];

  // Calculate the emojiPageoffset_ and totalPages_ according to keydata.
  var pageNum = 0;
  for (var i = 0, len = keyData[SpecNodeName.TEXT].length; i < len; ++i) {
    this.pageOffsets_.push(pageNum);
    pageNum += Math.ceil(
        keyData[SpecNodeName.TEXT][i].length / this.keysPerPage_);
  }

  /**
   * The category of each emoji page.
   *
   * @private {!Array.<number>}
   */
  this.pageToCategory_ = [];

  // Calculate the pageToCategory_ according to keydata.
  for (var i = 0, len = keyData[SpecNodeName.TEXT].length; i < len; ++i) {
    var lenJ = Math.ceil(
        keyData[SpecNodeName.TEXT][i].length / this.keysPerPage_);
    for (var j = 0; j < lenJ; ++j) {
      this.pageToCategory_.push(i);
    }
    this.pagesInCategory_.push(lenJ);
  }

  /**
   * The list of recent used emoji.
   *
   * @private {Array.<string>}
   */
  this.recentEmojiList_ = [];

  /**
   * The emoji keys on the recent page.
   *
   * @private {Array.<i18n.input.chrome.inputview.elements.content.EmojiKey>}
   */
  this.recentEmojiKeys_ = [];

  /**
   * The tabbars of the emoji view.
   *
   * @private {Array.<i18n.input.chrome.inputview.elements.content.TabBarKey>}
   */
  this.tabbarKeys_ = [];
};
var EmojiView = i18n.input.chrome.inputview.elements.content.EmojiView;
goog.inherits(EmojiView, KeysetView);


/**
 * The emoji rows of the emoji slider.
 *
 * @private {!i18n.input.chrome.inputview.elements.layout.ExtendedLayout}
 */
EmojiView.prototype.emojiRows_;


/**
 * The indicator of the emoji page index.
 *
 * @private {!i18n.input.chrome.inputview.elements.content.PageIndicator}
 */
EmojiView.prototype.pageIndicator_;


/**
 * Whether it is a drag event.
 *
 * @type {boolean}
 */
EmojiView.prototype.isDragging = false;


/**
 * Whether it is a drag event.
 *
 * @private {number}
 */
EmojiView.prototype.categoryID_ = 0;


/**
 * The timestamp of the last pointer down event.
 *
 * @private {number}
 */
EmojiView.prototype.pointerDownTimeStamp_ = 0;


/**
 * The drag distance of a drag event.
 *
 * @private {number}
 */
EmojiView.prototype.dragDistance_ = 0;


/**
 * The maximal required time interval for quick emoji page swipe in ms.
 *
 * @private {number}
 */
EmojiView.EMOJI_DRAG_INTERVAL_ = 300;


/**
 * The minimal required drag distance for quick emoji page swipe in px.
 *
 * @private {number}
 */
EmojiView.EMOJI_DRAG_DISTANCE_ = 60;


/** @private {!PointerHandler} */
EmojiView.prototype.pointerHandler_;


/** @override */
EmojiView.prototype.createDom = function() {
  goog.base(this, 'createDom');
  var elem = this.getElement();
  if (elem) {
    this.pointerHandler_ = new PointerHandler(elem);
  }
  this.getHandler().
      listen(this.pointerHandler_, EventType.POINTER_DOWN,
      this.onPointerDown_).
      listen(this.pointerHandler_, EventType.POINTER_UP, this.onPointerUp_).
      listen(this.pointerHandler_, EventType.DRAG, this.onDragEvent_).
      listen(this.pointerHandler_, EventType.LONG_PRESS, this.onLongPress_);
  this.emojiRows_ =
      /** @type {!ExtendedLayout} */ (this.getChildViewById('emojiRows'));
  this.pageIndicator_ =
      /** @type {!PageIndicator} */
      (this.getChildViewById('indicator-background'));
  for (var i = 0; i < this.keysPerPage_; i++) {
    this.recentEmojiKeys_.push(
        /** @type {!i18n.input.chrome.inputview.elements.content.EmojiKey} */
        (this.getChildViewById('emojikey' + i)));
  }
  for (var i = 0; i < this.totalTabbars_; i++) {
    this.tabbarKeys_.push(
        /** @type {!i18n.input.chrome.inputview.elements.content.TabBarKey} */
        (this.getChildViewById('Tabbar' + i)));
  }
};


/**
 * Handles the pointer down event.
 *
 * @param {!i18n.input.chrome.inputview.events.PointerEvent} e .
 * @private
 */
EmojiView.prototype.onPointerDown_ = function(e) {
  var view = e.view;
  if (view.type == ElementType.EMOJI_KEY) {
    this.pointerDownTimeStamp_ = e.timestamp;
    this.dragDistance_ = 0;
    return;
  }
};


/**
 * Handles the pointer up event.
 *
 * @param {!i18n.input.chrome.inputview.events.PointerEvent} e .
 * @private
 */
EmojiView.prototype.onPointerUp_ = function(e) {
  var view = e.view;
  switch (view.type) {
    case ElementType.EMOJI_KEY:
      if (this.isDragging) {
        var interval = e.timestamp - this.pointerDownTimeStamp_;
        if (interval < EmojiView.EMOJI_DRAG_INTERVAL_ &&
            Math.abs(this.dragDistance_) >= EmojiView.EMOJI_DRAG_DISTANCE_) {
          this.adjustMarginLeft_(this.dragDistance_);
        } else {
          this.adjustMarginLeft_();
        }

        this.isDragging = false;
      } else if (view.text != '') {
        this.setRecentEmoji_(view.text);
      }
      this.update();
      return;
    case ElementType.TAB_BAR_KEY:
      this.updateCategory_(view);
      this.updateTabbarBorder_();
      return;
  }
};


/**
 * Handles the drag event.
 *
 * @param {!i18n.input.chrome.inputview.events.PointerEvent} e .
 * @private
 */
EmojiView.prototype.onDragEvent_ = function(e) {
  var view = e.view;
  this.isDragging = true;
  if (view.type == ElementType.EMOJI_KEY) {
    this.setEmojiMarginLeft_(e.deltaX);
    this.dragDistance_ += e.deltaX;
  }
};


/**
 * Handles the long press event.
 *
 * @param {!i18n.input.chrome.inputview.events.PointerEvent} e .
 * @private
 */
EmojiView.prototype.onLongPress_ = function(e) {
  var view = e.view;
  view.setHighlighted(this.isDragging == false && view.text != '');
};


/** @override */
EmojiView.prototype.disposeInternal = function() {
  goog.dispose(this.pointerHandler_);

  goog.base(this, 'disposeInternal');
};


/**
 * Set the margin left of the emoji slider.
 *
 * @param {number} deltaX The margin left value.
 * @private
 */
EmojiView.prototype.setEmojiMarginLeft_ = function(deltaX) {
  this.emojiRows_.slide(deltaX);
  this.pageIndicator_.slide(-deltaX,
      this.pagesInCategory_[this.categoryID_]);
};


/**
 * Update the current emoji category.
 *
 * @param {i18n.input.chrome.inputview.elements.Element} view The view.
 * @private
 */
EmojiView.prototype.updateCategory_ = function(view) {
  this.categoryID_ = view.toCategory;
  this.emojiRows_.updateCategory(this.pageOffsets_[this.categoryID_]);
  this.pageIndicator_.gotoPage(0,
      this.pagesInCategory_[this.categoryID_]);
  this.updateTabbarBorder_();
};


/**
 * Adjust the margin left to the nearest page.
 *
 * @param {number=} opt_distance The distance to adjust to.
 * @private
 */
EmojiView.prototype.adjustMarginLeft_ = function(opt_distance) {
  var pageNum = this.emojiRows_.adjustMarginLeft(opt_distance);
  this.categoryID_ = this.pageToCategory_[pageNum];
  this.pageIndicator_.gotoPage(
      pageNum - this.pageOffsets_[this.categoryID_],
      this.pagesInCategory_[this.categoryID_]);
  this.updateTabbarBorder_();

};


/**
 * Clear all the states for the emoji.
 *
 */
EmojiView.prototype.clearEmojiStates = function() {
  this.categoryID_ = 1;
  this.emojiRows_.updateCategory(1);
  this.pageIndicator_.gotoPage(0, this.pagesInCategory_[1]);
  this.updateTabbarBorder_();
};


/**
 * Sets the recent emoji.
 *
 * @param {string} text The recent emoji text.
 * @private
 */
EmojiView.prototype.setRecentEmoji_ = function(text) {
  goog.array.insertAt(this.recentEmojiList_, text, 0);
  goog.array.removeDuplicates(this.recentEmojiList_);
  var len = this.recentEmojiList_.length;
  for (var i = 0; i < this.keysPerPage_; i++) {
    var newText = i < len ? this.recentEmojiList_[i] : '';
    this.recentEmojiKeys_[i].updateText(newText);
  }
};


/**
 * Update the tabbar's border.
 *
 * @private
 */
EmojiView.prototype.updateTabbarBorder_ = function() {
  for (var i = 0, len = this.totalTabbars_; i < len; i++) {
    this.tabbarKeys_[i].updateBorder(this.categoryID_);
  }
};


/** @override */
EmojiView.prototype.activate = function(rawKeyset) {
  this.adapter.setEmojiInputToolCode();
  goog.dom.classlist.add(this.getElement().parentElement.parentElement,
      Css.EMOJI);
  this.clearEmojiStates();
};


/** @override */
EmojiView.prototype.deactivate = function(rawKeyset) {
  this.adapter.unsetEmojiInputToolCode();
  goog.dom.classlist.remove(this.getElement().parentElement.parentElement,
      Css.EMOJI);
};
});  // goog.scope
