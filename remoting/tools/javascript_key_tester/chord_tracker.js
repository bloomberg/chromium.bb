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
 * @param {Event} event The keyup or keydown event.
 */
ChordTracker.prototype.addKeyEvent = function(event) {
  this.begin_();
  var span = document.createElement('span');
  span.title = this.makeTitle_(event);
  if (event.type == 'keydown') {
    span.classList.add('key-down');
    this.pressedKeys_[event.keyCode] = span;
  } else {
    span.classList.add('key-up');
    delete this.pressedKeys_[event.keyCode];
  }
  span.innerText = this.keyName_(event.keyCode);
  this.currentDiv_.appendChild(span);
  if (!this.keysPressed_()) {
    this.end_();
  }
};

ChordTracker.prototype.releaseAllKeys = function() {
  this.end_();
  for (var i in this.pressedKeys_) {
    this.pressedKeys_[i].classList.add('unreleased');
  }
  this.pressedKeys_ = {};
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

/**
 * @param {Event} event The keyup or keydown event.
 * @private
 */
ChordTracker.prototype.makeTitle_ = function(event) {
  return 'type: ' + event.type + '\n' +
         'alt: ' + event.altKey + '\n' +
         'shift: ' + event.shiftKey + '\n' +
         'control: ' + event.controlKey + '\n' +
         'meta: ' + event.metaKey + '\n' +
         'charCode: ' + event.charCode + '\n' +
         'keyCode: ' + event.keyCode + '\n' +
         'keyIdentifier: ' + event.keyIdentifier + '\n' +
         'repeat: ' + event.repeat + '\n';
};
