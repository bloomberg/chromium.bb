/* Copyright 2015 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/** @constructor */
var PNaClEvent = function() {
  /** @type {string} */
  this.type = "";
  /** @type {number} */
  this.modifiers = 0;
  /** @type {number} */
  this.keyCode = 0;
  /** @type {string} */
  this.characterText = '';
  /** @type {string} */
  this.code = '';
};


/**
 * @param {HTMLElement} jsLog
 * @param {HTMLElement} pnaclLog
 * @param {HTMLElement} textLog
 * @param {HTMLElement} pnaclPlugin
 * @param {HTMLElement} pnaclListener
 * @constructor
 */
var EventListeners = function(jsLog, pnaclLog, textLog,
                              pnaclPlugin, pnaclListener) {
  this.pnaclPlugin_ = pnaclPlugin;
  this.pnaclListener_ = pnaclListener;
  this.textLog_ = textLog;

  this.onKeyDownHandler_ = this.onKeyDown_.bind(this);
  this.onKeyUpHandler_ = this.onKeyUp_.bind(this);
  this.onMessageHandler_ = this.onMessage_.bind(this);
  this.onBlurHandler_ = this.onBlur_.bind(this);

  this.jsChordTracker_ = new ChordTracker(jsLog);
  this.pnaclChordTracker_ = new ChordTracker(pnaclLog);

  this.startTime_ = new Date();
};

/**
 * Start listening for keyboard events.
 */
EventListeners.prototype.activate = function() {
  document.body.addEventListener('keydown', this.onKeyDownHandler_, false);
  document.body.addEventListener('keyup', this.onKeyUpHandler_, false);
  this.pnaclListener_.addEventListener('message', this.onMessageHandler_, true);
  this.pnaclPlugin_.addEventListener('blur', this.onBlurHandler_, false);
  this.onBlur_();
};

/**
 * Stop listening for keyboard events.
 */
EventListeners.prototype.deactivate = function() {
  document.body.removeEventListener('keydown', this.onKeyDownHandler_, false);
  document.body.removeEventListener('keyup', this.onKeyUpHandler_, false);
  this.pnaclListener_.removeEventListener(
      'message', this.onMessageHandler_, true);
  this.pnaclPlugin_.removeEventListener('blur', this.onBlurHandler_, false);
};

/**
 * @param {Event} event
 * @private
 */
EventListeners.prototype.onKeyDown_ = function(event) {
  this.appendToTextLog_(this.jsonifyJavascriptKeyEvent_(event, 'keydown'));
  this.jsChordTracker_.addKeyDownEvent(
      event.keyCode, this.jsonifyJavascriptKeyEvent_(event, 'keydown', 2));
};

/**
 * @param {Event} event
 * @return {void}
 */
EventListeners.prototype.onKeyUp_ = function(event) {
  this.appendToTextLog_(this.jsonifyJavascriptKeyEvent_(event, 'keyup'));
  this.jsChordTracker_.addKeyUpEvent(
      event.keyCode, this.jsonifyJavascriptKeyEvent_(event, 'keyup', 2));
}

/**
 * @param {Event} event
 * @return {void}
 */
EventListeners.prototype.onMessage_ = function (event) {
  var pnaclEvent = /** @type {PNaClEvent} */ (event['data']);

  this.appendToTextLog_(this.jsonifyPnaclKeyboardInputEvent_(pnaclEvent));
  var title = this.jsonifyPnaclKeyboardInputEvent_(pnaclEvent, 2);
  if (pnaclEvent.type == "KEYDOWN") {
    this.pnaclChordTracker_.addKeyDownEvent(pnaclEvent.code, title);
  }
  if (pnaclEvent.type == "KEYUP") {
    this.pnaclChordTracker_.addKeyUpEvent(pnaclEvent.code, title);
  }
  if (pnaclEvent.type == "CHAR") {
    this.pnaclChordTracker_.addCharEvent(pnaclEvent.characterText, title);
  }
}

/**
 * @return {void}
 */
EventListeners.prototype.onBlur_ = function() {
  this.pnaclPlugin_.focus();
};

/**
 * @param {string} str
 * @private
 */
EventListeners.prototype.appendToTextLog_ = function(str) {
  var div = document.createElement('div');
  div.innerText = (new Date() - this.startTime_) + ':' + str;
  this.textLog_.appendChild(div);
};

/**
 * @param {Event} event
 * @param {string} eventName
 * @param {number=} opt_space
 * @return {string}
 * @private
 */
EventListeners.prototype.jsonifyJavascriptKeyEvent_ =
    function(event, eventName, opt_space) {
  return "JavaScript '" + eventName + "' event = " + JSON.stringify(
      event,
      ['type', 'alt', 'shift', 'control', 'meta', 'charCode', 'keyCode',
       'keyIdentifier', 'repeat'],
      opt_space);
};

/**
 * @param {PNaClEvent} event
 * @param {number=} opt_space
 * @return {string}
 * @private
 */
EventListeners.prototype.jsonifyPnaclKeyboardInputEvent_ =
    function(event, opt_space) {
  return "PNaCl KeyboardInputEvent = " + JSON.stringify(
      event,
      ['type', 'modifiers', 'keyCode', 'characterText', 'code'],
      opt_space);
};
