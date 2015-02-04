/* Copyright (c) 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @constructor
 * @param {HTMLElement} parentDiv
 */
var ChordTracker = function(parentDiv) {
  /** @type {HTMLElement} */
  this.parentDiv_ = parentDiv;
  /** @type {HTMLElement} */
  this.currentDiv_ = null;
  /** @type {Object.<HTMLElement>} */
  this.pressedKeys_ = {};
};

/**
 * @param {number|string} keyId Either a Javascript key-code, or a PNaCl "code"
 *     string.
 * @param {string} title
 * @return {void}
 */
ChordTracker.prototype.addKeyUpEvent = function(keyId, title) {
  var text = this.keyName_(keyId);
  var span = this.addSpanElement_('key-up', text, title);
  delete this.pressedKeys_[keyId];
  if (!this.keysPressed_()) {
    this.end_();
  }
};

/**
 * @param {number|string} keyId Either a Javascript key-code, or a PNaCl "code"
 *     string.
 * @param {string} title
 * @return {void}
 */
ChordTracker.prototype.addKeyDownEvent = function(keyId, title) {
  var text = this.keyName_(keyId);
  var span = this.addSpanElement_('key-down', text, title);
  this.pressedKeys_[keyId] = span;
};

/**
 * @param {string} characterText
 * @param {string} title
 * @return {void}
 */
ChordTracker.prototype.addCharEvent = function(characterText, title) {
  this.addSpanElement_('char-event', characterText, title);
};

/**
 * @return {void}
 */
ChordTracker.prototype.releaseAllKeys = function() {
  this.end_();
  for (var i in this.pressedKeys_) {
    this.pressedKeys_[i].classList.add('unreleased');
  }
  this.pressedKeys_ = {};
}

/**
 * @private
 * @param {string} className
 * @param {string} text
 * @param {string} title
 * @return {HTMLElement}
 */
ChordTracker.prototype.addSpanElement_ = function(className, text, title) {
  this.begin_();
  var span = /** @type {HTMLElement} */ (document.createElement('span'));
  span.classList.add(className);
  span.classList.add('key-div');
  span.innerText = text;
  span.title = title;
  this.currentDiv_.appendChild(span);
  return span;
}

/**
 * @private
 */
ChordTracker.prototype.begin_ = function() {
  if (this.currentDiv_) {
    return;
  }
  this.currentDiv_ = /** @type {HTMLElement} */ (document.createElement('div'));
  this.currentDiv_.classList.add('chord-div');
  this.parentDiv_.appendChild(this.currentDiv_);
};

/**
 * @private
 */
ChordTracker.prototype.end_ = function() {
  if (!this.currentDiv_) {
    return;
  }
  if (this.keysPressed_()) {
    this.currentDiv_.classList.add('some-keys-still-pressed');
  } else {
    this.currentDiv_.classList.add('all-keys-released');
  }
  this.currentDiv_ = null;
};

/**
 * @return {boolean} True if there are any keys pressed down.
 * @private
 */
ChordTracker.prototype.keysPressed_ = function() {
  for (var property in this.pressedKeys_) {
    return true;
  }
  return false;
};

/**
 * @param {number|string} keyId Either a Javascript key-code, or a PNaCl "code"
 *     string.
 * @return {string} A human-readable representation of the key.
 * @private
 */
ChordTracker.prototype.keyName_ = function(keyId) {
  if (typeof keyId == 'string') {
    return keyId;
  }
  var result = keyboardMap[keyId];
  if (!result) {
    result = '<' + keyId + '>';
  }
  return result;
};
