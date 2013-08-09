// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function logIfError() {
  if (chrome.runtime.lastError) {
    console.log(chrome.runtime.lastError);
  }
}

function insertText(text) {
  chrome.experimental.input.virtualKeyboard.insertText(text, logIfError);
}

function MoveCursor(swipe_direction, swipe_flags) {
  chrome.experimental.input.virtualKeyboard.moveCursor(swipe_direction,
                                                       swipe_flags);
}
