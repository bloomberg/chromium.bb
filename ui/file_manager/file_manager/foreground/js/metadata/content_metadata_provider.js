// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {!MessagePort=} opt_messagePort Message port overriding the default
 *     worker port.
 * @extends {NewMetadataProvider}
 * @constructor
 * @struct
 */
function ContentMetadataProvider(opt_messagePort) {
  NewMetadataProvider.call(
      this,
      ContentMetadataProvider.PROPERTY_NAMES);

  /**
   * Pass all URLs to the metadata reader until we have a correct filter.
   * @private {RegExp}
   */
  this.urlFilter_ = /.*/;

  /**
   * @private {!MessagePort}
   * @const
   */
  this.dispatcher_ = opt_messagePort ?
      opt_messagePort :
      new SharedWorker(ContentMetadataProvider.WORKER_SCRIPT).port;
  this.dispatcher_.onmessage = this.onMessage_.bind(this);
  this.dispatcher_.postMessage({verb: 'init'});
  this.dispatcher_.start();

  /**
   * Initialization is not complete until the Worker sends back the
   * 'initialized' message.  See below.
   * @private {boolean}
   */
  this.initialized_ = false;

  /**
   * Map from Entry.toURL() to callback.
   * Note that simultaneous requests for same url are handled in MetadataCache.
   * @private {!Object<!string, !Array<function(Object)>>}
   * @const
   */
  this.callbacks_ = {};
}

/**
 * @const {!Array<string>}
 */
ContentMetadataProvider.PROPERTY_NAMES = [
  'contentImageTransform',
  'contentThumbnailTransform',
  'contentThumbnailUrl',
  'exifLittleEndian',
  'ifd',
  'imageHeight',
  'imageWidth',
  'mediaArtist',
  'mediaMimeType',
  'mediaTitle'
];

/**
 * Path of a worker script.
 * @public {string}
 */
ContentMetadataProvider.WORKER_SCRIPT =
    'chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/' +
    'foreground/js/metadata/metadata_dispatcher.js';

/**
 * Converts content metadata from parsers to the internal format.
 * @param {Object} metadata The content metadata.
 * @return {!MetadataItem} Converted metadata.
 */
ContentMetadataProvider.convertContentMetadata = function(metadata) {
  var item = new MetadataItem();
  item.contentImageTransform = metadata['imageTransform'];
  item.contentThumbnailTransform = metadata['thumbnailTransform'];
  item.contentThumbnailUrl = metadata['thumbnailURL'];
  item.exifLittleEndian = metadata['littleEndian'];
  item.ifd = metadata['ifd'];
  item.imageHeight = metadata['height'];
  item.imageWidth = metadata['width'];
  item.mediaArtist = metadata['artist'];
  item.mediaMimeType = metadata['mimeType'];
  item.mediaTitle = metadata['title'];
  return item;
};

ContentMetadataProvider.prototype.__proto__ = NewMetadataProvider.prototype;

/**
 * @override
 */
ContentMetadataProvider.prototype.get = function(requests) {
  if (!requests.length)
    return Promise.resolve([]);

  var promises = [];
  for (var i = 0; i < requests.length; i++) {
    promises.push(new Promise(function(request, fulfill) {
      this.getImpl_(request.entry, request.names, fulfill);
    }.bind(this, requests[i])));
  }
  return Promise.all(promises);
};

/**
 * Fetches the metadata.
 * @param {Entry} entry File entry.
 * @param {!Array<string>} names Requested metadata type.
 * @param {function(Object)} callback Callback expects a map from metadata type
 *     to metadata value. This callback is called asynchronously.
 * @private
 */
ContentMetadataProvider.prototype.getImpl_ = function(entry, names, callback) {
  if (entry.isDirectory) {
    setTimeout(callback.bind(null, {}), 0);
    return;
  }
  var url = entry.toURL();
  if (this.callbacks_[url]) {
    this.callbacks_[url].push(callback);
  } else {
    this.callbacks_[url] = [callback];
    this.dispatcher_.postMessage({verb: 'request', arguments: [url]});
  }
};

/**
 * Dispatch a message from a metadata reader to the appropriate on* method.
 * @param {Object} event The event.
 * @private
 */
ContentMetadataProvider.prototype.onMessage_ = function(event) {
  var data = event.data;
  switch (data.verb) {
    case 'initialized':
      this.onInitialized_(data.arguments[0]);
      break;
    case 'result':
      this.onResult_(
          data.arguments[0],
          data.arguments[1] ?
          ContentMetadataProvider.convertContentMetadata(data.arguments[1]) :
          new MetadataItem());
      break;
    case 'error':
      this.onError_(
          data.arguments[0],
          data.arguments[1],
          data.arguments[2],
          data.arguments[3]);
      break;
    case 'log':
      this.onLog_(data.arguments[0]);
      break;
    default:
      assertNotReached();
      break;
  }
};

/**
 * Handles the 'initialized' message from the metadata reader Worker.
 * @param {RegExp} regexp Regexp of supported urls.
 * @private
 */
ContentMetadataProvider.prototype.onInitialized_ = function(regexp) {
  this.urlFilter_ = regexp;

  // Tests can monitor for this state with
  // ExtensionTestMessageListener listener("worker-initialized");
  // ASSERT_TRUE(listener.WaitUntilSatisfied());
  // Automated tests need to wait for this, otherwise we crash in
  // browser_test cleanup because the worker process still has
  // URL requests in-flight.
  util.testSendMessage('worker-initialized');
  this.initialized_ = true;
};

/**
 * Handles the 'result' message from the worker.
 * @param {string} url File url.
 * @param {!MetadataItem} metadataItem The metadata item.
 * @private
 */
ContentMetadataProvider.prototype.onResult_ = function(url, metadataItem) {
  var callbacks = this.callbacks_[url];
  delete this.callbacks_[url];
  for (var i = 0; i < callbacks.length; i++) {
    callbacks[i](metadataItem);
  }
};

/**
 * Handles the 'error' message from the worker.
 * @param {string} url File entry.
 * @param {string} step Step failed.
 * @param {string} errorDescription Error description.
 * @param {Object?} metadata The metadata, if available.
 * @private
 */
ContentMetadataProvider.prototype.onError_ = function(
    url, step, errorDescription, metadata) {
  // For error case, fill all fields with error object.
  var error = new ContentMetadataProvider.Error(url, step, errorDescription);
  var item = new MetadataItem();
  item.contentImageTransformError = error;
  item.contentThumbnailTransformError = error;
  item.contentThumbnailUrlError = error;
  item.exifLittleEndianError = error;
  item.ifdError = error;
  item.imageHeightError = error;
  item.imageWidthError = error;
  item.mediaArtistError = error;
  item.mediaMimeTypeError = error;
  item.mediaTitleError = error;

  this.onResult_(url, item);
};

/**
 * Handles the 'log' message from the worker.
 * @param {Array.<*>} arglist Log arguments.
 * @private
 */
ContentMetadataProvider.prototype.onLog_ = function(arglist) {
  console.log.apply(console, ['ContentMetadataProvider log:'].concat(arglist));
};

/**
 * Content metadata provider error.
 * @param {string} url File Entry.
 * @param {string} step Step failed.
 * @param {string} errorDescription Error description.
 * @constructor
 * @struct
 * @extends {Error}
 * @suppress {checkStructDictInheritance}
 */
ContentMetadataProvider.Error = function(url, step, errorDescription) {
  /**
   * @public {string}
   */
  this.url = url;

  /**
   * @public {string}
   */
  this.step = step;

  /**
   * @public {string}
   */
  this.errorDescription = errorDescription;
};

ContentMetadataProvider.Error.prototype.__proto__ = Error.prototype;
