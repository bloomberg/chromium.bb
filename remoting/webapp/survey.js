// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Functions and event handlers to invite the user to participate in a survey
 * to help improve the product.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

var kStorageKey = 'feedback-survey-dismissed';
var kSurveyDivId = 'survey-opt-in';
var kSurveyAcceptId = 'survey-accept';
var kSurveyDeclineId = 'survey-decline';

/**
 * Hide the survey request and record some basic information about the current
 * state of the world in synced storage. This may be useful in the future if we
 * want to show the request again. At the moment, the data itself is ignored;
 * only its presence or absence is important.
 *
 * @param {boolean} optIn True if the user clicked the "Take the survey" link;
 *     false if they clicked the close icon.
 */
remoting.dismissSurvey = function(optIn) {
  var value = {};
  value[kStorageKey] = {
    optIn: optIn,
    date: new Date(),
    version: chrome.runtime.getManifest().version
  };
  chrome.storage.sync.set(value);
  document.getElementById(kSurveyDivId).hidden = true;
};

/**
 * Show or hide the survey request, depending on whether or not the user has
 * already seen it.
 */
remoting.initSurvey = function() {
  /** @param {Object} value */
  var onFeedbackSurveyInfo = function(value) {
    /** @type {*} */
    var dismissed = value[kStorageKey];
    document.getElementById(kSurveyDivId).hidden = !!dismissed;
  };
  chrome.storage.sync.get(kStorageKey, onFeedbackSurveyInfo);
  var accept = document.getElementById(kSurveyAcceptId);
  var decline = document.getElementById(kSurveyDeclineId);
  accept.addEventListener(
      'click', remoting.dismissSurvey.bind(null, true), false);
  decline.addEventListener(
      'click', remoting.dismissSurvey.bind(null, false), false);
};
