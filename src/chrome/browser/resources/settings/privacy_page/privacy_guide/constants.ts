// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Steps in the privacy guide flow in their order of appearance. The page
 * updates from those steps to show the corresponding page content.
 */
export enum PrivacyGuideStep {
  WELCOME = 'welcome',
  MSBB = 'msbb',
  CLEAR_ON_EXIT = 'clearOnExit',
  HISTORY_SYNC = 'historySync',
  SAFE_BROWSING = 'safeBrowsing',
  COOKIES = 'cookies',
  COMPLETION = 'completion',
}
