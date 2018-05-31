// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

(function() {

/**
 * Returns the title and artist text associated with the given track.
 *
 * @param {string} audioAppId The Audio Player window ID.
 * @param {query} track Query for the Audio Player track.
 * @return {Promise<Object>} Promise to be fulfilled with a track details
 *     object containing {title:string, artist:string}.
 */
function getTrackText(audioAppId, track) {
  var titleElement = audioPlayerApp.callRemoteTestUtil(
      'queryAllElements', audioAppId, [track + ' > .data > .data-title']);
  var artistElement = audioPlayerApp.callRemoteTestUtil(
      'queryAllElements', audioAppId, [track + ' > .data > .data-artist']);
  return Promise.all([titleElement, artistElement]).then((data) => {
    return {
      title: data[0][0] && data[0][0].text,
      artist: data[1][0] && data[1][0].text
    };
  });
}

/*
 * Returns an Audio Player current track URL query for the given file name.
 *
 * @return {string} Track query for file name.
 */
function audioTrackQuery(fileName) {
  return '[currenttrackurl$="' + self.encodeURIComponent(fileName) + '"]';
}

/**
 * Returns a query for when the Audio Player is playing the given file name.
 *
 * @param {string} fileName The file name.
 * @return {string} Query for file name being played.
 */
function audioPlayingQuery(fileName) {
  return 'audio-player[playing]' + audioTrackQuery(fileName);
}

/**
 * Converts a file name to a file system scheme URL for a given volume path.
 * TODO(noel): remove all uses of this routine.
 *
 * @param {string} path Directory path: Downloads or Drive.
 * @param {string} fileName The file name.
 */
function audioFileSystemURL(path, fileName) {
  return 'filesystem:chrome-extension://' + AUDIO_PLAYER_APP_ID + '/external'
      + path + '/' + self.encodeURIComponent(fileName);
}

/**
 * Tests opening then closing the Audio Player from Files app.
 *
 * @param {string} path Directory path to be tested: Downloads or Drive.
 */
function audioOpenClose(path) {
  let audioAppId;
  let appId;

  StepsRunner.run([
    // Open Files.App on the given (volume) path.
    function() {
      setupAndWaitUntilReady(null, path, this.next);
    },
    // Open an audio file.
    function(result) {
      appId = result.windowId;
      remoteCall.callRemoteTestUtil(
          'openFile', appId, ['Beautiful Song.ogg'], this.next);
    },
    // Wait for the Audio Player window.
    function(result) {
      chrome.test.assertTrue(result);
      audioPlayerApp.waitForWindow('audio_player.html').then(this.next);
    },
    // Check: Audio Player should automatically play the file.
    function(windowId) {
      audioAppId = windowId;
      const playFile = audioPlayingQuery('Beautiful Song.ogg');
      audioPlayerApp.waitForElement(audioAppId, playFile).then(this.next);
    },
    // Close the Audio Player window.
    function() {
      audioPlayerApp.closeWindowAndWait(audioAppId).then(this.next);
    },
    // Check: Audio Player window should close.
    function(result) {
      chrome.test.assertTrue(!!result, 'Failed to close audio window');
      this.next();
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests if the audio player shows up for the selected image and that the audio
 * is loaded successfully.
 *
 * @param {string} path Directory path to be tested.
 */
function audioOpen(path) {
  var appId;
  var audioAppId;

  var expectedFilesBefore =
      TestEntryInfo.getExpectedRows(path == RootPath.DRIVE ?
          BASIC_DRIVE_ENTRY_SET : BASIC_LOCAL_ENTRY_SET).sort();
  var expectedFilesAfter =
      expectedFilesBefore.concat([ENTRIES.newlyAdded.getExpectedRow()]).sort();

  StepsRunner.run([
    // Open Files.App on the given (volume) path.
    function() {
      setupAndWaitUntilReady(null, path, this.next);
    },
    // Add an additional audio file.
    function(results) {
      appId = results.windowId;
      addEntries(['local', 'drive'], [ENTRIES.newlyAdded], this.next);
    },
    // Wait for the file list to change.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForFiles(appId, expectedFilesAfter).then(this.next);
    },
    // Open an audio file.
    function() {
      remoteCall.callRemoteTestUtil(
          'openFile', appId, ['Beautiful Song.ogg'], this.next);
    },
    // Wait for the Audio Player window.
    function(result) {
      chrome.test.assertTrue(result);
      audioPlayerApp.waitForWindow('audio_player.html').then(this.next);
    },
    // Check: Audio Player should automatically play the file.
    function(windowId) {
      audioAppId = windowId;
      const playFile = audioPlayingQuery('Beautiful Song.ogg');
      audioPlayerApp.waitForElement(audioAppId, playFile).then(this.next);
    },
    // Check: track 0 should be active.
    function() {
      getTrackText(audioAppId, '.track[index="0"][active]').then(this.next);
    },
    // Check: track 0 should have the correct title and artist.
    function(song) {
      chrome.test.assertEq('Beautiful Song', song.title);
      chrome.test.assertEq('Unknown Artist', song.artist);
      this.next();
    },
    // Check: track 1 should be inactive.
    function() {
      const inactive = '.track[index="1"]:not([active])';
      audioPlayerApp.waitForElement(audioAppId, inactive).then(this.next);
    },
    // Open another file.
    function(element) {
      chrome.test.assertTrue(!!element);
      remoteCall.callRemoteTestUtil(
          'openFile', appId, ['newly added file.ogg'], this.next);
    },
    // Check: Audio Player should automatically play the file.
    function(result) {
      chrome.test.assertTrue(result);
      const playFile = audioPlayingQuery('newly added file.ogg');
      audioPlayerApp.waitForElement(audioAppId, playFile).then(this.next);
    },
    // Check: track 1 should be active.
    function() {
      getTrackText(audioAppId, '.track[index="1"][active]').then(this.next);
    },
    // Check: track 1 should have the correct title and artist.
    function(song) {
      chrome.test.assertEq('newly added file', song.title);
      chrome.test.assertEq('Unknown Artist', song.artist);
      this.next();
    },
    // Check: track 0 should be inactive.
    function() {
      const inactive = '.track[index="0"]:not([active])';
      audioPlayerApp.waitForElement(audioAppId, inactive).then(this.next);
    },
    function(element) {
      chrome.test.assertTrue(!!element);
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests if the audio player play the next file after the current file.
 *
 * @param {string} path Directory path to be tested.
 */
function audioAutoAdvance(path) {
  var appId;
  var audioAppId;

  var expectedFilesBefore =
      TestEntryInfo.getExpectedRows(path == RootPath.DRIVE ?
          BASIC_DRIVE_ENTRY_SET : BASIC_LOCAL_ENTRY_SET).sort();
  var expectedFilesAfter =
      expectedFilesBefore.concat([ENTRIES.newlyAdded.getExpectedRow()]).sort();

  StepsRunner.run([
    // Open Files.App on the given (volume) path.
    function() {
      setupAndWaitUntilReady(null, path, this.next);
    },
    // Add an additional audio file.
    function(results) {
      appId = results.windowId;
      addEntries(['local', 'drive'], [ENTRIES.newlyAdded], this.next);
    },
    // Wait for the file list to change.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForFiles(appId, expectedFilesAfter).then(this.next);
    },
    // Open an audio file.
    function() {
      remoteCall.callRemoteTestUtil(
          'openFile', appId, ['Beautiful Song.ogg'], this.next);
    },
    // Wait for the Audio Player window.
    function(result) {
      chrome.test.assertTrue(result);
      audioPlayerApp.waitForWindow('audio_player.html').then(this.next);
    },
    // Check: Audio Player should automatically play the file.
    function(windowId) {
      audioAppId = windowId;
      const playFile = audioPlayingQuery('Beautiful Song.ogg');
      audioPlayerApp.waitForElement(audioAppId, playFile).then(this.next);
    },
    // Wait for next song.
    function() {
      const playFile = audioPlayingQuery('newly added file.ogg');
      audioPlayerApp.waitForElement(audioAppId, playFile).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests if the audio player play the next file after the current file.
 *
 * @param {string} path Directory path to be tested.
 */
function audioRepeatAllModeSingleFile(path) {
  var appId;
  var audioAppId;

  StepsRunner.run([
    // Open Files.App on the given (volume) path.
    function() {
      setupAndWaitUntilReady(null, path, this.next);
    },
    // Open an audio file.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
          'openFile', appId, ['Beautiful Song.ogg'], this.next);
    },
    // Wait for the Audio Player window.
    function(result) {
      chrome.test.assertTrue(result);
      audioPlayerApp.waitForWindow('audio_player.html').then(this.next);
    },
    // Check: Audio Player should automatically play the file.
    function(windowId) {
      audioAppId = windowId;
      const playFile = audioPlayingQuery('Beautiful Song.ogg');
      audioPlayerApp.waitForElement(audioAppId, playFile).then(this.next);
    },
    // Click the repeat button for repeat-all.
    function() {
      audioPlayerApp.callRemoteTestUtil(
          'fakeMouseClick',
          audioAppId,
          ['audio-player /deep/ repeat-button .no-repeat'],
          this.next);
    },
    function(result) {
      chrome.test.assertTrue(result, 'Failed to click the repeat button');

      var selector = 'audio-player[playing][playcount="1"]';
      audioPlayerApp.waitForElement(audioAppId, selector).then(this.next);
    },
    // Check: Beautiful Song.ogg should be playing.
    function(element) {
      chrome.test.assertEq(audioFileSystemURL(path, 'Beautiful Song.ogg'),
          element.attributes.currenttrackurl);
      this.next();
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests if the audio player play the next file after the current file.
 *
 * @param {string} path Directory path to be tested.
 */
function audioNoRepeatModeSingleFile(path) {
  var appId;
  var audioAppId;

  StepsRunner.run([
    // Open Files.App on the given (volume) path.
    function() {
      setupAndWaitUntilReady(null, path, this.next);
    },
    // Open an audio file.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
          'openFile', appId, ['Beautiful Song.ogg'], this.next);
    },
    // Wait for the Audio Player window.
    function(result) {
      chrome.test.assertTrue(result);
      audioPlayerApp.waitForWindow('audio_player.html').then(this.next);
    },
    // Check: Audio Player should automatically play the file.
    function(windowId) {
      audioAppId = windowId;
      const playFile = audioPlayingQuery('Beautiful Song.ogg');
      audioPlayerApp.waitForElement(audioAppId, playFile).then(this.next);
    },
    // When it ends, Audio Player should stop playing.
    function() {
      var selector = 'audio-player[playcount="1"]:not([playing])';
      audioPlayerApp.waitForElement(audioAppId, selector).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests if the audio player play the next file after the current file.
 *
 * @param {string} path Directory path to be tested.
 */
function audioRepeatOneModeSingleFile(path) {
  var appId;
  var audioAppId;

  StepsRunner.run([
    // Open Files.App on the given (volume) path.
    function() {
      setupAndWaitUntilReady(null, path, this.next);
    },
    // Open an audio file.
    function(results) {
      appId = results.windowId;
      remoteCall.callRemoteTestUtil(
          'openFile', appId, ['Beautiful Song.ogg'], this.next);
    },
    // Wait for the Audio Player window.
    function(result) {
      chrome.test.assertTrue(result);
      audioPlayerApp.waitForWindow('audio_player.html').then(this.next);
    },
    // Check: Audio Player should automatically play the file.
    function(windowId) {
      audioAppId = windowId;
      const playFile = audioPlayingQuery('Beautiful Song.ogg');
      audioPlayerApp.waitForElement(audioAppId, playFile).then(this.next);
    },
    // Click the repeat button for repeat-all.
    function() {
      audioPlayerApp.callRemoteTestUtil(
          'fakeMouseClick',
          audioAppId,
          ['audio-player /deep/ repeat-button .no-repeat'],
          this.next);
    },
    // Click the repeat button again for repeat-once.
    function(result) {
      chrome.test.assertTrue(result, 'Failed to click the repeat button');
      audioPlayerApp.callRemoteTestUtil(
          'fakeMouseClick',
          audioAppId,
          ['audio-player /deep/ repeat-button .repeat-all'],
          this.next);
    },
    function(result) {
      chrome.test.assertTrue(result, 'Failed to click the repeat button');

      var selector = 'audio-player[playing][playcount="1"]';
      audioPlayerApp.waitForElement(audioAppId, selector).then(this.next);
    },
    // Check: Beautiful Song.ogg should be playing.
    function(element) {
      chrome.test.assertEq(audioFileSystemURL(path, 'Beautiful Song.ogg'),
          element.attributes.currenttrackurl);
      this.next();
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests if the audio player play the next file after the current file.
 *
 * @param {string} path Directory path to be tested.
 */
function audioRepeatAllModeMultipleFile(path) {
  var appId;
  var audioAppId;

  var expectedFilesBefore =
      TestEntryInfo.getExpectedRows(path == RootPath.DRIVE ?
          BASIC_DRIVE_ENTRY_SET : BASIC_LOCAL_ENTRY_SET);
  var expectedFilesAfter =
      expectedFilesBefore.concat([ENTRIES.newlyAdded.getExpectedRow()]);

  StepsRunner.run([
    // Open Files.App on the given (volume) path.
    function() {
      setupAndWaitUntilReady(null, path, this.next);
    },
    // Add an additional audio file.
    function(results) {
      appId = results.windowId;
      addEntries(['local', 'drive'], [ENTRIES.newlyAdded], this.next);
    },
    // Wait for the file list to change.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForFiles(appId, expectedFilesAfter).then(this.next);
    },
    // Open an audio file.
    function() {
      remoteCall.callRemoteTestUtil(
          'openFile', appId, ['newly added file.ogg'], this.next);
    },
    // Wait for the Audio Player window.
    function(result) {
      chrome.test.assertTrue(result);
      audioPlayerApp.waitForWindow('audio_player.html').then(this.next);
    },
    // Check: Audio Player should automatically play the file.
    function(windowId) {
      audioAppId = windowId;
      const playFile = audioPlayingQuery('newly added file.ogg');
      audioPlayerApp.waitForElement(audioAppId, playFile).then(this.next);
    },
    // Click the repeat button for repeat-all.
    function() {
      audioPlayerApp.callRemoteTestUtil(
          'fakeMouseClick',
          audioAppId,
          ['audio-player /deep/ repeat-button .no-repeat'],
          this.next);
    },
    // Wait for next song.
    function(result) {
      chrome.test.assertTrue(result, 'Failed to click the repeat button');
      const playFile = audioPlayingQuery('Beautiful Song.ogg');
      audioPlayerApp.waitForElement(audioAppId, playFile).then(this.next);
    },
    // TODO(noel): this test is broken. It should check that the first
    // song plays again (since repeat-all mode is active).
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests if the audio player play the next file after the current file.
 *
 * @param {string} path Directory path to be tested.
 */
function audioNoRepeatModeMultipleFile(path) {
  var appId;
  var audioAppId;

  var expectedFilesBefore =
      TestEntryInfo.getExpectedRows(path == RootPath.DRIVE ?
          BASIC_DRIVE_ENTRY_SET : BASIC_LOCAL_ENTRY_SET);
  var expectedFilesAfter =
      expectedFilesBefore.concat([ENTRIES.newlyAdded.getExpectedRow()]);

  StepsRunner.run([
    // Open Files.App on the given (volume) path.
    function() {
      setupAndWaitUntilReady(null, path, this.next);
    },
    // Add an additional audio file.
    function(results) {
      appId = results.windowId;
      addEntries(['local', 'drive'], [ENTRIES.newlyAdded], this.next);
    },
    // Wait for the file list to change.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForFiles(appId, expectedFilesAfter).then(this.next);
    },
    // Open an audio file.
    function() {
      remoteCall.callRemoteTestUtil(
          'openFile', appId, ['newly added file.ogg'], this.next);
    },
    // Wait for the Audio Player window.
    function(result) {
      chrome.test.assertTrue(result);
      audioPlayerApp.waitForWindow('audio_player.html').then(this.next);
    },
    // Check: Audio Player should automatically play the file.
    function(windowId) {
      audioAppId = windowId;
      const playFile = audioPlayingQuery('newly added file.ogg');
      audioPlayerApp.waitForElement(audioAppId, playFile).then(this.next);
    },
    // When it ends, Audio Player should stop playing.
    function() {
      const stopped = 'audio-player:not([playing])';
      audioPlayerApp.waitForElement(audioAppId, stopped).then(this.next);
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

/**
 * Tests if the audio player play the next file after the current file.
 *
 * @param {string} path Directory path to be tested.
 */
function audioRepeatOneModeMultipleFile(path) {
  var appId;
  var audioAppId;

  var expectedFilesBefore =
      TestEntryInfo.getExpectedRows(path == RootPath.DRIVE ?
          BASIC_DRIVE_ENTRY_SET : BASIC_LOCAL_ENTRY_SET);
  var expectedFilesAfter =
      expectedFilesBefore.concat([ENTRIES.newlyAdded.getExpectedRow()]);

  StepsRunner.run([
    // Open Files.App on the given (volume) path.
    function() {
      setupAndWaitUntilReady(null, path, this.next);
    },
    // Add an additional audio file.
    function(results) {
      appId = results.windowId;
      addEntries(['local', 'drive'], [ENTRIES.newlyAdded], this.next);
    },
    // Wait for the file list to change.
    function(result) {
      chrome.test.assertTrue(result);
      remoteCall.waitForFiles(appId, expectedFilesAfter).then(this.next);
    },
    // Open an audio file.
    function() {
      remoteCall.callRemoteTestUtil(
          'openFile', appId, ['newly added file.ogg'], this.next);
    },
    // Wait for the Audio Player window.
    function(result) {
      chrome.test.assertTrue(result);
      audioPlayerApp.waitForWindow('audio_player.html').then(this.next);
    },
    // Check: Audio Player should automatically play the file.
    function(windowId) {
      audioAppId = windowId;
      const playFile = audioPlayingQuery('newly added file.ogg');
      audioPlayerApp.waitForElement(audioAppId, playFile).then(this.next);
    },
    // Click the repeat button for repeat-all.
    function() {
      audioPlayerApp.callRemoteTestUtil(
          'fakeMouseClick',
          audioAppId,
          ['audio-player /deep/ repeat-button .no-repeat'],
          this.next);
    },
    // Click the repeat button again for repeat-once.
    function() {
      audioPlayerApp.callRemoteTestUtil(
          'fakeMouseClick',
          audioAppId,
          ['audio-player /deep/ repeat-button .repeat-all'],
          this.next);
    },
    function(result) {
      chrome.test.assertTrue(result, 'Failed to click the repeat button');

      var selector = 'audio-player[playing][playcount="1"]';
      audioPlayerApp.waitForElement(audioAppId, selector).then(this.next);
    },
    // Check: newly added file.ogg should be playing.
    function(element) {
      chrome.test.assertEq(audioFileSystemURL(path, 'newly added file.ogg'),
          element.attributes.currenttrackurl);
      this.next();
    },
    function() {
      checkIfNoErrorsOccured(this.next);
    }
  ]);
}

testcase.audioOpenCloseDownloads = function() {
  audioOpenClose(RootPath.DOWNLOADS);
};

testcase.audioOpenCloseDrive = function() {
  audioOpenClose(RootPath.DRIVE);
};

testcase.audioOpenDownloads = function() {
  audioOpen(RootPath.DOWNLOADS);
};

testcase.audioOpenDrive = function() {
  audioOpen(RootPath.DRIVE);
};

testcase.audioAutoAdvanceDrive = function() {
  audioAutoAdvance(RootPath.DRIVE);
};

testcase.audioRepeatAllModeSingleFileDrive = function() {
  audioRepeatAllModeSingleFile(RootPath.DRIVE);
};

testcase.audioNoRepeatModeSingleFileDrive = function() {
  audioNoRepeatModeSingleFile(RootPath.DRIVE);
};

testcase.audioRepeatOneModeSingleFileDrive = function() {
  audioRepeatOneModeSingleFile(RootPath.DRIVE);
};

testcase.audioRepeatAllModeMultipleFileDrive = function() {
  audioRepeatAllModeMultipleFile(RootPath.DRIVE);
};

testcase.audioNoRepeatModeMultipleFileDrive = function() {
  audioNoRepeatModeMultipleFile(RootPath.DRIVE);
};

testcase.audioRepeatOneModeMultipleFileDrive = function() {
  audioRepeatOneModeMultipleFile(RootPath.DRIVE);
};

})();
