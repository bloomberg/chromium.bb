// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.addWebUIListener('dark-mode-changed', darkMode => {
  document.documentElement.toggleAttribute('dark', darkMode);
});

chrome.send('observeDarkMode');

/** @return {boolean} Whether the page is in dark mode. */
function inDarkMode() {
  return document.documentElement.hasAttribute('dark');
}
