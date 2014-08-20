// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Class implement the video frame recorder extension client.
 */

'use strict';

/** @suppress {duplicate} */
var remoting = remoting || {};

/**
 * @constructor
 * @param {remoting.ClientPlugin} plugin
 */
remoting.VideoFrameRecorder = function(plugin) {
  this.fileWriter_ = null;
  this.isRecording_ = false;
  this.plugin_ = plugin;
};

/**
 * Starts or stops video recording.
 */
remoting.VideoFrameRecorder.prototype.startStopRecording = function() {
  var data = {};
  if (this.isRecording_) {
    this.isRecording_ = false;
    data = { type: 'stop' }

    chrome.fileSystem.chooseEntry(
        {type: 'saveFile', suggestedName: 'videoRecording.pb'},
        this.onFileChosen_.bind(this));
  } else {
    this.isRecording_ = true;
    data = { type: 'start' }
  }
  this.plugin_.sendClientMessage('video-recorder', JSON.stringify(data));
}

/**
 * Returns true if the session is currently being recorded.
 * @return {boolean}
 */
remoting.VideoFrameRecorder.prototype.isRecording = function() {
  return this.isRecording_;
}

/**
 * Handles 'video-recorder' extension messages and returns true. Returns
 * false for all other message types.
 * @param {string} type Type of extension message.
 * @param {string} data Content of the extension message.
 * @return {boolean}
 */
remoting.VideoFrameRecorder.prototype.handleMessage =
    function(type, data) {
  if (type != 'video-recorder') {
    return false;
  }

  var message = getJsonObjectFromString(data);
  var messageType = getStringAttr(message, 'type');
  var messageData = getStringAttr(message, 'data');

  if (messageType == 'next-frame-reply') {
    if (!this.fileWriter_) {
      console.log("Received frame but have no writer");
      return true;
    }
    if (!messageData) {
      console.log("Finished receiving frames");
      this.fileWriter_ = null;
      return true;
    }

    console.log("Received frame");
    /* jscompile gets confused if you refer to this as just atob(). */
    var videoPacketString = /** @type {string?} */ window.atob(messageData);

    console.log("Converted from Base64 - length:" + videoPacketString.length);
    var byteArrays = [];

    for (var offset = 0; offset < videoPacketString.length; offset += 512) {
        var slice = videoPacketString.slice(offset, offset + 512);
        var byteNumbers = new Array(slice.length);
        for (var i = 0; i < slice.length; i++) {
            byteNumbers[i] = slice.charCodeAt(i);
        }
        var byteArray = new Uint8Array(byteNumbers);
        byteArrays.push(byteArray);
    }

    console.log("Writing frame");
    videoPacketString = null;
    /**
     * Our current compiler toolchain only understands the old (deprecated)
     * Blob constructor, which does not accept any parameters.
     * TODO(wez): Remove this when compiler is updated (see crbug.com/405298).
     * @suppress {checkTypes}
     * @param {Array} parts
     * @return {Blob}
     */
    var makeBlob = function(parts) {
      return new Blob(parts);
    }
    var videoPacketBlob = makeBlob(byteArrays);
    byteArrays = null;

    this.fileWriter_.write(videoPacketBlob);

    return true;
  }

  console.log("Unrecognized message: " + messageType);
  return true;
}

/** @param {FileEntry} fileEntry */
remoting.VideoFrameRecorder.prototype.onFileChosen_ = function(fileEntry) {
  if (!fileEntry) {
    console.log("Cancelled save of video frames.");
  } else {
    /** @type {function(string):void} */
    chrome.fileSystem.getDisplayPath(fileEntry, function(path) {
      console.log("Saving video frames to:" + path);
    });
    fileEntry.createWriter(this.onFileWriter_.bind(this));
  }
}

/** @param {FileWriter} fileWriter */
remoting.VideoFrameRecorder.prototype.onFileWriter_ = function(fileWriter) {
  console.log("Obtained FileWriter for video frame write");
  fileWriter.onwriteend = this.onWriteComplete_.bind(this);
  this.fileWriter_ = fileWriter;
  this.fetchNextFrame_();
}

remoting.VideoFrameRecorder.prototype.onWriteComplete_ = function(e) {
  console.log("Video frame write complete");
  this.fetchNextFrame_();
}

remoting.VideoFrameRecorder.prototype.fetchNextFrame_ = function() {
  console.log("Request next video frame");
  var data = { type: 'next-frame' }
  this.plugin_.sendClientMessage('video-recorder', JSON.stringify(data));
}
