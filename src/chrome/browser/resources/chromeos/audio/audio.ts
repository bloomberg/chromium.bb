// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$} from 'chrome://resources/js/util.m.js';

import {DevicePage} from './device_page.js';
import {FeedbackPage} from './feedback_page.js';
import {InputPage} from './input_page.js';
import {OutputPage} from './output_page.js';
import {PageNavigator} from './page.js';

function initialize() {
  // Initialize all pages and add to pageNavigator.
  const pageNavigator = PageNavigator.getInstance();
  const devicePage = DevicePage.getInstance();
  const outputPage = OutputPage.getInstance();
  const inputPage = InputPage.getInstance();
  const feedbackPage = FeedbackPage.getInstance();
  pageNavigator.addPage(devicePage);
  pageNavigator.addPage(outputPage);
  pageNavigator.addPage(inputPage);
  pageNavigator.addPage(feedbackPage);
  window.addEventListener('hashchange', function() {
    const pageName = window.location.hash.substr(1);
    if ($(pageName)) {
      pageNavigator.showPage(pageName);
    }
  });
  // Set default page to DevicePage.
  if (!window.location.hash) {
    pageNavigator.showPage(devicePage.pageName);
  }

  $('output-btn').addEventListener('click', function() {
    pageNavigator.showPage(outputPage.pageName);
  });
  $('input-btn').addEventListener('click', function() {
    pageNavigator.showPage(inputPage.pageName);
  });
  pageNavigator.showPage(window.location.hash.substr(1));
}

document.addEventListener('DOMContentLoaded', initialize);
