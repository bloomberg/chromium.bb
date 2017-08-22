// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.module('mr.dial.MediaSource');

const Logger = goog.require('mr.Logger');
const base64 = goog.require('mr.base64');


/**
 * Represents a DIAL media source containing information specific to a DIAL
 * launch.
 */
const MediaSource = class {
  /**
   * @param {string} appName The DIAL application name.
   * @param {string=} launchParameter DIAL application launch parameter.
   */
  constructor(appName, launchParameter = '') {
    /** @const {string} */
    this.appName = appName;
    /** @const {string} */
    this.launchParameter = launchParameter;
  }

  /**
   * Constructs a DIAL media source from a URL. The URL can take on the new
   * format (with dial: protocol) or the old format (with https: protocol).
   * @param {string} urlString The media source URL.
   * @return {?MediaSource} A DIAL media source if the parse was
   *     successful, null otherwise.
   */
  static create(urlString) {
    let url;
    try {
      url = new URL(urlString);
    } catch (err) {
      MediaSource.logger_.info('Invalid URL: ' + urlString);
      return null;
    }
    switch (url.protocol) {
      case 'dial:':
        return MediaSource.parseDialUrl_(url);
      case 'https:':

        return MediaSource.parseLegacyUrl_(url);
      default:
        MediaSource.logger_.fine('Unhandled protocol: ' + url.protocol);
        return null;
    }
  }

  /**
   * Parses the given URL using the new DIAL URL format, which takes the form:
   * dial:<App name>?postData=<base64-encoded launch parameters>
   * @param {!URL} url
   * @return {?MediaSource}
   * @private
   */
  static parseDialUrl_(url) {
    const appName = url.pathname;
    if (!appName.match(/^\w+$/)) {
      MediaSource.logger_.warning('Invalid app name: ' + appName);
      return null;
    }
    let postData = url.searchParams.get('postData') || undefined;
    if (postData) {
      try {
        postData = base64.decodeString(postData);
      } catch (err) {
        MediaSource.logger_.warning(
            'Invalid base64 encoded postData:' + postData);
        return null;
      }
    }
    return new MediaSource(appName, postData);
  }

  /**
   * Parses the given URL using the legacy format specified in
   * http://goo.gl/8qKAE7
   * Example:
   * http://www.youtube.com/tv#__dialAppName__=YouTube/__dialPostData__=dj0xMjM=
   * @param {!URL} url
   * @return {?MediaSource}
   * @private
   */
  static parseLegacyUrl_(url) {
    // Parse URI and get fragment.
    const fragment = url.hash;
    if (!fragment) return null;
    let appName = MediaSource.APP_NAME_REGEX_.exec(fragment);
    appName = appName ? appName[1] : null;
    if (!appName) return null;
    appName = decodeURIComponent(appName);

    let postData = MediaSource.LAUNCH_PARAM_REGEX_.exec(fragment);
    postData = postData ? postData[1] : undefined;
    if (postData) {
      try {
        postData = base64.decodeString(postData);
      } catch (err) {
        MediaSource.logger_.warning(
            'Invalid base64 encoded postData:' + postData);
        return null;
      }
    }
    return new MediaSource(appName, postData);
  }
};


/** @const @private {?Logger} */
MediaSource.logger_ = Logger.getInstance('mr.dial.MediaSource');


/** @const {string} */
MediaSource.URN_PREFIX = 'urn:dial-multiscreen-org:dial:application:';


/** @private @const {!RegExp} */
MediaSource.APP_NAME_REGEX_ = /__dialAppName__=([A-Za-z0-9-._~!$&'()*+,;=%]+)/;


/** @private @const {!RegExp} */
MediaSource.LAUNCH_PARAM_REGEX_ = /__dialPostData__=([A-Za-z0-9]+={0,2})/;


exports = MediaSource;
