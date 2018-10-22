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
testcase.launcherOpenSearchResult = function() {
  let galleryAppId;
  const imageName = ENTRIES.desktop.nameText;
  StepsRunner.run([
    // Create an image file in Drive.
    function() {
      addEntries(['drive'], [ENTRIES.desktop]).then(this.next);
    },
    // Get the image file URL.
    function() {
      remoteCall
          .callRemoteTestUtil(
              'getFilesUnderVolume', null, ['drive', [imageName]])
          .then(this.next);
    },
    // Request Files app to open the image URL.
    // This emulates the Launcher interaction with Files app.
    function(fileURLs) {
      chrome.test.assertEq(1, fileURLs.length);
      remoteCall.callRemoteTestUtil(
          'launcherSearchOpenResult', null, [fileURLs[0]], this.next);
    },
    // Files app opens Gallery for images, so check Gallery app has been
    // launched.
    function() {
      galleryApp.waitForWindow('gallery.html').then(this.next);
    },
    // Check the image is displayed.
    function(windowId) {
      galleryAppId = windowId;
      const imageNameNoExtension = imageName.split('.')[0];
      galleryApp.waitForSlideImage(galleryAppId, 0, 0, imageNameNoExtension)
          .then(this.next);
    },
    // Check that the previous step succeeded.
    function(imageElement) {
      chrome.test.assertTrue(
          !!imageElement, 'Failed to find the slide image element');
      this.next();
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
};

})();
