// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Creates the Camera App main object.
 * @constructor
 */
camera.Camera = function() {
  /**
   * @type {camera.Camera.Context}
   * @private
   */
  this.context_ = new camera.Camera.Context(
      this.isUIAnimating_.bind(this),
      this.onError_.bind(this),
      this.onErrorRecovered_.bind(this));

  /**
   * @type {Array.<camera.ViewsStack>}
   * @private
   */
  this.viewsStack_ = new camera.Camera.ViewsStack();

  /**
   * @type {camera.Router}
   * @private
   */
  this.router_ = new camera.Router(
      this.navigateById_.bind(this),
      this.viewsStack_.pop.bind(this.viewsStack_));

  /**
   * @type {camera.views.Camera}
   * @private
   */
  this.cameraView_ = new camera.views.Camera(this.context_, this.router_);

  /**
   * @type {camera.views.Album}
   * @private
   */
  this.albumView_ = new camera.views.Album(this.context_, this.router_);

  /**
   * @type {camera.views.Browser}
   * @private
   */
  this.browserView_ = new camera.views.Browser(this.context_, this.router_);

  /**
   * @type {camera.views.Dialog}
   * @private
   */
  this.dialogView_ = new camera.views.Dialog(this.context_, this.router_);

  /**
   * @type {?number}
   * @private
   */
  this.resizingTimer_ = null;

  /**
   * @type {camera.util.TooltipManager}
   * @private
   */
  this.tooltipManager_ = new camera.util.TooltipManager();

  // End of properties. Seal the object.
  Object.seal(this);

  // Handle key presses to make the Camera app accessible via the keyboard.
  document.body.addEventListener('keydown', this.onKeyPressed_.bind(this));

  // Handle window resize.
  window.addEventListener('resize', this.onWindowResize_.bind(this));

  // Animate the error icon.
  document.querySelector('#error .icon').addEventListener('mousedown',
      this.onErrorIconClicked_.bind(this));
  document.querySelector('#error .icon').addEventListener('touchstart',
      this.onErrorIconClicked_.bind(this));

  // Set the localized window title.
  document.title = chrome.i18n.getMessage('name');
};

/**
 * Creates context for the views.
 *
 * @param {function():boolean} isUIAnimating Checks if the UI is animating.
 * @param {function(string, string, opt_string)} onError Callback to be called,
 *     when an error occurs. Arguments: identifier, first line, second line.
 * @param {function(string)} onErrorRecovered Callback to be called,
 *     when the error goes away. The argument is the error id.
 * @constructor
 */
camera.Camera.Context = function(isUIAnimating, onError, onErrorRecovered) {
  camera.View.Context.call(this);

  /**
   * @type {boolean}
   */
  this.resizing = false;

  /**
   * @type {boolean}
   */
  this.hasError = false;

  /**
   * @type {function():boolean}
   */
  this.isUIAnimating = isUIAnimating;

  /**
   * @type {function(string, string, string)}
   */
  this.onError = onError;

  /**
   * @type {function(string)}
   */
  this.onErrorRecovered = onErrorRecovered;

  // End of properties. Seal the object.
  Object.seal(this);
};

camera.Camera.Context.prototype = {
  __proto__: camera.View.Context.prototype
};

/**
 * Creates a stack of views.
 * @constructor
 */
camera.Camera.ViewsStack = function() {
  /**
   * Stack with the views as well as return callbacks.
   * @type {Array.<Object>}
   * @private
   */
  this.stack_ = [];

  // No more properties. Seal the object.
  Object.seal(this);
};

camera.Camera.ViewsStack.prototype = {
  get current() {
    return this.stack_.length ? this.stack_[this.stack_.length - 1].view : null;
  }
};

/**
 * Adds the view on the stack and hence makes it the current one. Optionally,
 * passes the arguments to the view.
 *
 * @param {camera.View} view View to be pushed on top of the stack.
 * @param {Object=} opt_arguments Optional arguments.
 * @param {function(Object=)} opt_callback Optional result callback to be called
 *     when the view is closed.
 */
camera.Camera.ViewsStack.prototype.push = function(
    view, opt_arguments, opt_callback) {
  if (this.current)
    this.current.inactivate();

  this.stack_.push({
    view: view,
    callback: opt_callback || function(result) {}
  });

  view.enter(opt_arguments);
  view.activate();
};

/**
 * Removes the current view from the stack and hence makes the previous one
 * the current one. Calls the callback passed to the previous view's navigate()
 * method with the result.
 *
 * @param {Object=} opt_result Optional result. If not passed, then undefined
 *     will be passed to the callback.
 */
camera.Camera.ViewsStack.prototype.pop = function(opt_result) {
  var entry = this.stack_.pop();
  entry.view.inactivate();
  entry.view.leave();

  if (this.current)
    this.current.activate();
  if (entry.callback)
    entry.callback(opt_result);
};

camera.Camera.prototype = {
  get currentView() {
    return this.viewsStack_.current;
  },
  get cameraView() {
    return this.cameraView_;
  },
  get albumView() {
    return this.albumView_;
  },
  get browserView() {
    return this.browserView_;
  },
  get dialogView() {
    return this.dialogView_;
  }
};

/**
 * Starts the app by initializing views and showing the camera view.
 */
camera.Camera.prototype.start = function() {
  // Initialize all views, and then start the app.
  Promise.all([
      new Promise(this.cameraView_.initialize.bind(this.cameraView_)),
      new Promise(this.albumView_.initialize.bind(this.albumView_)),
      new Promise(this.browserView_.initialize.bind(this.browserView_))]).
    then(function() {
      this.tooltipManager_.initialize();
      this.viewsStack_.push(this.cameraView_);
      camera.util.makeElementsUnfocusableByMouse();
      camera.util.setAriaAttributes();
    }.bind(this)).
    catch(function(error) {
      console.error('Failed to initialize the Camera app.', error);
    });
};

/**
 * Switches the view using a router's view identifier.
 *
 * @param {camera.Router.ViewIdentifier} viewIdentifier View identifier.
 * @param {Object=} opt_arguments Optional arguments for the view.
 * @param {function(Object=)} opt_callback Optional result callback to be called
 *     when the view is closed.
 * @private
 */
camera.Camera.prototype.navigateById_ = function(
    viewIdentifier, opt_arguments, opt_callback) {
  switch (viewIdentifier) {
    case camera.Router.ViewIdentifier.CAMERA:
      this.viewsStack_.push(this.cameraView_, opt_arguments, opt_callback);
      break;
    case camera.Router.ViewIdentifier.ALBUM:
      this.viewsStack_.push(this.albumView_, opt_arguments, opt_callback);
      break;
    case camera.Router.ViewIdentifier.BROWSER:
      this.viewsStack_.push(this.browserView_, opt_arguments, opt_callback);
      break;
    case camera.Router.ViewIdentifier.DIALOG:
      this.viewsStack_.push(this.dialogView_, opt_arguments, opt_callback);
      break;
  }
};

/**
 * Handles resizing of the window.
 * @private
 */
camera.Camera.prototype.onWindowResize_ = function() {
  // Suspend capturing while resizing for smoother UI.
  this.context_.resizing = true;
  if (this.resizingTimer_) {
    clearTimeout(this.resizingTimer_);
    this.resizingTimer_ = null;
  }
  this.resizingTimer_ = setTimeout(function() {
    this.resizingTimer_ = null;
    this.context_.resizing = false;
  }.bind(this), 100);

  if (this.currentView)
    this.currentView.onResize();
};

/**
 * Handles clicking on the error icon.
 * @private
 */
camera.Camera.prototype.onErrorIconClicked_ = function() {
  var icon = document.querySelector('#error .icon');
  icon.classList.remove('animate');
  setTimeout(function() {
    icon.classList.add('animate');
  }, 0);
};

/**
 * Handles pressed keys.
 * @param {Event} event Key press event.
 * @private
 */
camera.Camera.prototype.onKeyPressed_ = function(event) {
  if (this.context_.hasError)
    return;
  this.currentView.onKeyPressed(event);
};

/**
 * Shows an error message.
 *
 * @param {string} identifier Identifier of the error.
 * @param {string} message Message for the error.
 * @param {string=} opt_hint Optional hint for the error message.
 * @private
 */
camera.Camera.prototype.onError_ = function(identifier, message, opt_hint) {
  document.body.classList.add('has-error');
  this.context_.hasError = true;
  document.querySelector('#error-msg').textContent = message;
  document.querySelector('#error-msg-hint').textContent = opt_hint || '';
};

/**
 * Checks if any of UI elements are animating.
 * @return {boolean} True if animating, false otherwise.
 * @private
 */
camera.Camera.prototype.isUIAnimating_ = function() {
  return this.tooltipManager_.animating;
};

/**
 * Removes the error message when an error goes away.
 * @param {string} identifier Identifier of the error.
 * @private
 */
camera.Camera.prototype.onErrorRecovered_ = function(identifier) {
  // TODO(mtomasz): Implement identifiers handling in case of multiple
  // error messages at once.
  this.context_.hasError = false;
  document.body.classList.remove('has-error');
};

/**
 * @type {camera.Camera} Singleton of the Camera object.
 * @private
 */
camera.Camera.instance_ = null;

/**
 * Returns the singleton instance of the Camera class.
 * @return {camera.Camera} Camera object.
 */
camera.Camera.getInstance = function() {
  if (!camera.Camera.instance_)
    camera.Camera.instance_ = new camera.Camera();
  return camera.Camera.instance_;
};

/**
 * Creates the Camera object and starts screen capturing.
 */
document.addEventListener('DOMContentLoaded', function() {
  camera.Camera.getInstance().start();
  chrome.app.window.current().show();
});

