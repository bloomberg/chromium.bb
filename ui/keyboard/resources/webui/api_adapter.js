// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function insertText(text) {
  chrome.send('insertText', [ text ]);
}

function sendKeyEvent(event) {
  chrome.send('sendKeyEvent', [ event ]);
}

function hideKeyboard() {
  chrome.send('hideKeyboard');
}

(function(exports) {
  /**
   * An array to save callbacks of each request.
   * @type {Array.<function(Object)>}
   */
  var requestIdCallbackMap = [];

  /**
   * An incremental integer that represents a unique requestId.
   * @type {number}
   */
  var requestId = 0;

  /**
   * Called when a text input box gets focus.
   * @param {object} inputContext Describes an input context. It only contains
   *     the type of text input box at present and only "password", "number" and
   *     "text" are supported.
   */
  function OnTextInputBoxFocused(inputContext) {
    // Do not want to use the system keyboard for passwords in webui.
    if (inputContext.type == 'password')
      inputContext.type = 'text';
    keyboard.inputTypeValue = inputContext.type;
  }

  /**
   * Gets the context of the focused input field. The context is returned as a
   * paramter in the |callback|.
   * @param {function(Object)} callback The callback function after the webui
   *     function finished.
   * @return {number} The ID of the new request.
   */
  function GetInputContext(callback) {
    var id = requestId;
    requestIdCallbackMap[id] = callback;
    chrome.send('getInputContext', [ id ]);
    requestId++;
    return id;
  }

  /**
   * Cancel the callback specified by requestId.
   * @param {number} requestId The requestId of the callback that about to
   *     cancel.
   */
  function CancelRequest(requestId) {
    requestIdCallbackMap[requestId] = undefined;
  }

  /**
   * Webui function callback. Any call to chrome.send('getInputContext', [id])
   * should trigger this function being called with the parameter
   * inputContext.requestId == id.
   * @param {Object} inputContext The context of focused input field. Note we
   *     only have type(input box type) and requestId fields now.
   */
  function GetInputContextCallback(inputContext) {
    var requestId = inputContext.requestId;
    if (!requestIdCallbackMap[requestId])
      return;
    requestIdCallbackMap[requestId](inputContext);
  }

  exports.OnTextInputBoxFocused = OnTextInputBoxFocused;
  exports.getInputContext = GetInputContext;
  exports.cancelRequest = CancelRequest;
  exports.GetInputContextCallback = GetInputContextCallback;
})(this);
