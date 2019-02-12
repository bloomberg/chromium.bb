// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * The openSingleImage test for Downloads.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.openSingleVideoOnDownloads = function() {
  var test = openSingleVideo('local', 'downloads', ENTRIES.world);
  return test
      .then(function() {
        // Video player starts playing given file automatically.
        return waitForFunctionResult('isPlaying', 'world.ogv', true);
      })
      .then(function() {
        // Play will finish in 2 seconds (world.ogv is 2-second short movie.)
        return waitForFunctionResult('isPlaying', 'world.ogv', false);
      });
};

/**
 * The openSingleImage test for Drive.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.openSingleVideoOnDrive = function() {
  var test = openSingleVideo('drive', 'drive', ENTRIES.world);
  return test
      .then(function() {
        // Video player starts playing given file automatically.
        return waitForFunctionResult('isPlaying', 'world.ogv', true);
      })
      .then(function() {
        // Play will finish in 2 seconds (world.ogv is 2-second short movie.)
        return waitForFunctionResult('isPlaying', 'world.ogv', false);
      });
};

/**
 * Test that if player can sccussfully search subtitle with same url.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.openVideoWithSubtitle = async function() {
  await openSingleVideo('local', 'downloads', ENTRIES.world, ENTRIES.subtitle);
  await waitForFunctionResult('isPlaying', 'world.ogv', true);
  await waitForFunctionResult('hasSubtitle', 'world.ogv', true);
};

/**
 * Test that if player will ignore unrelated subtitle.
 * @return {Promise} Promise to be fulfilled with on success.
 */
testcase.openVideoWithoutSubtitle = async function() {
  await openSingleVideo('local', 'downloads', ENTRIES.video, ENTRIES.subtitle);
  await waitForFunctionResult('isPlaying', 'video_long.ogv', true);
  await waitForFunctionResult('hasSubtitle', 'video_long.ogv', false);
};
