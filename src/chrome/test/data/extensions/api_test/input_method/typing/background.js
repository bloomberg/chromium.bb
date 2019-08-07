// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class TestEnv {
  constructor() {
    this.inputContext = null;

    chrome.input.ime.onFocus.addListener((context) => {
      this.inputContext = context;
    });

    chrome.input.ime.onBlur.addListener(() => {
      this.inputContext = null;
    });
  }

  getContextID() {
    return this.inputContext.contextID;
  }

  onSurroundingTextChanged() {
    return new Promise((resolve) => {
      chrome.input.ime.onSurroundingTextChanged.addListener(
          function listener(_, surroundingInfo) {
            chrome.input.ime.onSurroundingTextChanged.removeListener(listener);
            resolve(surroundingInfo.text);
          });
    });
  }

  onCompositionBoundsChanged() {
    return new Promise((resolve) => {
      chrome.inputMethodPrivate.onCompositionBoundsChanged.addListener(
          function listener(_, boundsList) {
            chrome.inputMethodPrivate.onCompositionBoundsChanged.removeListener(
                listener);
            resolve(boundsList);
          });
    });
  }
};

const testEnv = new TestEnv();

// Wrap inputMethodPrivate in a promise-based API to simplify test code.
function wrapAsync(apiFunction) {
  return (...args) => {
    return new Promise((resolve, reject) => {
      apiFunction(...args, (...result) => {
        if (!!chrome.runtime.lastError) {
          console.log(chrome.runtime.lastError.message);
          reject(Error(chrome.runtime.lastError));
        } else {
          resolve(...result);
        }
      });
    });
  }
}

const asyncInputIme = {
  commitText: wrapAsync(chrome.input.ime.commitText),
  setComposition: wrapAsync(chrome.input.ime.setComposition),
}

const asyncInputMethodPrivate = {
  setCurrentInputMethod:
      wrapAsync(chrome.inputMethodPrivate.setCurrentInputMethod),
  setCompositionRange:
      wrapAsync(chrome.inputMethodPrivate.setCompositionRange)
};

chrome.test.runTests([
  async function setUp() {
    await asyncInputMethodPrivate.setCurrentInputMethod(
        '_ext_ime_ilanclmaeigfpnmdlgelmhkpkegdioiptest');

    chrome.test.succeed();
  },

  async function setCompositionRangeTest() {
    await asyncInputIme.commitText({
      contextID: testEnv.getContextID(),
      text: 'hello world'
    });

    chrome.test.assertEq('hello world',
        await testEnv.onSurroundingTextChanged());

    // Cursor is at the end of the string.
    await asyncInputMethodPrivate.setCompositionRange({
      contextID: testEnv.getContextID(),
      selectionBefore: 5,
      selectionAfter: 0,
      segments: [
        { start: 0, end: 2, style: "underline" },
        { start: 2, end: 5, style: "underline" }
      ]
    });

    // Should underline "world".
    chrome.test.assertEq(5,
      (await testEnv.onCompositionBoundsChanged()).length);

    await asyncInputIme.setComposition({
      contextID: testEnv.getContextID(),
      text: "foo",
      cursor: 0
    });

    // Composition should change to "foo".
    chrome.test.assertEq(3,
      (await testEnv.onCompositionBoundsChanged()).length);

    // Should replace composition with "again".
    await asyncInputIme.commitText({
      contextID: testEnv.getContextID(),
      text: 'again'
    });

    chrome.test.assertEq('hello again',
        await testEnv.onSurroundingTextChanged());

    // Cursor is at end of the string.
    // Call setCompositionRange with no segments.
    await asyncInputMethodPrivate.setCompositionRange({
      contextID: testEnv.getContextID(),
      selectionBefore: 5,
      selectionAfter: 0
    });

    // Composition should be "again".
    chrome.test.assertEq(5,
      (await testEnv.onCompositionBoundsChanged()).length);

    chrome.test.succeed();
  }
]);
