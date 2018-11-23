// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(function() {

/**
 * Tests opening (then closing) the image Gallery from Files app.
 *
 * @param {string} path Directory path (Downloads or Drive).
 */
function imageOpen(path) {
  let appId;
  let galleryAppId;

  StepsRunner.run([
    // Open Files.App on |path|, add image3 to Downloads and Drive.
    function() {
      setupAndWaitUntilReady(
          null, path, this.next, [ENTRIES.image3], [ENTRIES.image3]);
    },
    // Open the image file in Files app.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
          'openFile', appId, [ENTRIES.image3.targetPath], this.next);
    },
    // Check: the Gallery window should open.
    function(result) {
      chrome.test.assertTrue(result);
      galleryApp.waitForWindow('gallery.html').then(this.next);
    },
    // Check: the image should appear in the Gallery window.
    function(windowId) {
      galleryAppId = windowId;
      galleryApp.waitForSlideImage(
          galleryAppId, 640, 480, 'image3').then(this.next);
    },
    // Close the Gallery window.
    function() {
      galleryApp.closeWindowAndWait(galleryAppId).then(this.next);
    },
    // Check: the Gallery window should close.
    function(result) {
      chrome.test.assertTrue(result, 'Failed to close Gallery window');
      this.next();
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests opening the image Gallery from Files app: once the Gallery opens and
 * shows the initial image, open a different image from FilesApp.
 *
 * @param {string} path Directory path (Downloads or Drive).
 */
function imageOpenGalleryOpen(path) {
  let appId;
  let galleryAppId;

  const testImages = [ENTRIES.image3, ENTRIES.desktop];

  StepsRunner.run([
    // Open Files.App on |path|, add test images to Downloads and Drive.
    function() {
      setupAndWaitUntilReady(null, path, this.next, testImages, testImages);
    },
    // Open an image file in Files app.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
          'openFile', appId, [ENTRIES.image3.targetPath], this.next);
    },
    // Wait a11y-msg to have some text.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForElement(appId, '#a11y-msg:not(:empty)').then(this.next);
    },
    // Fetch A11y messages.
    function() {
      remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, [])
          .then(this.next);
    },
    // Check that opening the file was announced to screen reader.
    function(a11yMessages) {
      chrome.test.assertTrue(a11yMessages instanceof Array);
      chrome.test.assertEq(1, a11yMessages.length);
      chrome.test.assertEq('Opening file image3.jpg.', a11yMessages[0]);
      this.next();
    },
    // Check: the Gallery window should open.
    function() {
      galleryApp.waitForWindow('gallery.html').then(this.next);
    },
    // Check: the image should appear in the Gallery window.
    function(windowId) {
      galleryAppId = windowId;
      galleryApp.waitForSlideImage(
          galleryAppId, 640, 480, 'image3').then(this.next);
    },
    // Now open a different image file in Files app.
    function() {
      remoteCall.callRemoteTestUtil(
          'openFile', appId, [ENTRIES.desktop.targetPath], this.next);
    },
    // Check: the new image should appear in the Gallery window.
    function() {
      galleryApp.waitForSlideImage(
          galleryAppId, 800, 600, 'My Desktop Background').then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

testcase.imageOpenDownloads = function() {
  imageOpen(RootPath.DOWNLOADS);
};

testcase.imageOpenDrive = function() {
  imageOpen(RootPath.DRIVE);
};

testcase.imageOpenGalleryOpenDownloads = function() {
  imageOpenGalleryOpen(RootPath.DOWNLOADS);
};

testcase.imageOpenGalleryOpenDrive = function() {
  imageOpenGalleryOpen(RootPath.DRIVE);
};

})();
