// Copyright 2015  The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('image.collections.extension.Controller');

goog.require('goog.events.EventHandler');
goog.require('goog.events.EventTarget');

goog.scope(function() {



/**
 * A base class for all controller classes. Controllers connect UI of the
 * 'Google Stars' Chrome extension (see go/stars for details) to backends
 * (such as GWS, Chrome Sync etc.) Should not be instantiated by itself.
 * Each derived controller should handle certain types of UI events
 * (e.g. search requests, clustering requests etc.)
 *
 * @extends {goog.events.EventTarget}
 * @constructor
 */
image.collections.extension.Controller = function() {
  Controller.base(this, 'constructor');

  /** @protected {!goog.events.EventHandler} */
  this.eventHandler = new goog.events.EventHandler(this);
  this.registerDisposable(this.eventHandler);
};
goog.inherits(image.collections.extension.Controller, goog.events.EventTarget);
var Controller = image.collections.extension.Controller;


/**
 * Initializes the controller. By default, sets the parent event target
 * (controllers communicate with UI by handling and dispatching events
 * on this event target).
 * @param {!goog.events.EventTarget} parentEventTarget
 */
Controller.prototype.initialize = function(parentEventTarget) {
  this.setParentEventTarget(parentEventTarget);
};
});  // goog.scope
