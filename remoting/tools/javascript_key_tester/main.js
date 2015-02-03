/* Copyright (c) 2014 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/**
 * @param {string} eventName
 * @param {number=} opt_space
 * @return {string}
 */
function jsonifyJavascriptKeyEvent(event, eventName, opt_space) {
  return "JavaScript '" + eventName + "' event = " + JSON.stringify(
      event,
      ['type', 'alt', 'shift', 'control', 'meta', 'charCode', 'keyCode',
          'keyIdentifier', 'repeat'],
      opt_space);
};

/**
 * @param {ChordTracker} jsChordTracker
 * @param {Event} event
 * @return {void}
 */
function handleJavascriptKeyDownEvent(jsChordTracker, event) {
  appendToTextLog(jsonifyJavascriptKeyEvent(event, 'keydown', undefined));
  jsChordTracker.addKeyDownEvent(
      event.keyCode, jsonifyJavascriptKeyEvent(event, 'keydown', 2));
}

/**
 * @param {ChordTracker} jsChordTracker
 * @param {Event} event
 * @return {void}
 */
function handleJavascriptKeyUpEvent(jsChordTracker, event) {
  appendToTextLog(jsonifyJavascriptKeyEvent(event, 'keyup', undefined));
  jsChordTracker.addKeyUpEvent(
      event.keyCode, jsonifyJavascriptKeyEvent(event, 'keyup', 2));
}

/** @constructor */
var PNaClEvent = function() {
  /** @type {string} */
  this.type = "";
  /** @type {number} */
  this.modifiers = 0;
  /** @type {number} */
  this.keyCode = 0;
  /** @type {string|undefined} */
  this.characterText = undefined;
  /** @type {string} */
  this.code = "";
};

/**
 * @param {PNaClEvent} event
 * @param {number|undefined} space
 * @return {string}
 */
function jsonifyPnaclKeyboardInputEvent(event, space) {
  return "PNaCl KeyboardInputEvent = " + JSON.stringify(
      event,
      ['type', 'modifiers', 'keyCode', 'characterText', 'code'],
      space);
};

/**
 * @param {ChordTracker} pnaclChordTracker
 * @param {Event} event
 * @return {void}
 */
function handlePNaclMessage(pnaclChordTracker, event) {
  var pnaclEvent = /** @type {PNaClEvent} */ (event.data);

  appendToTextLog(jsonifyPnaclKeyboardInputEvent(pnaclEvent, undefined));
  var title = jsonifyPnaclKeyboardInputEvent(pnaclEvent, 2);
  if (pnaclEvent.type == "KEYDOWN") {
    pnaclChordTracker.addKeyDownEvent(pnaclEvent.keyCode, title);
  }
  if (pnaclEvent.type == "KEYUP") {
    pnaclChordTracker.addKeyUpEvent(pnaclEvent.keyCode, title);
  }
  if (pnaclEvent.type == "CHAR") {
    pnaclChordTracker.addCharEvent(pnaclEvent.characterText, title);
  }
}

/**
 * @param {string} str
 * @return {void}
 */
function appendToTextLog(str) {
  var textLog = document.getElementById('text-log');
  var div = document.createElement('div');
  div.innerText = str;
  textLog.appendChild(div);
}

function onLoad() {
  // Start listening to Javascript keyup/keydown events.
  var jsLog = document.getElementById('javascript-log');
  var jsChordTracker = new ChordTracker(jsLog);
  document.body.addEventListener(
      'keydown',
      function (event) {
        handleJavascriptKeyDownEvent(jsChordTracker, event);
      },
      false);
  document.body.addEventListener(
      'keyup',
      function (event) {
        handleJavascriptKeyUpEvent(jsChordTracker, event);
      },
      false);

  // Start listening to PNaCl keyboard input events.
  var pnaclLog = document.getElementById('pnacl-log');
  var pnaclChordTracker = new ChordTracker(pnaclLog);
  document.getElementById('pnacl-listener').addEventListener(
       'message',
       function (message) {
         handlePNaclMessage(pnaclChordTracker, message);
       },
       true);

  // Start listening to generic, source-agnostic events.
  window.addEventListener(
      'blur',
      function () {
        jsChordTracker.releaseAllKeys();
        pnaclChordTracker.releaseAllKeys();
      },
      false);
  document.getElementById('clear-log').addEventListener(
      'click',
      function() {
        jsLog.innerText = '';
        pnaclLog.innerText = '';
        document.getElementById('text-log').innerText = '';
      },
      false);
}

window.addEventListener('load', onLoad, false);
