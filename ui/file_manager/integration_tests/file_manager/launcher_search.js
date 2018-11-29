// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Tests for interface we expose to Launcher app's search feature.
 */

'use strict';

(function() {

/**
 * Tests opening an image using the Launcher app's search feature.
 */
testcase.launcherOpenSearchResult = async function() {
  const imageName = ENTRIES.desktop.nameText;

  // Create an image file in Drive.
  await addEntries(['drive'], [ENTRIES.desktop]);

  // Get the image file URL.
  const fileURLs = await remoteCall.callRemoteTestUtil(
      'getFilesUnderVolume', null, ['drive', [imageName]]);

  // Request Files app to open the image URL.
  // This emulates the Launcher interaction with Files app.
  chrome.test.assertEq(1, fileURLs.length);
  await remoteCall.callRemoteTestUtil(
      'launcherSearchOpenResult', null, [fileURLs[0]]);

  // Files app opens Gallery for images, so check Gallery app has been
  // launched.
  const galleryAppId = await galleryApp.waitForWindow('gallery.html');

  // Check the image is displayed.
  const imageNameNoExtension = imageName.split('.')[0];

  // Check: the slide image element should appear.
  chrome.test.assertTrue(
      !!await galleryApp.waitForSlideImage(
          galleryAppId, 0, 0, imageNameNoExtension),
      'Failed to find the slide image element');
};
})();
