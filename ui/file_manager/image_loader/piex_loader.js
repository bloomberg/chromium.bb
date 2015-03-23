// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   fulfill: function(PiexLoaderResponse):undefined,
 *   reject: function(string):undefined}
 * }}
 */
var PiexRequestCallbacks;

/**
 * @param {{id:number, thumbnail:!ArrayBuffer, orientation:number}}
 *     data Data directly returned from NaCl module.
 * @constructor
 * @struct
 */
function PiexLoaderResponse(data) {
  /**
   * @public {number}
   * @const
   */
  this.id = data.id;

  /**
   * @public {!ArrayBuffer}
   * @const
   */
  this.thumbnail = data.thumbnail;

  /**
   * @public {!ImageOrientation}
   * @const
   */
  this.orientation =
      ImageOrientation.fromExifOrientation(data.orientation);
}

/**
 * @constructor
 * @struct
 */
function PiexLoader() {
  /**
   * @private {!Element}
   * @const
   */
  this.naclModule_ = document.createElement('embed');

  /**
   * @private {!Promise}
   * @const
   */
  this.naclPromise_ = new Promise(function(fulfill) {
    var embed = this.naclModule_;
    embed.setAttribute('type', 'application/x-pnacl');
    // The extension nmf is not allowed to load. We uses .nmf.js instead.
    embed.setAttribute('src', '/pnacl/piex_loader.nmf.js');
    embed.width = 0;
    embed.height = 0;

    // The <EMBED> element is wrapped inside a <DIV>, which has both a 'load'
    // and a 'message' event listener attached.  This wrapping method is used
    // instead of attaching the event listeners directly to the <EMBED> element
    // to ensure that the listeners are active before the NaCl module 'load'
    // event fires.
    var listenerContainer = document.createElement('div');
    listenerContainer.appendChild(embed);
    listenerContainer.addEventListener('load', fulfill, true);
    listenerContainer.addEventListener(
        'message', this.onMessage_.bind(this), true);
    listenerContainer.addEventListener('error', function() {
      console.error(embed['lastError']);
    }.bind(this), true);
    listenerContainer.addEventListener('crash', function() {
      console.error('PiexLoader crashed.');
    }.bind(this), true);
    listenerContainer.style.height = '0px';
    document.body.appendChild(listenerContainer);
  }.bind(this));

  /**
   * @private {!Object<number, PiexRequestCallbacks>}
   * @const
   */
  this.requests_ = {};

  /**
   * @private {number}
   */
  this.requestIdCount_ = 0;
}

/**
 * @param {Event} event
 * @private
 */
PiexLoader.prototype.onMessage_ = function(event) {
  var id = event.data.id;
  if (!event.data.error) {
    var response = new PiexLoaderResponse(event.data);
    this.requests_[id].fulfill(response);
  } else {
    this.requests_[id].reject(event.data.error);
  }
  delete this.requests_[id];
};

/**
 * Starts to load RAW image.
 * @param {string} url
 * @return {!Promise<!PiexLoaderResponse>}
 */
PiexLoader.prototype.load = function(url) {
  return this.naclPromise_.then(function() {
    var message = {
      id: this.requestIdCount_++,
      name: 'loadThumbnail',
      url: url
    };
    this.naclModule_.postMessage(message);
    return new Promise(function(fulfill, reject) {
      this.requests_[message.id] = {fulfill: fulfill, reject: reject};
    }.bind(this));
  }.bind(this));
};
