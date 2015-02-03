/* Copyright (c) 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @constructor
 * @param {HTMLElement} parentDiv
 */
var ChordTracker = function(parentDiv) {
  this.parentDiv_ = parentDiv;
  this.currentDiv_ = null;
  this.pressedKeys_ = {};
};

/**
 * @param {number} keyCode
 * @param {string} title
 * @return {void}
 */
ChordTracker.prototype.addKeyUpEvent = function(keyCode, title) {
  var text = this.keyName_(keyCode);
  var span = this.addSpanElement_('key-up', text, title);
  delete this.pressedKeys_[keyCode];
  if (!this.keysPressed_()) {
    this.end_();
  }
};

/**
 * @param {number} keyCode
 * @param {string} title
 * @return {void}
 */
ChordTracker.prototype.addKeyDownEvent = function(keyCode, title) {
  var text = this.keyName_(keyCode);
  var span = this.addSpanElement_('key-down', text, title);
  this.pressedKeys_[keyCode] = span;
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
 */
ChordTracker.prototype.addSpanElement_ = function(className, text, title) {
  this.begin_();
  var span = document.createElement('span');
  span.classList.add(className);
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
  this.currentDiv_ = document.createElement('div');
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
 * @param {number} keyCode The keyCode field of the keyup or keydown event.
 * @return {string} A human-readable representation of the key.
 * @private
 */
ChordTracker.prototype.keyName_ = function(keyCode) {
  var result = keyboardMap[keyCode];
  if (!result) {
    result = '<' + keyCode + '>';
  }
  return result;
};

