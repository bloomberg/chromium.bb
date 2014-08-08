// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Media manager class.
 * This class supports the information for the media file.
 * @param {FileEntry} entry Entry of media file. This must be a external entry.
 * @constructor
 */
function MediaManager(entry) {
  this.entry_ = entry;

  this.cachedDriveProp_ = null;
  this.cachedUrl_ = null;
  this.cachedToken_ = null;

  Object.seal(this);
}

MediaManager.prototype = {};

/**
 * Checks if the file is available for cast or not.
 *
 * @return {Promise} Promise which is resolved with boolean. If true, the file
 *     is available for cast. False, otherwise.
 */
MediaManager.prototype.isAvailableForCast = function() {
  return this.getUrl().then(function(url) {
    return true;
  }, function() {
    return false;
  });
};

/**
 * Retrieves the token for cast.
 *
 * @param {boolean} refresh If true, force to refresh a token. If false, use the
 *     cached token if available.
 * @return {Promise} Promise which is resolved with the token. Reject if failed.
 */
MediaManager.prototype.getToken = function(refresh) {
  if (this.cachedToken_ && !refresh)
    return Promise.resolve(this.cachedToken_);

  return new Promise(function(fulfill, reject) {
    // TODO(yoshiki): Creates the method to get a token and use it.
    chrome.fileBrowserPrivate.getDownloadUrl(this.entry_.toURL(), fulfill);
  }.bind(this)).then(function(url) {
    if (!url)
      return Promise.reject('Token fetch failed.');
    var token = url.substring(url.indexOf('access_token=') + 13);
    if (token) {
      this.cachedToken_ = token;
      return token;
    } else {
      return Promise.reject('Token fetch failed.');
    }
  }.bind(this));
};

/**
 * Retrieves the url for cast.
 *
 * @return {Promise} Promise which is resolved with the url. Reject if failed.
 */
MediaManager.prototype.getUrl = function() {
  if (this.cachedUrl_)
    return Promise.resolve(this.cachedUrl_);

  return new Promise(function(fulfill, reject) {
    // TODO(yoshiki): Creates the method to get a url and use it.
    chrome.fileBrowserPrivate.getDownloadUrl(this.entry_.toURL(), fulfill);
  }.bind(this)).then(function(url) {
    if (!url)
      return Promise.reject('URL fetch failed.');
    var access_token_index = url.indexOf('access_token=');
    if (access_token_index) {
      url = url.substring(0, access_token_index - 1);
    }
    this.cachedUrl_ = url;
    return url;
  }.bind(this));
};

/**
 * Retrieves the mime of file.
 *
 * @return {Promise} Promise which is resolved with the mime. Reject if failed.
 */
MediaManager.prototype.getMime = function() {
  if (this.cachedDriveProp_)
    return Promise.resolve(this.cachedDriveProp_.thumbnailUrl);

  return new Promise(function(fulfill, reject) {
    chrome.fileBrowserPrivate.getDriveEntryProperties(
        [this.entry_.toURL()], fulfill);
  }.bind(this)).then(function(props) {
    if (!props || !props[0] || !props[0].contentMimeType) {
      // TODO(yoshiki): Adds a logic to guess the mime.
      return Promise.reject('Mime fetch failed.');
    } else {
      this.cachedDriveProp_ = props[0];
      return props[0].contentMimeType;
    }
  }.bind(this));
};

/**
 * Retrieves the thumbnail url of file.
 *
 * @return {Promise} Promise which is resolved with the url. Reject if failed.
 */
MediaManager.prototype.getThumbnail = function() {
  if (this.cachedDriveProp_)
    return Promise.resolve(this.cachedDriveProp_.thumbnailUrl);

  return new Promise(function(fulfill, reject) {
    chrome.fileBrowserPrivate.getDriveEntryProperties(
        [this.entry_.toURL()], fulfill);
  }.bind(this)).then(function(props) {
    if (!props || !props[0] || !props[0].thumbnailUrl) {
      // TODO(yoshiki): Adds a logic to guess the mime.
      return Promise.reject('Thumbnail fetch failed.');
    } else {
      this.cachedDriveProp_ = props[0];
      return props[0].thumbnailUrl;
    }
  }.bind(this));
};
