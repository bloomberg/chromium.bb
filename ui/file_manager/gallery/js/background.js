// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Configuration of the Gallery window.
 * @const
 * @type {Object}
 */
var windowCreateOptions = {
  id: 'gallery',
  outerBounds: {
    minWidth: 860,
    minHeight: 554
  },
  frame: {
    color: '#1E2023'
  },
  hidden: true
};

/**
 * Backgound object. This is necessary for AppWindowWrapper.
 * @type {!BackgroundBase}
 */
var background = new BackgroundBase();

/**
 * Wrapper of gallery window.
 * @type {SingletonAppWindowWrapper}
 */
var gallery = new SingletonAppWindowWrapper('gallery.html',
    windowCreateOptions);

/**
 * Opens gallery window.
 * @param {!Array<string>} urls List of URL to show.
 * @return {!Promise} Promise to be fulfilled on success, or rejected on error.
 */
function openGalleryWindow(urls) {
  return new Promise(function(fulfill, reject) {
    util.URLsToEntries(urls).then(function(result) {
      fulfill(util.entriesToURLs(result.entries));
    }).catch(reject);
  }).then(function(urls) {
    if (urls.length === 0)
      return Promise.reject('No file to open.');

    // Opens a window.
    return new Promise(function(fulfill, reject) {
      gallery.launch(
          {urls: urls},
          false,
          fulfill.bind(null, gallery));
    }).then(function(gallery) {
      var galleryDocument = gallery.rawAppWindow.contentWindow.document;
      if (galleryDocument.readyState == 'complete')
        return gallery;

      return new Promise(function(fulfill, reject) {
        galleryDocument.addEventListener(
            'DOMContentLoaded', fulfill.bind(null, gallery));
      });
    });
  }).then(function(gallery) {
    gallery.rawAppWindow.show();
    return gallery.rawAppWindow.contentWindow.appID;
  }).catch(function(error) {
    console.error('Launch failed' + error.stack || error);
    return Promise.reject(error);
  });
}

background.setLaunchHandler(openGalleryWindow);
