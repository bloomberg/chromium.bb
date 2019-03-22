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
async function imageOpen(path) {
  // Open Files.App on |path|, add image3 to Downloads and Drive.
  const {appId} = await setupAndWaitUntilReady(
      null, path, null, [ENTRIES.image3], [ENTRIES.image3]);

  // Open the image file in Files app.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'openFile', appId, [ENTRIES.image3.targetPath]));

  // Check: the Gallery window should open.
  const galleryAppId = await galleryApp.waitForWindow('gallery.html');

  // Check: the image should appear in the Gallery window.
  await galleryApp.waitForSlideImage(galleryAppId, 640, 480, 'image3');

  // Close the Gallery window.
  chrome.test.assertTrue(
      await galleryApp.closeWindowAndWait(galleryAppId),
      'Failed to close Gallery window');
}

/**
 * Tests opening the image Gallery from Files app: once the Gallery opens and
 * shows the initial image, open a different image from FilesApp.
 *
 * @param {string} path Directory path (Downloads or Drive).
 */
async function imageOpenGalleryOpen(path) {
  const testImages = [ENTRIES.image3, ENTRIES.desktop];

  // Open Files.App on |path|, add test images to Downloads and Drive.
  const {appId} =
      await setupAndWaitUntilReady(null, path, null, testImages, testImages);

  // Open an image file in Files app.
  chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
      'openFile', appId, [ENTRIES.image3.targetPath]));

  // Wait a11y-msg to have some text.
  await remoteCall.waitForElement(appId, '#a11y-msg:not(:empty)');

  // Fetch A11y messages.
  const a11yMessages =
      await remoteCall.callRemoteTestUtil('getA11yAnnounces', appId, []);

  // Check that opening the file was announced to screen reader.
  chrome.test.assertTrue(a11yMessages instanceof Array);
  chrome.test.assertEq(1, a11yMessages.length);
  chrome.test.assertEq('Opening file image3.jpg.', a11yMessages[0]);

  // Check: the Gallery window should open.
  const galleryAppId = await galleryApp.waitForWindow('gallery.html');

  // Check: the image should appear in the Gallery window.
  await galleryApp.waitForSlideImage(galleryAppId, 640, 480, 'image3');

  // Now open a different image file in Files app.
  await remoteCall.callRemoteTestUtil(
      'openFile', appId, [ENTRIES.desktop.targetPath]);

  // Check: the new image should appear in the Gallery window.
  await galleryApp.waitForSlideImage(
      galleryAppId, 800, 600, 'My Desktop Background');
}

testcase.imageOpenDownloads = function() {
  return imageOpen(RootPath.DOWNLOADS);
};

testcase.imageOpenDrive = function() {
  return imageOpen(RootPath.DRIVE);
};

testcase.imageOpenGalleryOpenDownloads = function() {
  return imageOpenGalleryOpen(RootPath.DOWNLOADS);
};

testcase.imageOpenGalleryOpenDrive = function() {
  return imageOpenGalleryOpen(RootPath.DRIVE);
};
})();
