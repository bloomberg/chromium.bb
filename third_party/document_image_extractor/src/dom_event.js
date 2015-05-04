// Copyright 2015  The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file provides the DomEvent class.
 */

goog.provide('image.collections.extension.DomEvent');

goog.require('goog.events.Event');

goog.scope(function() {



/**
 * Represents the content script DOM event.
 * @param {!DomEvent.Type} type The type of this event.
 * @extends {goog.events.Event}
 * @constructor
 */
image.collections.extension.DomEvent = function(type) {
  DomEvent.base(this, 'constructor', type);
};
goog.inherits(image.collections.extension.DomEvent, goog.events.Event);
var DomEvent = image.collections.extension.DomEvent;


/** @enum {string} */
DomEvent.Type = {
  DOM_INITIALIZED: 'dom_initialized',
  INITIALIZE_DOM: 'initialize_dom'
};

});  // goog.scope
