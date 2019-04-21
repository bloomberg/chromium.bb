// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var tabCapture = chrome.tabCapture;

var helloWorldPageUri = 'data:text/html;charset=UTF-8,' +
    encodeURIComponent('<html><body>Hello world!</body></html>');

chrome.test.runTests([
 function canOpenUpToThreeOffscreenTabs() {
   function stopAllStreams(streams) {
     for (var i = 0, end = streams.length; i < end; ++i) {
       streams[i].getVideoTracks()[0].stop();
     }
   }

   function launchTabsUntilLimitReached(streamsSoFar) {
     tabCapture.captureOffscreenTab(
         helloWorldPageUri,
         {video: true},
         function(stream) {
           if (streamsSoFar.length == 4) {
             // 5th off-screen tab capture should fail.
             chrome.test.assertLastError(
                 'Extension has already started too many off-screen tabs.');
             chrome.test.assertFalse(!!stream);
             stopAllStreams(streamsSoFar);
             chrome.test.succeed();
           } else if (stream) {
             streamsSoFar.push(stream);
             setTimeout(
                 function() {
                   launchTabsUntilLimitReached(streamsSoFar);
                 }, 0);
           } else {
             console.error("Failed to capture stream, iter #" +
                 streamsSoFar.length);
             stopAllStreams(streamsSoFar);
             chrome.test.fail();
           }
         });
   }

   launchTabsUntilLimitReached([]);
 }
]);
