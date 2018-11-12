// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var cca = cca || {};

/**
 * Creates a named view controller attached to the root element.
 * @param {cca.Router} router View router to switch views.
 * @param {HTMLElement} rootElement Root element of the view.
 * @param {string} name View name.
 * @constructor
 */
cca.View = function(router, rootElement, name) {
  /**
   * @type {boolean}
   * @private
   */
  this.active_ = false;

  /**
   * @type {boolean}
   * @private
   */
  this.entered_ = false;

  /**
   * @type {cca.Router}
   * @private
   */
  this.router_ = router;

  /**
   * @type {HTMLElement}
   * @private
   */
  this.rootElement_ = rootElement;

  /**
   * @type {string}
   * @private
   */
  this.name_ = name;
};

cca.View.prototype = {
  get entered() {
    return this.entered_;
  },
  get active() {
    return this.active_;
  },
  get router() {
    return this.router_;
  },
};

/**
 * Processes key pressed events.
 * @param {Event} event Key event.
 */
cca.View.prototype.onKeyPressed = function(event) {
};

/**
 * Processes resizing events.
 */
cca.View.prototype.onResize = function() {
};

/**
 * Handles entering the view.
 * @param {Object=} opt_arguments Optional arguments.
 */
cca.View.prototype.onEnter = function(opt_arguments) {
};

/**
 * Handles leaving the view.
 */
cca.View.prototype.onLeave = function() {
};

/**
 * Handles activating the view.
 */
cca.View.prototype.onActivate = function() {
};

/**
 * Handles inactivating the view.
 */
cca.View.prototype.onInactivate = function() {
};

/**
 * Enters the view.
 * @param {Object=} opt_arguments Optional arguments.
 */
cca.View.prototype.enter = function(opt_arguments) {
  document.body.classList.add(this.name_);
  this.entered_ = true;
  this.onEnter(opt_arguments);
};

/**
 * Leaves the view.
 */
cca.View.prototype.leave = function() {
  this.onLeave();
  this.entered_ = false;
  document.body.classList.remove(this.name_);
};

/**
 * Activates the view by restoring child elements' tabindex.
 */
cca.View.prototype.activate = function() {
  this.rootElement_.querySelectorAll('[data-tabindex]').forEach((element) => {
    element.setAttribute('tabindex', element.dataset.tabindex);
    element.removeAttribute('data-tabindex');
  });
  this.active_ = true;
  this.onActivate();
};

/**
 * Inactivates the view by making child elements unfocusable.
 */
cca.View.prototype.inactivate = function() {
  this.onInactivate();
  this.active_ = false;
  this.rootElement_.querySelectorAll('[tabindex]').forEach((element) => {
    element.dataset.tabindex = element.getAttribute('tabindex');
    element.setAttribute('tabindex', '-1');
  });
};
