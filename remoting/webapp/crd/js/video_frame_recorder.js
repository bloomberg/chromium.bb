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
 * @implements {remoting.ProtocolExtension}
 */
remoting.VideoFrameRecorder = function() {
  /** @private {?function(string,string)} */
  this.sendMessageToHostCallback_ = null;

  this.fileWriter_ = null;
  this.isRecording_ = false;
};

/** @private {string} */
remoting.VideoFrameRecorder.EXTENSION_TYPE = 'video-recorder';

/** @return {Array<string>} */
remoting.VideoFrameRecorder.prototype.getExtensionTypes = function() {
  return [remoting.VideoFrameRecorder.EXTENSION_TYPE];
};

/**
 * @param {function(string,string)} sendMessageToHost Callback to send a message
 *     to the host.
 */
remoting.VideoFrameRecorder.prototype.startExtension =
    function(sendMessageToHost) {
  this.sendMessageToHostCallback_ = sendMessageToHost;
};

/**
 * @param {Object} data The data to send.
 * @private
 */
remoting.VideoFrameRecorder.prototype.sendMessageToHost_ = function(data) {
  this.sendMessageToHostCallback_(remoting.VideoFrameRecorder.EXTENSION_TYPE,
                                  JSON.stringify(data));
}

/**
 * Handles 'video-recorder' extension messages and returns true. Returns
 * false for all other message types.
 *
 * @param {string} type Type of extension message.
 * @param {Object} message The parsed extension message data.
 * @return {boolean}
 */
remoting.VideoFrameRecorder.prototype.onExtensionMessage =
    function(type, message) {
  console.assert(type == remoting.VideoFrameRecorder.EXTENSION_TYPE,
                'Unexpected extension message type: ' + type + '.');

  var messageType = base.getStringAttr(message, 'type');
  var messageData = base.getStringAttr(message, 'data');

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
    var videoPacketString = window.atob(messageData);

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
  this.sendMessageToHost_(data);
}

/**
 * Returns true if the session is currently being recorded.
 * @return {boolean}
 */
remoting.VideoFrameRecorder.prototype.isRecording = function() {
  return this.isRecording_;
}

/**
 * @param {Entry=} entry The single file entry if multiple files are not
 *     allowed.
 * @param {Array<!FileEntry>=} fileEntries List of file entries if multiple
 *     files are allowed.
 */
remoting.VideoFrameRecorder.prototype.onFileChosen_ = function(
    entry, fileEntries) {
  if (!entry) {
    console.log("Cancelled save of video frames.");
  } else {
    chrome.fileSystem.getDisplayPath(entry, function(path) {
      console.log("Saving video frames to:" + path);
    });
    entry.createWriter(this.onFileWriter_.bind(this));
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
  this.sendMessageToHost_(data);
}
