// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @struct
 * @extends {PolymerElement}
 */
function FilesToggleRipple() {}

/**
 * @type {boolean}
 */
FilesToggleRipple.prototype.activated;

/**
 * @constructor
 * @struct
 * @extends {PolymerElement}
 */
function FilesToast() {}

/**
 * @type {boolean}
 */
FilesToast.prototype.visible;

/**
 * @type {number}
 */
FilesToast.prototype.timeout;

/**
 * @param {string} text
 * @param {{text: string, callback: function()}=} opt_action
 */
FilesToast.prototype.show = function(text, opt_action) {};

/**
 * @return {!Promise}
 */
FilesToast.prototype.hide = function() {};

/**
 * @constructor
 * @struct
 * @extends {PolymerElement}
 */
function AudioPlayerElement() {}

/** @type {boolean} */
AudioPlayerElement.prototype.playing;

/** @type {number} */
AudioPlayerElement.prototype.time;

/** @type {boolean} */
AudioPlayerElement.prototype.shuffule;

/** @type {boolean} */
AudioPlayerElement.prototype.repeat;

/** @type {number} */
AudioPlayerElement.prototype.volume;

/** @type {boolean} */
AudioPlayerElement.prototype.expanded;

/** @type {number} */
AudioPlayerElement.prototype.currentTrackIndex;

/** @type {string} */
AudioPlayerElement.prototype.currenttrackurl;

/** @type {number} */
AudioPlayerElement.prototype.playcount;

/** @type {Array<!Object>} */
AudioPlayerElement.prototype.tracks;

/** @type {Object} */
AudioPlayerElement.prototype.model;

/** @type {boolean} */
AudioPlayerElement.prototype.volumeSliderShown;

AudioPlayerElement.prototype.onPageUnload = function() {};

AudioPlayerElement.prototype.onAudioError = function() {};
