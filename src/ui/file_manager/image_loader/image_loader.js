// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Loads and resizes an image.
 * @constructor
 */
function ImageLoader() {
  /**
   * Persistent cache object.
   * @type {ImageCache}
   * @private
   */
  this.cache_ = new ImageCache();

  /**
   * Manages pending requests and runs them in order of priorities.
   * @type {Scheduler}
   * @private
   */
  this.scheduler_ = new Scheduler();

  /**
   * Piex loader for RAW images.
   * @private {!PiexLoader}
   */
  this.piexLoader_ = new PiexLoader();

  // Grant permissions to all volumes, initialize the cache and then start the
  // scheduler.
  chrome.fileManagerPrivate.getVolumeMetadataList(function(volumeMetadataList) {
    // Listen for mount events, and grant permissions to volumes being mounted.
    chrome.fileManagerPrivate.onMountCompleted.addListener(
        function(event) {
          if (event.eventType === 'mount' && event.status === 'success') {
            chrome.fileSystem.requestFileSystem(
                {volumeId: event.volumeMetadata.volumeId}, function() {});
          }
        });
    const initPromises = volumeMetadataList.map(function(volumeMetadata) {
      const requestPromise = new Promise(function(callback) {
        chrome.fileSystem.requestFileSystem(
            {volumeId: volumeMetadata.volumeId},
            /** @type {function(FileSystem=)} */(callback));
      });
      return requestPromise;
    });
    initPromises.push(new Promise(function(resolve, reject) {
      this.cache_.initialize(resolve);
    }.bind(this)));

    // After all initialization promises are done, start the scheduler.
    Promise.all(initPromises).then(this.scheduler_.start.bind(this.scheduler_));
  }.bind(this));

  // Listen for incoming requests.
  chrome.runtime.onMessageExternal.addListener(
      this.onIncomingRequest_.bind(this));
}

/**
 * List of extensions allowed to perform image requests.
 *
 * @const
 * @type {Array<string>}
 */
ImageLoader.ALLOWED_CLIENT_ORIGINS = [
  'chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj',  // File Manager
  'chrome-extension://nlkncpkkdoccmpiclbokaimcnedabhhm',  // Gallery
  'chrome-extension://jcgeabjmjgoblfofpppfkcoakmfobdko',  // Video Player
];

/**
 * Handler for incoming requests.
 *
 * @param {*} request_data A LoadImageRequest (received untyped).
 * @param {!MessageSender} sender
 * @param {function(*): void} sendResponse
 */
ImageLoader.prototype.onIncomingRequest_ = function(
    request_data, sender, sendResponse) {
  if (!sender.origin || !request_data) {
    return;
  }
  if (ImageLoader.ALLOWED_CLIENT_ORIGINS.indexOf(sender.origin) === -1) {
    return;
  }

  const request = /** @type {!LoadImageRequest} */ (request_data);

  // Sending a response may fail if the receiver already went offline.
  // This is not an error, but a normal and quite common situation.
  const failSafeSendResponse = function(response) {
    try {
      sendResponse(response);
    } catch (e) {
      // Ignore the error.
    }
  };
  // Incoming requests won't have the full type.
  assert(!(request.orientation instanceof ImageOrientation));
  assert(!(typeof request.orientation === 'number'));

  if (request.orientation) {
    request.orientation =
        ImageOrientation.fromRotationAndScale(request.orientation);
  } else {
    request.orientation = new ImageOrientation(1, 0, 0, 1);
  }
  return this.onMessage_(sender.origin, request, failSafeSendResponse);
};

/**
 * Handles a request. Depending on type of the request, starts or stops
 * an image task.
 *
 * @param {string} senderOrigin Sender's extension origin.
 * @param {!LoadImageRequest} request Pre-processed request.
 * @param {function(!LoadImageResponse)} callback Callback to be called to
 *     return response.
 * @return {boolean} True if the message channel should stay alive until the
 *     callback is called.
 * @private
 */
ImageLoader.prototype.onMessage_ = function(senderOrigin, request, callback) {
  const requestId = senderOrigin + ':' + request.taskId;
  if (request.cancel) {
    // Cancel a task.
    this.scheduler_.remove(requestId);
    return false;  // No callback calls.
  } else {
    // Create a request task and add it to the scheduler (queue).
    const requestTask = new ImageRequest(
        requestId, this.cache_, this.piexLoader_, request, callback);
    this.scheduler_.add(requestTask);
    return true;  // Request will call the callback.
  }
};

/**
 * Returns the singleton instance.
 * @return {ImageLoader} ImageLoader object.
 */
ImageLoader.getInstance = function() {
  if (!ImageLoader.instance_) {
    ImageLoader.instance_ = new ImageLoader();
  }
  return ImageLoader.instance_;
};
