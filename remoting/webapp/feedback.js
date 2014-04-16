// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

var remoting = remoting || {};

/**
 * Show or hide the feedback button based on whether or not the current version
 * of Chrome recognizes Chrome Remote Desktop as an authorized feedback source.
 *
 * @param {HTMLElement} helpIcon The parent <span> for the help icon and the
 *     <ul> containing the help and feedback entries.
 * @param {HTMLElement} helpButton The Help <li> associated with the help icon.
 * @param {HTMLElement} feedbackButton The Feedback <li> associated with the
 *     help icon.
 * @constructor
 */
remoting.Feedback = function(helpIcon, helpButton, feedbackButton) {
  var menuButton = new remoting.MenuButton(helpIcon);
  var showHelp = function() {
    window.open('https://www.google.com/support/chrome/bin/answer.py?' +
                'answer=1649523');
  }
  helpButton.addEventListener('click', showHelp, false);
  var chromeVersion = parseInt(
      window.navigator.appVersion.match(/Chrome\/(\d+)\./)[1], 10);
  if (chromeVersion >= 35) {
    feedbackButton.addEventListener('click',
                                    this.sendFeedback_.bind(this),
                                    false);
  } else {
    feedbackButton.hidden = true;
  }
};

/**
 * Pass the current version of Chrome Remote Desktop to the Google Feedback
 * extension and instruct it to show the feedback dialog.
 */
remoting.Feedback.prototype.sendFeedback_ = function() {
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