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
goog.provide('i18n.input.chrome.inputview.elements.content.CandidateView');

goog.require('goog.dom.TagName');
goog.require('goog.dom.classlist');
goog.require('goog.object');
goog.require('goog.style');
goog.require('i18n.input.chrome.inputview.Css');
goog.require('i18n.input.chrome.inputview.elements.Element');
goog.require('i18n.input.chrome.inputview.elements.ElementType');
goog.require('i18n.input.chrome.inputview.elements.content.Candidate');
goog.require('i18n.input.chrome.inputview.elements.content.CandidateButton');
goog.require('i18n.input.chrome.message.Name');



goog.scope(function() {
var Css = i18n.input.chrome.inputview.Css;
var TagName = goog.dom.TagName;
var Candidate = i18n.input.chrome.inputview.elements.content.Candidate;
var Type = i18n.input.chrome.inputview.elements.content.Candidate.Type;
var ElementType = i18n.input.chrome.inputview.elements.ElementType;
var Element = i18n.input.chrome.inputview.elements.Element;
var content = i18n.input.chrome.inputview.elements.content;
var Name = i18n.input.chrome.message.Name;



/**
 * The candidate view.
 *
 * @param {string} id The id.
 * @param {goog.events.EventTarget=} opt_eventTarget The event target.
 * @constructor
 * @extends {i18n.input.chrome.inputview.elements.Element}
 */
i18n.input.chrome.inputview.elements.content.CandidateView = function(id,
    opt_eventTarget) {
  goog.base(this, id, ElementType.CANDIDATE_VIEW, opt_eventTarget);

  /**
   * The icons.
   *
   * @private {!Array.<!i18n.input.chrome.inputview.elements.Element>}
   */
  this.iconButtons_ = [];

  this.iconButtons_[CandidateView.IconType.BACK] = new content.CandidateButton(
      '', ElementType.BACK_BUTTON, '',
      chrome.i18n.getMessage('HANDWRITING_BACK'), this);
  this.iconButtons_[CandidateView.IconType.SHRINK_CANDIDATES] = new content.
      CandidateButton('', ElementType.SHRINK_CANDIDATES,
          Css.SHRINK_CANDIDATES_ICON, '', this);
  this.iconButtons_[CandidateView.IconType.EXPAND_CANDIDATES] = new content.
      CandidateButton('', ElementType.EXPAND_CANDIDATES,
          Css.EXPAND_CANDIDATES_ICON, '', this);
};
goog.inherits(i18n.input.chrome.inputview.elements.content.CandidateView,
    i18n.input.chrome.inputview.elements.Element);
var CandidateView = i18n.input.chrome.inputview.elements.content.CandidateView;


/**
 * The icon type at the right of the candiate view.
 *
 * @enum {number}
 */
CandidateView.IconType = {
  BACK: 0,
  SHRINK_CANDIDATES: 1,
  EXPAND_CANDIDATES: 2
};


/**
 * How many candidates in this view.
 *
 * @type {number}
 */
CandidateView.prototype.candidateCount = 0;


/**
 * The padding between candidates.
 *
 * @type {number}
 * @private
 */
CandidateView.PADDING_ = 50;


/**
 * The width in weight which stands for the entire row. It is used for the
 * alignment of the number row.
 *
 * @private {number}
 */
CandidateView.prototype.widthInWeight_ = 0;


/**
 * True if it is showing candidate.
 *
 * @type {boolean}
 */
CandidateView.prototype.showingCandidates = false;


/**
 * true if it is showing number row.
 *
 * @type {boolean}
 */
CandidateView.prototype.showingNumberRow = false;


/**
 * The width for a candidate when showing in THREE_CANDIDATE mode.
 *
 * @type {number}
 * @private
 */
CandidateView.WIDTH_FOR_THREE_CANDIDATES_ = 235;


/**
 * The width of the icon at the right of the candidate view, it would be back
 * icon, hide candidates icon, or show candidates icon.
 *
 * @private {number}
 */
CandidateView.ICON_WIDTH_ = 120;


/**
 * The handwriting keyset code.
 *
 * @private {string}
 */
CandidateView.HANDWRITING_VIEW_CODE_ = 'hwt';


/**
 * The width of the inter container.
 *
 * @private {number}
 */
CandidateView.prototype.interContainerWidth_ = 0;


/** @override */
CandidateView.prototype.createDom = function() {
  goog.base(this, 'createDom');

  var dom = this.getDomHelper();
  var elem = this.getElement();
  this.interContainer_ = dom.createDom(TagName.DIV,
      Css.CANDIDATE_INTER_CONTAINER);
  dom.appendChild(elem, this.interContainer_);

  for (var i = 0; i < this.iconButtons_.length; i++) {
    var button = this.iconButtons_[i];
    button.render(elem);
    button.setVisible(false);
  }

  // CandidateView is a container which doesn't handle any event and could be
  // taken as a empty area, so don't attach the view.
  elem['view'] = null;
};


/**
 * Hides the number row.
 */
CandidateView.prototype.hideNumberRow = function() {
  if (this.showingNumberRow) {
    this.getDomHelper().removeChildren(this.interContainer_);
    this.showingNumberRow = false;
  }
};


/**
 * Shows the number row.
 */
CandidateView.prototype.showNumberRow = function() {
  goog.dom.classlist.remove(this.getElement(),
      i18n.input.chrome.inputview.Css.THREE_CANDIDATES);
  var dom = this.getDomHelper();
  var numberWidth = this.width / this.widthInWeight_ - 1;
  dom.removeChildren(this.interContainer_);
  for (var i = 0; i < 10; i++) {
    var candidateElem = new Candidate(String(i), goog.object.create(
        Name.CANDIDATE, String((i + 1) % 10)),
        Type.NUMBER, this.height, false, numberWidth, this);
    candidateElem.render(this.interContainer_);
  }
  this.showingNumberRow = true;
};


/**
 * Shows the candidates.
 *
 * @param {!Array.<!Object>} candidates The candidate list.
 * @param {boolean} showThreeCandidates .
 * @param {boolean=} opt_expandable True if the candidates would be shown
 *     in expanded view.
 */
CandidateView.prototype.showCandidates = function(candidates,
    showThreeCandidates, opt_expandable) {
  this.clearCandidates();
  if (candidates.length > 0) {
    if (showThreeCandidates) {
      this.addThreeCandidates_(candidates);
    } else {
      this.addFullCandidates_(candidates);
      if (opt_expandable) {
        this.switchToIcon(CandidateView.IconType.EXPAND_CANDIDATES,
            this.candidateCount < candidates.length);
      }
    }
    this.showingCandidates = true;
  }
};


/**
 * Adds the candidates in THREE-CANDIDATE mode.
 *
 * @param {!Array.<!Object>} candidates The candidate list.
 * @private
 */
CandidateView.prototype.addThreeCandidates_ = function(candidates) {
  goog.dom.classlist.add(this.getElement(),
      i18n.input.chrome.inputview.Css.THREE_CANDIDATES);
  var num = Math.min(3, candidates.length);
  var dom = this.getDomHelper();
  for (var i = 0; i < num; i++) {
    var candidateElem = new Candidate(String(i), candidates[i], Type.CANDIDATE,
        this.height, i == 1 || num == 1, CandidateView.
        WIDTH_FOR_THREE_CANDIDATES_, this);
    candidateElem.render(this.interContainer_);
  }
  this.candidateCount = num;
};


/**
 * Clears the candidates.
 */
CandidateView.prototype.clearCandidates = function() {
  if (this.showingCandidates) {
    this.candidateCount = 0;
    this.getDomHelper().removeChildren(this.interContainer_);
    this.showingCandidates = false;
  }
};


/**
 * Adds candidates into the view, as many as the candidate bar can support.
 *
 * @param {!Array.<!Object>} candidates The candidate list.
 * @private
 */
CandidateView.prototype.addFullCandidates_ = function(candidates) {
  goog.dom.classlist.remove(this.getElement(),
      i18n.input.chrome.inputview.Css.THREE_CANDIDATES);
  var totalWidth = Math.floor(this.width - CandidateView.ICON_WIDTH_);
  var w = 0;
  var dom = this.getDomHelper();
  var i;
  for (i = 0; i < candidates.length; i++) {
    var candidateElem = new Candidate(String(i), candidates[i], Type.CANDIDATE,
        this.height, false, undefined, this);
    candidateElem.render(this.interContainer_);
    var size = goog.style.getSize(candidateElem.getElement());
    var candidateWidth = size.width + CandidateView.PADDING_ * 2;
    // 1px is the width of the separator.
    w += candidateWidth + 1;

    if (w >= totalWidth) {
      this.interContainer_.removeChild(candidateElem.getElement());
      break;
    }
    goog.style.setWidth(candidateElem.getElement(), candidateWidth);
  }
  this.candidateCount = i;
};


/**
 * Sets the widthInWeight which equals to a total line in the
 * keyset view and it is used for alignment of number row.
 *
 * @param {number} widthInWeight .
 */
CandidateView.prototype.setWidthInWeight = function(widthInWeight) {
  this.widthInWeight_ = widthInWeight;
};


/** @override */
CandidateView.prototype.resize = function(width, height) {
  goog.style.setSize(this.getElement(), width, height);
  this.interContainer_.style.height = height + 'px';
  for (var i = 0; i < this.iconButtons_.length; i++) {
    var button = this.iconButtons_[i];
    button.resize(CandidateView.ICON_WIDTH_, height);
  }

  goog.base(this, 'resize', width, height);

  if (this.showingNumberRow) {
    this.showNumberRow();
  }
};


/**
 * Switches to the icon, or hide it.
 *
 * @param {number} type .
 * @param {boolean} visible The visibility of back button.
 */
CandidateView.prototype.switchToIcon = function(type, visible) {
  for (var i = 0; i < this.iconButtons_.length; i++) {
    this.iconButtons_[i].setVisible(false);
  }

  this.iconButtons_[type].setVisible(visible);
};


/**
 * Updates the candidate view by key set changing.
 *
 * @param {string} keyset .
 * @param {boolean} isPasswordBox .
 * @param {boolean} isRTL .
 */
CandidateView.prototype.updateByKeyset = function(
    keyset, isPasswordBox, isRTL) {
  this.switchToIcon(CandidateView.IconType.BACK,
      keyset == CandidateView.HANDWRITING_VIEW_CODE_);
  if (isPasswordBox && keyset.indexOf('compact') != -1) {
    this.showNumberRow();
  } else {
    this.hideNumberRow();
  }
  this.interContainer_.style.direction = isRTL ? 'rtl' : 'ltr';
};
});  // goog.scope
