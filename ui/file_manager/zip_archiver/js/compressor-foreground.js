// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

window.onload = function() {
  chrome.runtime.getBackgroundPage(function(backgroundPage) {

    // Called from the background page. We need to place this function here
    // because chrome.fileSystem.chooseEntry only works on forground page.
    backgroundPage.unpacker.Compressor.prototype.createArchiveFileForeground_ =
        function(compressorId) {
          var compressor =
              backgroundPage.unpacker.app.compressors[compressorId];
          var suggestedName = compressor.archiveName_;
          // Create an archive file.
          chrome.fileSystem.chooseEntry(
              {type: 'saveFile', suggestedName: suggestedName},
              function(entry, fileEntries) {
                if (!entry) {
                  console.error('Failed to create an archive file.');
                  compressor.onError_(compressor.compressorId_);
                  return;
                }

                compressor.archiveFileEntry_ = entry;

                compressor.sendCreateArchiveRequest_();
              });
        };

    // Some compressors are waiting for this foreground page to be loaded.
    backgroundPage.unpacker.Compressor.CompressorIdQueue.forEach(
        function(compressorId) {
          var compressor =
              backgroundPage.unpacker.app.compressors[compressorId];
          compressor.createArchiveFileForeground_(compressorId);
        });
  });
};
