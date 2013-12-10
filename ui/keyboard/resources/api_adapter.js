// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Queries the document for an element with a matching id.
 * @param {string} id is a case-sensitive string representing the unique ID of
 *     the element being sought.
 * @return {?Element} The element with that id.
 */
var $ = function(id) {
  return document.getElementById(id);
}

function logIfError() {
  if (chrome.runtime.lastError) {
    console.log(chrome.runtime.lastError);
  }
}

function insertText(text) {
  chrome.virtualKeyboardPrivate.insertText(text, logIfError);
}

function MoveCursor(swipe_direction, swipe_flags) {
  chrome.virtualKeyboardPrivate.moveCursor(swipe_direction, swipe_flags);
}

function sendKeyEvent(event) {
  chrome.virtualKeyboardPrivate.sendKeyEvent(event, logIfError);
}

function hideKeyboard() {
  chrome.virtualKeyboardPrivate.hideKeyboard(logIfError);
}

function keyboardLoaded() {
  chrome.virtualKeyboardPrivate.keyboardLoaded(logIfError);
}

chrome.virtualKeyboardPrivate.onTextInputBoxFocused.addListener(
  function (inputContext) {
    $('keyboard').inputTypeValue = inputContext.type;
  }
);
