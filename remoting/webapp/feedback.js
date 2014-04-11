// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

var remoting = remoting || {};

/**
 * Show or hide the feedback button based on whether or not the current version
 * of Chrome recognizes Chrome Remote Desktop as an authorized feedback source.
 *
 * @param {HTMLElement} feedbackButton
 */
remoting.initFeedback = function(feedbackButton) {
  var chromeVersion = parseInt(
      window.navigator.appVersion.match(/Chrome\/(\d+)\./)[1], 10);
  if (chromeVersion >= 35) {
    feedbackButton.hidden = false;
    feedbackButton.addEventListener('click', remoting.sendFeedback, false);
  } else {
    feedbackButton.hidden = true;
  }
};

/**
 * Pass the current version of Chrome Remote Desktop to the Google Feedback
 * extension and instruct it to show the feedback dialog.
 */
remoting.sendFeedback = function() {
  var message = {
    requestFeedback: true,
    feedbackInfo: {
      description: '',
      systemInformation: [
        { key: 'version', value: remoting.getExtensionInfo() }
      ]
    }
  };
  var kFeedbackExtensionId = 'gfdkimpbcpahaombhbimeihdjnejgicl';
  chrome.runtime.sendMessage(kFeedbackExtensionId, message, function() {});
};