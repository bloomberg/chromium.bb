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
goog.provide('i18n.input.chrome.inputview.KeyboardContainer');

goog.require('goog.dom.TagName');
goog.require('goog.dom.classlist');
goog.require('goog.ui.Container');
goog.require('i18n.input.chrome.inputview.Css');
goog.require('i18n.input.chrome.inputview.elements.content.AltDataView');
goog.require('i18n.input.chrome.inputview.elements.content.CandidateView');
goog.require('i18n.input.chrome.inputview.elements.content.EmojiView');
goog.require('i18n.input.chrome.inputview.elements.content.ExpandedCandidateView');
goog.require('i18n.input.chrome.inputview.elements.content.HandwritingView');
goog.require('i18n.input.chrome.inputview.elements.content.KeysetView');
goog.require('i18n.input.chrome.inputview.elements.content.MenuView');



goog.scope(function() {
var Css = i18n.input.chrome.inputview.Css;
var EmojiView = i18n.input.chrome.inputview.elements.content.EmojiView;
var HandwritingView = i18n.input.chrome.inputview.elements.content.
    HandwritingView;
var KeysetView = i18n.input.chrome.inputview.elements.content.KeysetView;
var SpecNodeName = i18n.input.chrome.inputview.SpecNodeName;
var content = i18n.input.chrome.inputview.elements.content;



/**
 * The keyboard container.
 *
 * @param {i18n.input.chrome.inputview.Adapter=} opt_adapter .
 * @constructor
 * @extends {goog.ui.Container}
 */
i18n.input.chrome.inputview.KeyboardContainer = function(opt_adapter) {
  goog.base(this);

  /** @type {!content.CandidateView} */
  this.candidateView = new content.CandidateView('candidateView', this);

  /** @type {!content.AltDataView} */
  this.altDataView = new content.AltDataView(this);

  /** @type {!content.MenuView} */
  this.menuView = new content.MenuView(this);

  /** @type {!content.ExpandedCandidateView} */
  this.expandedCandidateView = new content.ExpandedCandidateView(this);

  /**
   * The map of the KeysetViews.
   * Key: keyboard code.
   * Value: The view object.
   *
   * @type {!Object.<string, !KeysetView>}
   */
  this.keysetViewMap = {};

  /**
   * The bus channel to communicate with background.
   *
   * @private {i18n.input.chrome.inputview.Adapter}
   */
  this.adapter_ = opt_adapter || null;
};
goog.inherits(i18n.input.chrome.inputview.KeyboardContainer,
    goog.ui.Container);
var KeyboardContainer = i18n.input.chrome.inputview.KeyboardContainer;


/** @type {!KeysetView} */
KeyboardContainer.prototype.currentKeysetView;


/**
 * The padding bottom of the whole keyboard.
 *
 * @private {number}
 */
KeyboardContainer.PADDING_BOTTOM_ = 7;


/**
 * The padding value of handwriting panel.
 *
 * @type {number}
 * @private
 */
KeyboardContainer.HANDWRITING_PADDING_ = 22;


/**
 * The padding of emoji keyset.
 *
 * @type {number}
 * @private
 */
KeyboardContainer.EMOJI_PADDING_ = 22;


/**
 * An div to wrapper candidate view and keyboard set view.
 *
 * @private {Element}
 */
KeyboardContainer.prototype.wrapperDiv_ = null;


/** @override */
KeyboardContainer.prototype.createDom = function() {
  goog.base(this, 'createDom');

  var elem = this.getElement();
  this.wrapperDiv_ = this.getDomHelper().createDom(
      goog.dom.TagName.DIV, Css.WRAPPER);
  this.candidateView.render(this.wrapperDiv_);
  this.getDomHelper().appendChild(elem, this.wrapperDiv_);
  this.altDataView.render();
  this.menuView.render();
  this.expandedCandidateView.render(this.wrapperDiv_);
  this.expandedCandidateView.setVisible(false);
  goog.dom.classlist.add(elem, Css.CONTAINER);
};


/** @override */
KeyboardContainer.prototype.enterDocument = function() {
  goog.base(this, 'enterDocument');

  this.setFocusable(false);
  this.setFocusableChildrenAllowed(false);
};


/**
 * Updates the whole keyboard.
 */
KeyboardContainer.prototype.update = function() {
  this.currentKeysetView && this.currentKeysetView.update();
};


/**
 * Adds a keyset view.
 *
 * @param {!Object} keysetData .
 * @param {!Object} layoutData .
 * @param {string} keyset .
 * @param {string} languageCode .
 * @param {!i18n.input.chrome.inputview.Model} model .
 * @param {string} inputToolName .
 * @param {!Object.<string, boolean>} conditions .
 */
KeyboardContainer.prototype.addKeysetView = function(keysetData, layoutData,
    keyset, languageCode, model, inputToolName, conditions) {
  var view;
  if (keyset == 'emoji') {
    view = new EmojiView(keysetData, layoutData, keyset, languageCode, model,
        inputToolName, this, this.adapter_);
  } else if (keyset == 'hwt') {
    view = new HandwritingView(keysetData, layoutData, keyset, languageCode,
        model, inputToolName, this, this.adapter_);
  } else {
    view = new KeysetView(keysetData, layoutData, keyset, languageCode, model,
        inputToolName, this, this.adapter_);
  }
  view.render(this.wrapperDiv_);
  view.applyConditions(conditions);
  view.setVisible(false);
  this.keysetViewMap[keyset] = view;
};


/**
 * Switches to a keyset.
 *
 * @param {string} keyset .
 * @param {string} title .
 * @param {boolean} isPasswordBox .
 * @param {boolean} isA11yMode .
 * @param {string} rawKeyset The raw keyset id will switch to.
 * @param {string} lastRawkeyset .
 * @param {string} languageCode .
 * @return {boolean} True if switched successfully.
 */
KeyboardContainer.prototype.switchToKeyset = function(keyset, title,
    isPasswordBox, isA11yMode, rawKeyset, lastRawkeyset, languageCode) {
  if (!this.keysetViewMap[keyset]) {
    return false;
  }

  for (var name in this.keysetViewMap) {
    var view = this.keysetViewMap[name];
    if (name == keyset) {
      this.candidateView.setVisible(!view.disableCandidateView);
      view.setVisible(true);
      view.update();
      if (view.spaceKey) {
        view.spaceKey.updateTitle(title, !isPasswordBox);
      }
      if (isA11yMode) {
        goog.dom.classlist.add(this.getElement(), Css.A11Y);
      }
      // If current raw keyset is changed, record it.
      if (lastRawkeyset != rawKeyset) {
        view.fromKeyset = lastRawkeyset;
      }
      if (view instanceof HandwritingView) {
        view.setLanguagecode(languageCode);
      }
      this.currentKeysetView = view;
      this.candidateView.updateByKeyset(rawKeyset, isPasswordBox,
          goog.i18n.bidi.isRtlLanguage(languageCode));
    } else {
      view.setVisible(false);
    }
  }
  return true;
};


/**
 * Resizes the whole keyboard.
 *
 * @param {number} width .
 * @param {number} height .
 * @param {number} widthPercent .
 * @param {number} candidateViewHeight .
 */
KeyboardContainer.prototype.resize = function(width, height, widthPercent,
    candidateViewHeight) {
  if (!this.currentKeysetView) {
    return;
  }
  var elem = this.getElement();

  var h;
  if (this.currentKeysetView.isHandwriting()) {
    h = height - KeyboardContainer.HANDWRITING_PADDING_;
    elem.style.paddingBottom = '';
  } else {
    h = height - KeyboardContainer.PADDING_BOTTOM_;
    elem.style.paddingBottom = KeyboardContainer.PADDING_BOTTOM_ + 'px';
  }

  var padding = Math.round((width - width * widthPercent) / 2);
  elem.style.paddingLeft = elem.style.paddingRight = padding + 'px';

  var w = width - 2 * padding;
  h = this.currentKeysetView.disableCandidateView ?
      h - KeyboardContainer.EMOJI_PADDING_ : h - candidateViewHeight;

  this.candidateView.setWidthInWeight(
      this.currentKeysetView.getWidthInWeight());
  this.candidateView.resize(w, candidateViewHeight);
  this.currentKeysetView.resize(w, h);
  this.expandedCandidateView.resize(w, h);
  this.altDataView.resize(screen.width, height);
  this.menuView.resize(screen.width, height);
};


/** @override */
KeyboardContainer.prototype.disposeInternal = function() {
  goog.dispose(this.candidateView);
  goog.dispose(this.altDataView);
  goog.dispose(this.menuView);
  for (var key in this.keysetViewMap) {
    goog.dispose(this.keysetViewMap[key]);
  }

  goog.base(this, 'disposeInternal');
};


/**
 * Whether there are strokes on canvas.
 *
 * @return {boolean} Whether there are strokes on canvas.
 */
KeyboardContainer.prototype.hasStrokesOnCanvas = function() {
  if (this.currentKeysetView) {
    return this.currentKeysetView.hasStrokesOnCanvas();
  } else {
    return false;
  }
};


/**
 * Cleans the stokes.
 */
KeyboardContainer.prototype.cleanStroke = function() {
  if (this.currentKeysetView) {
    this.currentKeysetView.cleanStroke();
  }
};
});  // goog.scope
