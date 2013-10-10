// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

chrome.virtualKeyboardPrivate.onTextInputBoxFocused.addListener(
  function (inputContext) {
    keyboard.inputTypeValue = inputContext.type;
  }
);
