// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Extension ID of gallery app.
 * @type {string}
 * @const
 */
var GALLERY_APP_ID = 'nlkncpkkdoccmpiclbokaimcnedabhhm';

var gallery = new RemoteCallGallery(GALLERY_APP_ID);

/**
 * Launches the gallery with the given entries.
 *
 * @param {string} testVolumeName Test volume name passed to the addEntries
 *     function. Either 'drive' or 'local'.
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @param {Array<TestEntryInfo>} entries Entries to be parepared and passed to
 *     the application.
 * @param {Array<TestEntryInfo>=} opt_selected Entries to be selected. Should
 *     be a sub-set of the entries argument.
 * @return {Promise} Promise to be fulfilled with the data of the main element
 *     in the allery.
 */
function launch(testVolumeName, volumeType, entries, opt_selected) {
  var entriesPromise = addEntries([testVolumeName], entries).then(function() {
    var selectedEntries = opt_selected || entries;
    var selectedEntryNames =
        selectedEntries.map(function(entry) { return entry.nameText; });
    return gallery.callRemoteTestUtil(
        'getFilesUnderVolume', null, [volumeType, selectedEntryNames]);
  });

  var appId = null;
  var urls = [];
  return entriesPromise.then(function(result) {
    urls = result;
    return gallery.callRemoteTestUtil('openGallery', null, [urls]);
  }).then(function(windowId) {
    chrome.test.assertTrue(!!windowId);
    appId = windowId;
    return gallery.waitForElement(appId, 'div.gallery');
  }).then(function(args) {
    return {
      appId: appId,
      mailElement: args[0],
      urls: urls,
    };
  });
}

/**
 * Namespace for test cases.
 */
var testcase = {};

// Ensure the test cases are loaded.
window.addEventListener('load', function() {
  var steps = [
    // Check for the guest mode.
    function() {
      chrome.test.sendMessage(
          JSON.stringify({name: 'isInGuestMode'}), steps.shift());
    },
    // Obtain the test case name.
    function(result) {
      if (JSON.parse(result) != chrome.extension.inIncognitoContext)
        return;
      chrome.test.sendMessage(
          JSON.stringify({name: 'getRootPaths'}), steps.shift());
    },
    // Obtain the root entry paths.
    function(result) {
      var roots = JSON.parse(result);
      RootPath.DOWNLOADS = roots.downloads;
      RootPath.DRIVE = roots.drive;
      chrome.test.sendMessage(
          JSON.stringify({name: 'getTestName'}), steps.shift());
    },
    // Run the test case.
    function(testCaseName) {
      var targetTest = testcase[testCaseName];
      if (!targetTest) {
        chrome.test.fail(testCaseName + ' is not found.');
        return;
      }
      // Specify the name of test to the test system.
      targetTest.generatedName = testCaseName;
      chrome.test.runTests([function() {
        return testPromiseAndApps(targetTest(), [gallery]);
      }]);
    }
  ];
  steps.shift()();
});
