// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

var remoting = remoting || {};

/**
 * Attach appropriate event handlers and show or hide the feedback button based
 * on whether or not the current version of Chrome recognizes Chrome Remote
 * Desktop as an authorized feedback source.
 *
 * @param {HTMLElement} container The menu containing the help and feedback
 *     items.
 */
remoting.manageHelpAndFeedback = function(container) {
  var showHelp = function() {
    window.open('https://www.google.com/support/chrome/bin/answer.py?' +
                'answer=1649523');
  }
  var helpButton = container.querySelector('.menu-help');
  base.debug.assert(helpButton != null);
  helpButton.addEventListener('click', showHelp, false);
  var feedbackButton = container.querySelector('.menu-feedback');
  base.debug.assert(feedbackButton != null);
  var chromeVersion = parseInt(
      window.navigator.appVersion.match(/Chrome\/(\d+)\./)[1], 10);
  if (chromeVersion >= 35) {
    feedbackButton.addEventListener('click',
                                    remoting.sendFeedback_,
                                    false);
  } else {
    feedbackButton.hidden = true;
  }
};

/**
 * Pass the current version of Chrome Remote Desktop to the Google Feedback
 * extension and instruct it to show the feedback dialog.
 */
remoting.sendFeedback_ = function() {
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