// Copyright (c) 2013 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Namespace for the Camera app.
 */
var camera = camera || {};

/**
 * Namespace for views.
 */
camera.views = camera.views || {};

/**
 * Creates the Camera view controller.
 * @param {camera.View.Context} context Context object.
 * @param {camera.Router} router View router to switch views.
 * @constructor
 */
camera.views.Camera = function(context, router) {
  camera.View.call(
      this, context, router, document.querySelector('#camera'), 'camera');

  /**
   * Gallery model used to save taken pictures.
   * @type {camera.models.Gallery}
   * @private
   */
  this.model_ = null;

  /**
   * Video element to catch the stream and plot it later onto a canvas.
   * @type {Video}
   * @private
   */
  this.video_ = document.createElement('video');

  /**
   * @type {?number}
   * @private
   */
  this.retryStartTimer_ = null;

  /**
   * @type {?number}
   * @private
   */
  this.watchdog_ = null;

  /**
   * Shutter sound player.
   * @type {Audio}
   * @private
   */
  this.shutterSound_ = document.createElement('audio');

  /**
   * Tick sound player.
   * @type {Audio}
   * @private
   */
  this.tickSound_ = document.createElement('audio');

  /**
   * Canvas element with the current frame downsampled to small resolution, to
   * be used in effect preview windows.
   *
   * @type {Canvas}
   * @private
   */
  this.previewInputCanvas_ = document.createElement('canvas');

  /**
   * Canvas element with the current frame downsampled to small resolution, to
   * be used by the head tracker.
   *
   * @type {Canvas}
   * @private
   */
  this.trackerInputCanvas_ = document.createElement('canvas');

  /**
   * @type {boolean}
   * @private
   */
  this.running_ = false;

  /**
   * @type {boolean}
   * @private
   */
  this.capturing_ = false;

  /**
   * The main (full screen) canvas for full quality capture.
   * @type {fx.Canvas}
   * @private
   */
  this.mainCanvas_ = null;

  /**
   * Texture for the full quality canvas.
   * @type {fx.Texture}
   * @private
   */
  this.mainCanvasTexture_ = null;

  /**
   * The main (full screen canvas) for fast capture.
   * @type {fx.Canvas}
   * @private
   */
  this.mainFastCanvas_ = null;

  /**
   * Texture for the fast canvas.
   * @type {fx.Texture}
   * @private
   */
  this.mainFastCanvasTexture_ = null;

  /**
   * Shared fx canvas for effects' previews.
   * @type {fx.Canvas}
   * @private
   */
  this.previewCanvas_ = null;

  /**
   * Texture for the effects' canvas.
   * @type {fx.Texture}
   * @private
   */
  this.previewCanvasTexture_ = null;

  /**
   * The main (full screen) processor in the full quality mode.
   * @type {camera.Processor}
   * @private
   */
  this.mainProcessor_ = null;

  /**
   * The main (full screen) processor in the fast mode.
   * @type {camera.Processor}
   * @private
   */
  this.mainFastProcessor_ = null;

  /**
   * Processors for effect previews.
   * @type {Array.<camera.Processor>}
   * @private
   */
  this.previewProcessors_ = [];

  /**
   * Selected effect or null if no effect.
   * @type {number}
   * @private
   */
  this.currentEffectIndex_ = 0;

  /**
   * Face detector and tracker.
   * @type {camera.Tracker}
   * @private
   */
  this.tracker_ = new camera.Tracker(this.trackerInputCanvas_);

  /**
   * Current frame.
   * @type {number}
   * @private
   */
  this.frame_ = 0;

  /**
   * If the toolbar is expanded.
   * @type {boolean}
   * @private
   */
  this.expanded_ = false;

  /**
   * If the window controls are visible.
   * @type {boolean}
   * @private
   */
  this.controlsVisible_ = true;

  /**
   * Toolbar animation effect wrapper.
   * @type {camera.util.StyleEffect}
   * @private
   */
  this.toolbarEffect_ = new camera.util.StyleEffect(
      function(args, callback) {
        var toolbar = document.querySelector('#toolbar');
        var activeEffect = document.querySelector('#effects #effect-' +
            this.currentEffectIndex_);
        if (args) {
          toolbar.classList.add('expanded');
        } else {
          toolbar.classList.remove('expanded');
          // Make all of the effects non-focusable.
          var elements = document.querySelectorAll('#effects li');
          for (var index = 0; index < elements.length; index++) {
            elements[index].tabIndex = -1;
          }
          // If something was focused before, then focus the toggle button.
          if (document.activeElement != document.body)
            document.querySelector('#filters-toggle').focus();
        }
        camera.util.waitForTransitionCompletion(
            document.querySelector('#toolbar'), 500, function() {
          // If the ribbon is opened, then make all of the items focusable.
          if (args) {
            activeEffect.tabIndex = 0;  // In the focusing order.

            // If the filters button was previously selected, then advance to
            // the ribbon.
            if (document.activeElement ==
                document.querySelector('#filters-toggle')) {
              activeEffect.focus();
            }
          }
          callback();
        });
      }.bind(this));

  /**
   * Timer for hiding the toast message after some delay.
   * @type {number?}
   * @private
   */
  this.toastHideTimer_ = null;

  /**
   * Toast transition wrapper. Shows or hides the toast with the passed message.
   * @type {camera.util.StyleEffect}
   * @private
   */
  this.toastEffect_ = new camera.util.StyleEffect(
      function(args, callback) {
        var toastElement = document.querySelector('#toast');
        var toastMessageElement = document.querySelector('#toast-message');
        // Hide the message if visible.
        if (!args.visible && toastElement.classList.contains('visible')) {
          toastElement.classList.remove('visible');
          camera.util.waitForTransitionCompletion(
              toastElement, 500, callback);
        } else if (args.visible) {
          // If showing requested, then show.
          toastMessageElement.textContent = args.message;
          toastElement.classList.add('visible');
          camera.util.waitForTransitionCompletion(
             toastElement, 500, callback);
        } else {
          callback();
        }
      }.bind(this));

  /**
   * Window controls animation effect wrapper.
   * @type {camera.util.StyleEffect}
   * @private
   */
  this.controlsEffect_ = new camera.util.StyleEffect(
      function(args, callback) {
        if (args)
          document.body.classList.add('controls-visible');
        else
          document.body.classList.remove('controls-visible');
        camera.util.waitForTransitionCompletion(
            document.body, 500, callback);
      });

  /**
   * Whether a picture is being taken. Used to decrease video quality of
   * previews for smoother response.
   * @type {boolean}
   * @private
   */
  this.taking_ = false;

  /**
   * Timer used to automatically collapse the tools.
   * @type {?number}
   * @private
   */
  this.collapseTimer_ = null;

  /**
   * Set to true before the ribbon is displayed. Used to render the ribbon's
   * frames while it is not yet displayed, so the previews have some image
   * as soon as possible.
   * @type {boolean}
   * @private
   */
  this.ribbonInitialization_ = true;

  /**
   * Scroller for the ribbon with effects.
   * @type {camera.util.SmoothScroller}
   * @private
   */
  this.scroller_ = new camera.util.SmoothScroller(
      document.querySelector('#effects'),
      document.querySelector('#effects .padder'));

  /**
   * Scroll bar for the ribbon with effects.
   * @type {camera.HorizontalScrollBar}
   * @private
   */
  this.scrollBar_ = new camera.HorizontalScrollBar(this.scroller_);

  /**
   * Detects if the mouse has been moved or clicked, and if any touch events
   * have been performed on the view. If such events are detected, then the
   * ribbon and the window buttons are shown.
   *
   * @type {camera.util.PointerTracker}
   * @private
   */
  this.pointerTracker_ = new camera.util.PointerTracker(
      document.body, this.onPointerActivity_.bind(this));

  /**
   * Enables scrolling the ribbon by dragging the mouse.
   * @type {camera.util.MouseScroller}
   * @private
   */
  this.mouseScroller_ = new camera.util.MouseScroller(this.scroller_);

  /**
   * Detects whether scrolling is being performed or not.
   * @type {camera.util.ScrollTracker}
   * @private
   */
  this.scrollTracker_ = new camera.util.ScrollTracker(
      this.scroller_, function() {}, this.onScrollEnded_.bind(this));

  /**
   * @type {string}
   * @private
   */
  this.keyBuffer_ = '';

  /**
   * Measures frames per seconds.
   * @type {camera.util.PerformanceMonitor}
   * @private
   */
  this.performanceMonitor_ = new camera.util.PerformanceMonitor();

  /**
   * Makes the toolbar pullable.
   * @type {camera.util.Puller}
   * @private
   */
  this.puller_ = new camera.util.Puller(
      document.querySelector('#toolbar-puller-wrapper'),
      document.querySelector('#toolbar-stripe'),
      this.onRibbonPullReleased_.bind(this));

  /**
   * Counter used to refresh periodically invisible images on the ribbons, to
   * avoid displaying stale ones.
   * @type {number}
   * @private
   */
  this.staleEffectsRefreshIndex_ = 0;

  /**
   * Timer used to countdown before taking the picture.
   * @type {number?}
   * @private
   */
  this.takePictureTimer_ = null;

  /**
   * Used by the performance test to progress to a next step. If not null, then
   * the performance test is in progress.
   * @type {number?}
   * @private
   */
  this.performanceTestTimer_ = null;

  /**
   * Stores results of the performance test.
   * @type {Array.<Object>}
   * @private
   */
  this.performanceTestResults_ = [];

  /**
   * Used by the performance test to periodically update the UI.
   * @type {number?}
   * @private
   */
  this.performanceTestUIInterval_ = null;

  // End of properties, seal the object.
  Object.seal(this);

  // Synchronize bounds of the video now, when window is resized or if the
  // video dimensions are loaded.
  this.video_.addEventListener('loadedmetadata',
      this.synchronizeBounds_.bind(this));
  this.synchronizeBounds_();

  // Sets dimensions of the input canvas for the effects' preview on the ribbon.
  // Keep in sync with CSS.
  this.previewInputCanvas_.width = 80;
  this.previewInputCanvas_.height = 80;

  // Handle the 'Take' button.
  document.querySelector('#take-picture').addEventListener(
      'click', this.onTakePictureClicked_.bind(this));

  document.querySelector('#toolbar #album-enter').addEventListener('click',
      this.onAlbumEnterClicked_.bind(this));

  document.querySelector('#toolbar #filters-toggle').addEventListener('click',
      this.onFiltersToggleClicked_.bind(this));

  // Hide window controls when moving outside of the window.
  window.addEventListener('mouseout', this.onWindowMouseOut_.bind(this));

  // Hide window controls when any key pressed.
  // TODO(mtomasz): Move managing window controls to main.js.
  window.addEventListener('keydown', this.onWindowKeyDown_.bind(this));

  document.querySelector('#toggle-timer').addEventListener(
      'keypress', this.onToggleTimerKeyPress_.bind(this));
  document.querySelector('#toggle-timer').addEventListener(
      'click', this.onToggleTimerClicked_.bind(this));
  document.querySelector('#toggle-multi').addEventListener(
      'keypress', this.onToggleMultiKeyPress_.bind(this));
  document.querySelector('#toggle-multi').addEventListener(
      'click', this.onToggleMultiClicked_.bind(this));

  // Load the shutter and the tick sound.
  this.shutterSound_.src = '../sounds/shutter.ogg';
  this.tickSound_.src = '../sounds/tick.ogg';
};

/**
 * Frame draw mode.
 * @enum {number}
 */
camera.views.Camera.DrawMode = Object.freeze({
  OPTIMIZED: 0,  // Quality optimized for performance.
  FULL: 1  // The best quality possible.
});

/**
 * Head tracker quality.
 * @enum {number}
 */
camera.views.Camera.HeadTrackerQuality = Object.freeze({
  LOW: 0,    // Very low resolution, used for the effects' previews.
  NORMAL: 1  // Default resolution, still low though.
});

/**
 * Number of frames to be skipped between optimized effects' ribbon refreshes
 * and the head detection invocations (which both use the preview back buffer).
 *
 * @type {number}
 * @const
 */
camera.views.Camera.PREVIEW_BUFFER_SKIP_FRAMES = 3;

/**
 * Number of frames to be skipped between the head tracker invocations when
 * the head tracker is used for the ribbon only.
 *
 * @type {number}
 * @const
 */
camera.views.Camera.RIBBON_HEAD_TRACKER_SKIP_FRAMES = 30;

camera.views.Camera.prototype = {
  __proto__: camera.View.prototype,
  get running() {
    return this.running_;
  },
  get capturing() {
    return this.capturing_;
  }
};

/**
 * Initializes the view.
 * @override
 */
camera.views.Camera.prototype.initialize = function(callback) {
  // Initialize the webgl canvases.
  try {
    this.mainCanvas_ = fx.canvas();
    this.mainFastCanvas_ = fx.canvas();
    this.previewCanvas_ = fx.canvas();
  }
  catch (e) {
    // TODO(mtomasz): Replace with a better icon.
    this.context_.onError('no-camera',
        chrome.i18n.getMessage('errorMsgNoWebGL'),
        chrome.i18n.getMessage('errorMsgNoWebGLHint'));

    // Initialization failed due to lack of webgl.
    document.body.classList.remove('initializing');
  }

  if (this.mainCanvas_ && this.mainFastCanvas_) {
    // Initialize the processors.
    this.mainCanvasTexture_ = this.mainCanvas_.texture(this.video_);
    this.mainFastCanvasTexture_ = this.mainFastCanvas_.texture(this.video_);
    this.mainProcessor_ = new camera.Processor(
        this.tracker_,
        this.mainCanvasTexture_,
        this.mainCanvas_,
        this.mainCanvas_);
    this.mainFastProcessor_ = new camera.Processor(
        this.tracker_,
        this.mainFastCanvasTexture_,
        this.mainFastCanvas_,
        this.mainFastCanvas_,
        camera.Processor.Mode.FAST);

    // Insert the main canvas to its container.
    document.querySelector('#main-canvas-wrapper').appendChild(
        this.mainCanvas_);
    document.querySelector('#main-fast-canvas-wrapper').appendChild(
        this.mainFastCanvas_);

    // Set the default effect.
    this.mainProcessor_.effect = new camera.effects.Normal();

    // Prepare effect previews.
    this.previewCanvasTexture_ = this.previewCanvas_.texture(
        this.previewInputCanvas_);
    this.addEffect_(new camera.effects.Normal());
    this.addEffect_(new camera.effects.Vintage());
    this.addEffect_(new camera.effects.Cinema());
    this.addEffect_(new camera.effects.TiltShift());
    this.addEffect_(new camera.effects.Retro30());
    this.addEffect_(new camera.effects.Retro50());
    this.addEffect_(new camera.effects.Retro60());
    this.addEffect_(new camera.effects.PhotoLab());
    this.addEffect_(new camera.effects.BigHead());
    this.addEffect_(new camera.effects.BigJaw());
    this.addEffect_(new camera.effects.BigEyes());
    this.addEffect_(new camera.effects.BunnyHead());
    this.addEffect_(new camera.effects.Grayscale());
    this.addEffect_(new camera.effects.Sepia());
    this.addEffect_(new camera.effects.Colorize());
    this.addEffect_(new camera.effects.Modern());
    this.addEffect_(new camera.effects.Beauty());
    this.addEffect_(new camera.effects.Newspaper());
    this.addEffect_(new camera.effects.Funky());
    this.addEffect_(new camera.effects.Ghost());
    this.addEffect_(new camera.effects.Swirl());

    // Select the default effect and state of the timer toggle button.
    // TODO(mtomasz): Move to chrome.storage.local.sync, after implementing
    // syncing of the gallery.
    chrome.storage.local.get(
        {
          effectIndex: 0,
          toggleTimer: true,
          toggleMulti: false
        },
        function(values) {
          if (values.effectIndex < this.previewProcessors_.length)
            this.setCurrentEffect_(values.effectIndex);
          else
            this.setCurrentEffect_(0);
          document.querySelector('#toggle-timer').checked = values.toggleTimer;
          document.querySelector('#toggle-multi').checked = values.toggleMulti;
        }.bind(this));
  }

  // Acquire the gallery model.
  camera.models.Gallery.getInstance(function(model) {
    this.model_ = model;
    callback();
  }.bind(this), function() {
    // TODO(mtomasz): Add error handling.
    console.error('Unable to initialize the file system.');
    callback();
  });
};

/**
 * Enters the view.
 * @override
 */
camera.views.Camera.prototype.onEnter = function() {
  if (!this.running_ && this.mainCanvas_ && this.mainFastCanvas_)
    this.start_();
  this.performanceMonitor_.start();
  this.tracker_.start();
  this.onResize();
};

/**
 * Leaves the view.
 * @override
 */
camera.views.Camera.prototype.onLeave = function() {
  this.performanceMonitor_.stop();
  this.tracker_.stop();
};

/**
 * @override
 */
camera.views.Camera.prototype.onActivate = function() {
  this.scrollTracker_.start();
  if (document.activeElement != document.body)
    document.querySelector('#take-picture').focus();
};

/**
 * @override
 */
camera.views.Camera.prototype.onInactivate = function() {
  this.scrollTracker_.stop();
};

/**
 * @override
 */
camera.views.Camera.prototype.onInactivate = function() {
  this.resetTakePicture_();
};

/**
 * Handles clicking on the take-picture button.
 * @param {Event} event Mouse event
 * @private
 */
camera.views.Camera.prototype.onTakePictureClicked_ = function(event) {
  if (this.performanceTestTimer_)
    return;
  this.takePicture_();
};

/**
 * Handles clicking on the album button.
 * @param {Event} event Mouse event
 * @private
 */
camera.views.Camera.prototype.onAlbumEnterClicked_ = function(event) {
  if (this.performanceTestTimer_)
    return;
  this.router.navigate(camera.Router.ViewIdentifier.ALBUM);
};

/**
 * Handles clicking on the toggle filters button.
 * @param {Event} event Mouse event
 * @private
 */
camera.views.Camera.prototype.onFiltersToggleClicked_ = function(event) {
  if (this.performanceTestTimer_)
    return;
  this.setExpanded_(!this.expanded_);
};

/**
 * Handles releasing the puller on the ribbon, and toggles it.
 * @param {number} distance Pulled distance in pixels.
 * @private
 */
camera.views.Camera.prototype.onRibbonPullReleased_ = function(distance) {
  if (this.performanceTestTimer_)
    return;
  if (distance < -50)
    this.setExpanded_(!this.expanded_);
  else if (distance > 25)
    this.setExpanded_(false);
};

/**
 * Handles moving the mouse outside of the window.
 * @param {Event} event Mouse event
 * @private
 */
camera.views.Camera.prototype.onWindowMouseOut_ = function(event) {
  if (this.performanceTestTimer_)
    return;
  if (event.toElement !== null)
    return;

  this.setControlsVisible_(false, 1000);
};

/**
 * Handles pressing a key within a window.
 * TODO(mtomasz): Simplify this logic.
 *
 * @param {Event} event Key down event
 * @private
 */
camera.views.Camera.prototype.onWindowKeyDown_ = function(event) {
  if (this.performanceTestTimer_)
    return;
  // When the ribbon is focused, then do not collapse it when pressing keys.
  if (document.activeElement == document.querySelector('#effects-wrapper')) {
    this.setExpanded_(true);
    this.setControlsVisible_(true);
    return;
  }

  // If anything else is focused, then hide controls when navigation keys
  // are pressed (or space).
   switch (camera.util.getShortcutIdentifier(event)) {
    case 'Right':
    case 'Left':
    case 'Up':
    case 'Down':
    case 'U+0020':  // Space.
    case 'Home':
    case 'End':
      this.setControlsVisible_(false);
    default:
      this.setControlsVisible_(true);
  }
};

/**
 * Handles pressing a key on the timer switch.
 * @param {Event} event Key press event.
 * @private
 */
camera.views.Camera.prototype.onToggleTimerKeyPress_ = function(event) {
  if (this.performanceTestTimer_)
    return;
  if (camera.util.getShortcutIdentifier(event) == 'Enter')
    document.querySelector('#toggle-timer').click();
};

/**
 * Handles pressing a key on the multi-shot switch.
 * @param {Event} event Key press event.
 * @private
 */
camera.views.Camera.prototype.onToggleMultiKeyPress_ = function(event) {
  if (this.performanceTestTimer_)
    return;
  if (camera.util.getShortcutIdentifier(event) == 'Enter')
    document.querySelector('#toggle-multi').click();
};

/**
 * Handles clicking on the timer switch.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Camera.prototype.onToggleTimerClicked_ = function(event) {
  if (this.performanceTestTimer_)
    return;
  var enabled = document.querySelector('#toggle-timer').checked;
  this.showToastMessage_(
      chrome.i18n.getMessage(enabled ? 'toggleTimerActiveMessage' :
                                       'toggleTimerInactiveMessage'));
  chrome.storage.local.set({toggleTimer: enabled});
};

/**
 * Handles clicking on the multi-shot switch.
 * @param {Event} event Click event.
 * @private
 */
camera.views.Camera.prototype.onToggleMultiClicked_ = function(event) {
  if (this.performanceTestTimer_)
    return;
  var enabled = document.querySelector('#toggle-multi').checked;
  this.showToastMessage_(
      chrome.i18n.getMessage(enabled ? 'toggleMultiActiveMessage' :
                                       'toggleMultiInactiveMessage'));
  chrome.storage.local.set({toggleMulti: enabled});
};

/**
 * Handles pointer actions, such as mouse or touch activity.
 * @param {Event} event Activity event.
 * @private
 */
camera.views.Camera.prototype.onPointerActivity_ = function(event) {
  if (this.performanceTestTimer_)
    return;
  // Show the window controls.
  this.setControlsVisible_(true);

  // Update the ribbon's visibility.
  switch (event.type) {
    case 'mousedown':
      // Toggle the ribbon if clicking on static area.
      if (event.target == document.body ||
          document.querySelector('#main-canvas-wrapper').contains(
              event.target) ||
          document.querySelector('#main-fast-canvas-wrapper').contains(
              event.target)) {
        this.setExpanded_(!this.expanded_);
        break;
      }  // Otherwise continue.
    default:
      // Prevent auto-hiding the ribbon for any other activity.
      if (this.expanded_)
        this.setExpanded_(true);
      break;
  }
};

/**
 * Handles end of scroll on the ribbon with effects.
 * @private
 */
camera.views.Camera.prototype.onScrollEnded_ = function() {
  if (document.activeElement != document.body && this.expanded_) {
    var effect = document.querySelector('#effects #effect-' +
        this.currentEffectIndex_);
    effect.focus();
  }
};

/**
 * Adds an effect to the user interface.
 * @param {camera.Effect} effect Effect to be added.
 * @private
 */
camera.views.Camera.prototype.addEffect_ = function(effect) {
  // Create the preview on the ribbon.
  var listPadder = document.querySelector('#effects .padder');
  var wrapper = document.createElement('div');
  wrapper.className = 'preview-canvas-wrapper';
  var canvas = document.createElement('canvas');
  canvas.width = 257;  // Forces acceleration on the canvas.
  canvas.height = 257;
  var padder = document.createElement('div');
  padder.className = 'preview-canvas-padder';
  padder.appendChild(canvas);
  wrapper.appendChild(padder);
  var item = document.createElement('li');
  item.appendChild(wrapper);
  listPadder.appendChild(item);
  var label = document.createElement('span');
  label.textContent = effect.getTitle();
  item.appendChild(label);

  // Calculate the effect index.
  var effectIndex = this.previewProcessors_.length;
  item.id = 'effect-' + effectIndex;

  // Set aria attributes.
  item.setAttribute('i18n-aria-label', effect.getTitle());
  item.setAttribute('aria-role', 'option');
  item.setAttribute('aria-selected', 'false');
  item.tabIndex = -1;

  // Assign events.
  item.addEventListener('click', function() {
    if (this.currentEffectIndex_ == effectIndex)
      this.previewProcessors_[effectIndex].effect.randomize();
    this.setCurrentEffect_(effectIndex);
  }.bind(this));
  item.addEventListener('focus',
      this.setCurrentEffect_.bind(this, effectIndex));

  // Create the preview processor.
  var processor = new camera.Processor(
      this.tracker_,
      this.previewCanvasTexture_,
      canvas,
      this.previewCanvas_);
  processor.effect = effect;
  this.previewProcessors_.push(processor);
};

/**
 * Sets the current effect.
 * @param {number} effectIndex Effect index.
 * @private
 */
camera.views.Camera.prototype.setCurrentEffect_ = function(effectIndex) {
  var previousEffect =
      document.querySelector('#effects #effect-' + this.currentEffectIndex_);
  previousEffect.removeAttribute('selected');
  previousEffect.setAttribute('aria-selected', 'false');

  if (this.expanded_)
    previousEffect.tabIndex = -1;

  var effect = document.querySelector('#effects #effect-' + effectIndex);
  effect.setAttribute('selected', '');
  effect.setAttribute('aria-selected', 'true');
  if (this.expanded_)
    effect.tabIndex = 0;
  camera.util.ensureVisible(effect, this.scroller_);

  // If there was something focused before, then synchronize the focus.
  if (this.expanded_ && document.activeElement != document.body) {
    // If not scrolling, then focus immediately. Otherwise, the element will
    // be focused, when the scrolling is finished in onScrollEnded.
    if (!this.scrollTracker_.scrolling && !this.scroller_.animating)
      effect.focus();
  }

  this.mainProcessor_.effect = this.previewProcessors_[effectIndex].effect;
  this.mainFastProcessor_.effect = this.previewProcessors_[effectIndex].effect;

  var listWrapper = document.querySelector('#effects-wrapper');
  listWrapper.setAttribute('aria-activedescendant', effect.id);
  listWrapper.setAttribute('aria-labelledby', effect.id);
  this.currentEffectIndex_ = effectIndex;

  // Show the ribbon when switching effects.
  if (!this.performanceTestTimer_)
    this.setExpanded_(true);

  // TODO(mtomasz): This is a little racy, since setting may be run in parallel,
  // without guarantee which one will be written as the last one.
  chrome.storage.local.set({effectIndex: effectIndex});
};

/**
 * @override
 */
camera.views.Camera.prototype.onResize = function() {
  this.synchronizeBounds_();
  camera.util.ensureVisible(
      document.querySelector('#effect-' + this.currentEffectIndex_),
      this.scroller_);
};

/**
 * @override
 */
camera.views.Camera.prototype.onKeyPressed = function(event) {
  if (this.performanceTestTimer_)
    return;
  this.keyBuffer_ += String.fromCharCode(event.which);
  this.keyBuffer_ = this.keyBuffer_.substr(-10);

  // Allow to load a file stream (for debugging).
  if (this.keyBuffer_.indexOf('CRAZYPONY') !== -1) {
    this.chooseFileStream_();
    this.keyBuffer_ = '';
  }

  if (this.keyBuffer_.indexOf('VER') !== -1) {
    this.showVersion_();
    this.keyBuffer_ = '';
  }

  if (this.keyBuffer_.indexOf('CHOCOBUNNY') !== -1) {
    this.startPerformanceTest_();
    this.keyBuffer_ = '';
  }

  switch (camera.util.getShortcutIdentifier(event)) {
    case 'Left':
      this.setCurrentEffect_(
          (this.currentEffectIndex_ + this.previewProcessors_.length - 1) %
              this.previewProcessors_.length);
      event.preventDefault();
      break;
    case 'Right':
      this.setCurrentEffect_(
          (this.currentEffectIndex_ + 1) % this.previewProcessors_.length);
      event.preventDefault();
      break;
    case 'Home':
      this.setCurrentEffect_(0);
      event.preventDefault();
      break;
    case 'End':
      this.setCurrentEffect_(this.previewProcessors_.length - 1);
      event.preventDefault();
      break;
    case 'U+0020':  // Space key for taking the picture.
      document.querySelector('#take-picture').click();
      event.stopPropagation();
      event.preventDefault();
      break;
    case 'U+0047':  // G key for the gallery.
      this.router.navigate(camera.Router.ViewIdentifier.ALBUM);
      event.preventDefault();
      break;
  }
};

/**
 * Shows a non-intrusive toast message in the middle of the screen.
 * @param {string} message Message to be shown.
 * @private
 */
camera.views.Camera.prototype.showToastMessage_ = function(message) {
  var cancelHideTimer = function() {
    if (this.toastHideTimer_) {
      clearTimeout(this.toastHideTimer_);
      this.toastHideTimer_ = null;
    }
  }.bind(this);

  // If running, then reinvoke recursively after closing the toast message.
  if (this.toastEffect_.animating || this.toastHideTimer_) {
    cancelHideTimer();
    this.toastEffect_.invoke({
      visible: false
    }, this.showToastMessage_.bind(this, message));
    return;
  }

  // Cancel any pending hide timers.
  cancelHideTimer();

  // Start the hide timer.
  this.toastHideTimer_ = setTimeout(function() {
    this.toastEffect_.invoke({
      visible: false
    }, function() {});
    this.toastHideTimer_ = null;
  }.bind(this), 2000);

  // Show the toast message.
  this.toastEffect_.invoke({
    visible: true,
    message: message
  }, function() {});
};

/**
 * Toggles the toolbar visibility. However, it may delay the operation, if
 * eg. some UI element is hovered.
 *
 * @param {boolean} expanded True to show the toolbar, false to hide.
 * @private
 */
camera.views.Camera.prototype.setExpanded_ = function(expanded) {
  if (this.collapseTimer_) {
    clearTimeout(this.collapseTimer_);
    this.collapseTimer_ = null;
  }
  if (expanded) {
    var isRibbonHovered =
        document.querySelector('#toolbar').webkitMatchesSelector(':hover');
    if (!isRibbonHovered && !this.performanceTestTimer_) {
      this.collapseTimer_ = setTimeout(
          this.setExpanded_.bind(this, false), 3000);
    }
    if (!this.expanded_) {
      this.toolbarEffect_.invoke(true, function() {
        this.expanded_ = true;
      }.bind(this));
    }
  } else {
    if (this.expanded_) {
      this.expanded_ = false;
      this.toolbarEffect_.invoke(false, function() {});
    }
  }
};
/**
 * Toggles the window controls visibility.
 *
 * @param {boolean} visible True to show the controls, false to hide.
 * @param {number=} opt_delay Optional delay before toggling.
 * @private
 */
camera.views.Camera.prototype.setControlsVisible_ = function(
    visible, opt_delay) {
  if (this.controlsVisible_ == visible)
    return;

  this.controlsEffect_.invoke(visible, function() {}, opt_delay);

  // Set the visibility property as soon as possible, to avoid races, when
  // showing, and hiding one after each other.
  this.controlsVisible_ = visible;
};

/**
 * Chooses a file stream to override the camera stream. Used for debugging.
 * @private
 */
camera.views.Camera.prototype.chooseFileStream_ = function() {
  chrome.fileSystem.chooseEntry(function(fileEntry) {
    if (!fileEntry)
      return;
    fileEntry.file(function(file) {
      var url = URL.createObjectURL(file);
      this.video_.src = url;
      this.video_.play();
    }.bind(this));
  }.bind(this));
};

/**
 * Shows a version dialog.
 * @private
 */
camera.views.Camera.prototype.showVersion_ = function() {
  // No need to localize, since for debugging purpose only.
  var message = 'Version: ' + chrome.runtime.getManifest().version + '\n' +
      'Resolution: ' + this.video_.videoWidth + 'x' + this.video_.videoHeight +
          '\n' +
      'Frames per second: ' + this.performanceMonitor_.fps.toPrecision(2) +
          '\n' +
      'Head tracking frames per second: ' + this.tracker_.fps.toPrecision(2);
  this.router.navigate(camera.Router.ViewIdentifier.DIALOG, {
    type: camera.views.Dialog.Type.ALERT,
    message: message
  });
};

/**
 * Starts a performance test.
 * @private
 */
camera.views.Camera.prototype.startPerformanceTest_ = function() {
  if (this.performanceTestTimer_)
    return;

  this.performanceTestResults_ = [];

  // Start the test after resizing to desired dimensions.
  var onBoundsChanged = function() {
    document.body.classList.add('performance-test');
    this.progressPerformanceTest_(0);
    var perfTestBubble = document.querySelector('#perf-test-bubble');
    this.performanceTestUIInterval_ = setInterval(function() {
      var fps = this.performanceMonitor_.fps;
      var scale = 1 + Math.min(fps / 60, 1);
      // (10..30) -> (0..30)
      var hue = 120 * Math.max(0, Math.min(fps, 30) * 40 / 30 - 10) / 30;
      perfTestBubble.textContent = Math.round(fps);
      perfTestBubble.style.backgroundColor =
          'hsla(' + hue + ', 100%, 75%, 0.75)';
      perfTestBubble.style.webkitTransform = 'scale(' + scale + ')';
    }.bind(this), 100);
    // Removing listener will be ignored if not registered earlier.
    chrome.app.window.current().onBoundsChanged.removeListener(onBoundsChanged);
  }.bind(this);

   // Set the default window size and wait until it is applied.
  var onRestored = function() {
    if (this.setDefaultGeometry_())
      chrome.app.window.current().onBoundsChanged.addListener(onBoundsChanged);
    else
      onBoundsChanged();
    chrome.app.window.current().onRestored.removeListener(onRestored);
  }.bind(this);

  // If maximized, then restore before proceeding. The timer has to be used, to
  // know that the performance test has started.
  // TODO(mtomasz): Consider using a bool member instead of reusing timer.
  this.performanceTestTimer_ = setTimeout(function() {
    if (chrome.app.window.current().isMaximized()) {
      chrome.app.window.current().restore();
      chrome.app.window.current().onRestored.addListener(onRestored);
    } else {
      onRestored();
    }
  }, 0);
};

/**
 * Progresses to the next step of the performance test.
 * @param {number} index Step index to progress to.
 * @private
 */
camera.views.Camera.prototype.progressPerformanceTest_ = function(index) {
  // Finalize the previous test.
  if (index) {
    var result = {
      effect: Math.floor(index - 1 / 2),
      ribbon: (index - 1) % 2,
      // TODO(mtomasz): Avoid localization. Use English instead.
      name: this.mainProcessor_.effect.getTitle(),
      fps: this.performanceMonitor_.fps
    };
    this.performanceTestResults_.push(result);
  }

  // Check if the end.
  if (index == this.previewProcessors_.length * 2) {
    this.stopPerformanceTest_();
    var message = '';
    var score = 0;
    this.performanceTestResults_.forEach(function(result) {
      message += [
        result.effect,
        result.ribbon,
        result.name,
        Math.round(result.fps)
      ].join(', ') + '\n';
      score += result.fps / this.performanceTestResults_.length;
    }.bind(this));
    var header = 'Score: ' + Math.round(score * 100) + '\n';
    this.router.navigate(camera.Router.ViewIdentifier.DIALOG, {
      type: camera.views.Dialog.Type.ALERT,
      message: header + message
    });
    return;
  }

  // Run new test.
  this.performanceMonitor_.stop();
  this.performanceMonitor_.start();
  this.setCurrentEffect_(Math.floor(index / 2));
  this.setExpanded_(index % 2 == 1);

  // Update the progress bar.
  var progress = (index / (this.previewProcessors_.length * 2)) * 100;
  var perfTestBar = document.querySelector('#perf-test-bar');
  perfTestBar.textContent = Math.round(progress) + '%';
  perfTestBar.style.width = progress + '%';

  // Schedule the next test.
  this.performanceTestTimer_ = setTimeout(function() {
    this.progressPerformanceTest_(index + 1);
  }.bind(this), 5000);
};

/**
 * Stops the performance test.
 * @private
 */
camera.views.Camera.prototype.stopPerformanceTest_ = function() {
  if (!this.performanceTestTimer_)
    return;
  clearTimeout(this.performanceTestTimer_);
  this.performanceTestTimer_ = null;
  clearInterval(this.performanceTestUIInterval_);
  this.performanceTestUIInterval_ = null;
  this.showToastMessage_('Performance test terminated');
  document.body.classList.remove('performance-test');
};

/**
 * Takes the picture with a timer if enabled, otherwise immediately.
 * @private
 */
camera.views.Camera.prototype.takePicture_ = function() {
  if (!this.running_)
    return;

  var toggleTimer = document.querySelector('#toggle-timer');
  var toggleMulti = document.querySelector('#toggle-multi');

  var timerEnabled = toggleTimer.checked;
  var multiEnabled = toggleMulti.checked;

  toggleTimer.disabled = true;
  toggleMulti.disabled = true;
  document.querySelector('#take-picture').disabled = true;

  var tickCounter = timerEnabled ? 6 : 1;
  var multiShotCounter = multiEnabled ? 3 : 1;
  var onTimerTick = function() {
    tickCounter--;
    if (tickCounter == 0) {
      var takePicture = function() {
        // If in timer mode, but the timer got cancelled, then do not continue.
        if (!this.takePictureTimer_)
          return;
        this.takePictureImmediately_(function() {
          multiShotCounter--;
          if (!multiShotCounter) {
            // Reset the controls.
            this.resetTakePicture_();
          } else {
            setTimeout(takePicture, 250);
          }
        }.bind(this));
      }.bind(this);
      takePicture();
    } else {
      this.takePictureTimer_ = setTimeout(onTimerTick, 1000);
      this.tickSound_.play();
      // Blink the toggle timer button.
      toggleTimer.classList.add('animate');
      setTimeout(function() {
        if (this.takePictureTimer_)
          toggleTimer.classList.remove('animate');
      }.bind(this), 500);
    }
  }.bind(this);

  // First tick immediately in the next message loop cycle.
  this.takePictureTimer_ = setTimeout(onTimerTick, 0);
};

/**
 * Resets scheduled picture takes (if any).
 * @private
 */
camera.views.Camera.prototype.resetTakePicture_ = function() {
  if (this.takePictureTimer_) {
    clearTimeout(this.takePictureTimer_);
    this.takePictureTimer_ = null;
  }
  var toggleTimer = document.querySelector('#toggle-timer');
  toggleTimer.classList.remove('animate');
  toggleTimer.disabled = false;
  document.querySelector('#take-picture').disabled = false;
  document.querySelector('#toggle-multi').disabled = false;
};

/**
 * Takes the picture immediately, and saves and puts to the album with a nice
 * animation.
 *
 * @param {function()=} opt_callback Completion callback.
 * @private
 */
camera.views.Camera.prototype.takePictureImmediately_ = function(opt_callback) {
  if (!this.running_) {
    if (opt_callback)
      opt_callback();
    return;
  }

  // Lock refreshing for smoother experience.
  this.taking_ = true;

  // Show flashing animation with the shutter sound.
  document.body.classList.add('show-shutter');
  var albumButton = document.querySelector('#toolbar #album-enter');
  camera.util.setAnimationClass(albumButton, albumButton, 'flash');
  setTimeout(function() {
    document.body.classList.remove('show-shutter');
  }.bind(this), 200);

  this.shutterSound_.currentTime = 0;
  this.shutterSound_.play();

  setTimeout(function() {
    this.mainCanvasTexture_.loadContentsOf(this.video_);
    this.mainProcessor_.processFrame();
    var dataURL = this.mainCanvas_.toDataURL('image/jpeg');

    // Create a picture preview animation.
    var picturePreview = document.querySelector('#picture-preview');
    var img = document.createElement('img');
    img.src = dataURL;
    img.style.webkitTransform = 'rotate(' + (Math.random() * 40 - 20) + 'deg)';
    img.addEventListener('click', function() {
      // For simplicity, always navigate to the newest picture.
      if (this.model_.length) {
        this.model_.currentIndex = this.model_.length - 1;
        this.router.navigate(camera.Router.ViewIdentifier.BROWSER);
      }
    }.bind(this));

    // Create the fly-away animation after two second.
    setTimeout(function() {
      img.classList.remove('activated');

      var sourceRect = img.getBoundingClientRect();
      var targetRect = albumButton.getBoundingClientRect();

      // If the album button is hidden, then we can't get its geometry.
      if (targetRect.width && targetRect.height) {
        var translateXValue = (targetRect.left + targetRect.right) / 2 -
            (sourceRect.left + sourceRect.right) / 2;
        var translateYValue = (targetRect.top + targetRect.bottom) / 2 -
            (sourceRect.top + sourceRect.bottom) / 2;
        var scaleValue = targetRect.width / sourceRect.width;

        img.style.webkitTransform =
            'rotate(0) translateX(' + translateXValue +'px) ' +
            'translateY(' + translateYValue + 'px) ' +
            'scale(' + scaleValue + ')';
      }
      img.style.opacity = 0;

      camera.util.waitForTransitionCompletion(img, 1500, function() {
        img.parentNode.removeChild(img);
        this.taking_ = false;
      }.bind(this));
    }.bind(this), 2000);

    var onPointerDown = function() {
      img.classList.add('activated');
    };

    // When clicking or touching, zoom the preview a little to give feedback.
    // Do not release the 'activated' flag since in most cases, releasing the
    // mouse button or touch would redirect to the browser view.
    img.addEventListener('touchstart', onPointerDown);
    img.addEventListener('mousedown', onPointerDown);

    // Add to DOM.
    picturePreview.appendChild(img);

    // Add the picture to the model.
    this.model_.addPicture(dataURL);

    // Call the callback asynchronously, so the picture is displayed in DOM.
    if (opt_callback)
      setTimeout(opt_callback, 0);
  }.bind(this), 0);
};

/**
 * Resolutions to be probed on the camera. Format: [[width, height], ...].
 * @type {Array.<Array.<number>>}
 * @const
 */
camera.views.Camera.RESOLUTIONS = [[1280, 720], [800, 600], [640, 480]];

/**
 * Synchronizes video size with the window's current size.
 * @private
 */
camera.views.Camera.prototype.synchronizeBounds_ = function() {
  if (!this.video_.videoWidth)
    return;

  var width = document.body.offsetWidth;
  var height = document.body.offsetHeight;

  this.video_.width = this.video_.videoWidth;
  this.video_.height = this.video_.videoHeight;

  // Add 1 pixel to avoid artifacts.
  var zoom = (width + 1) / this.video_.videoWidth;
}

/**
 * Sets resolution of the low-resolution tracker input canvas. Depending on the
 * argument, the resolution is low, or very low.
 *
 * @param {camera.views.Camera.HeadTrackerQuality} quality Quality of the head
 *     tracker.
 * @private
 */
camera.views.Camera.prototype.setHeadTrackerQuality_ = function(quality) {
  var videoRatio = this.video_.videoWidth / this.video_.videoHeight;
  var scale;
  switch (quality) {
    case camera.views.Camera.HeadTrackerQuality.NORMAL:
      scale = 1;
      break;
    case camera.views.Camera.HeadTrackerQuality.LOW:
      scale = 0.75;
      break;
  }
  if (videoRatio < 1.5) {
    // For resolutions: 800x600.
    this.trackerInputCanvas_.width = Math.round(120 * scale);
    this.trackerInputCanvas_.height = Math.round(90 * scale);
  } else {
    // For wide resolutions (any other).
    this.trackerInputCanvas_.width = Math.round(160 * scale);
    this.trackerInputCanvas_.height = Math.round(90 * scale);
  }
};

/**
 * Starts capturing with the specified resolution.
 *
 * @param {Array.<number>} resolution Width and height of the capturing mode,
 *     eg. [800, 600].
 * @param {function(number, number)} onSuccess Success callback with the set
 *     resolution.
 * @param {function()} onFailure Failure callback, eg. the resolution is
 *     not supported.
 * @param {function()} onDisconnected Called when the camera connection is lost.
 * @private
 */
 camera.views.Camera.prototype.startWithResolution_ =
     function(resolution, onSuccess, onFailure, onDisconnected) {
  if (this.running_)
    this.stop();

  navigator.webkitGetUserMedia({
    video: {
      mandatory: {
        minWidth: resolution[0],
        minHeight: resolution[1]
      }
    }
  }, function(stream) {
    this.video_.src = window.URL.createObjectURL(stream);
    var onLoadedMetadata = function() {
      this.video_.removeEventListener('loadedmetadata', onLoadedMetadata);
      this.running_ = true;
      // Use a watchdog since the stream.onended event is unreliable in the
      // recent version of Chrome.
      this.watchdog_ = setInterval(function() {
        if (stream.ended) {
          this.capturing_ = false;
          onDisconnected();
          clearInterval(this.watchdog_);
          this.watchdog_ = null;
        }
      }.bind(this), 1000);
      this.capturing_ = true;
      var onAnimationFrame = function() {
        if (!this.running_)
          return;
        this.onAnimationFrame_();
        requestAnimationFrame(onAnimationFrame);
      }.bind(this);
      onAnimationFrame();
      onSuccess();
    }.bind(this);
    // Load the stream and wait for the metadata.
    this.video_.addEventListener('loadedmetadata', onLoadedMetadata);
    this.video_.play();
  }.bind(this), function(error) {
    onFailure();
  });
};

/**
 * Stops capturing the camera.
 */
camera.views.Camera.prototype.stop = function() {
  this.running_ = false;
  this.capturing_ = false;
  this.video_.pause();
  this.video_.src = '';
  if (this.watchdog_) {
    clearTimeout(this.watchdog_);
    this.watchdog_ = null;
  }
};

/**
 * Sets the window size to the default dimensions.
 * @return {boolean} Whether the window has been resized.
 * @private
 */
camera.views.Camera.prototype.setDefaultGeometry_ = function() {
  var bounds = chrome.app.window.current().getBounds();
  var targetAspectRatio = this.video_.videoWidth / this.video_.videoHeight;
  var targetWidth = Math.round(screen.width * 0.8);
  var targetHeight = Math.round(targetWidth / targetAspectRatio);
  chrome.app.window.current().resizeTo(targetWidth, targetHeight);
  chrome.app.window.current().moveTo(
      bounds.left - (targetWidth - bounds.width) / 2,
      bounds.top - (targetHeight - bounds.height) / 2);
 return bounds.width != targetWidth || bounds.height != targetHeight;
};

/**
 * Starts capturing the camera with the highest possible resolution.
 * @private
 */
camera.views.Camera.prototype.start_ = function() {
  var index = 0;

  var onSuccess = function() {
    // Set the default dimensions to at most half of the available width
    // and to the compatible aspect ratio. 640/360 dimensions are used to
    // detect that the window has never been opened.
    var bounds = chrome.app.window.current().getBounds();
    if (bounds.width == 640 && bounds.height == 360)
      this.setDefaultGeometry_();

    setTimeout(function() {
      // Remove the initialization layer, but with some small delay to give
      // the UI change to reflow and update correctly.
      document.body.classList.remove('initializing');

      // Show tools after some small delay to make it more visible.
      this.ribbonInitialization_ = false;
      this.setExpanded_(true);
    }.bind(this), 500);

    if (this.retryStartTimer_) {
      clearTimeout(this.retryStartTimer_);
      this.retryStartTimer_ = null;
    }
    this.context_.onErrorRecovered('no-camera');
  }.bind(this);

  var scheduleRetry = function() {
    this.context_.onError(
        'no-camera',
        chrome.i18n.getMessage('errorMsgNoCamera'),
        chrome.i18n.getMessage('errorMsgNoCameraHint'));
    if (this.retryStartTimer_) {
      clearTimeout(this.retryStartTimer_);
      this.retryStartTimer_ = null;
    }
    this.retryStartTimer_ = setTimeout(this.start_.bind(this), 1000);
  }.bind(this);

  var onFailure = function() {
    document.body.classList.remove('initializing');
    scheduleRetry();
  }.bind(this);

  var tryNextResolution = function() {
    this.startWithResolution_(
        camera.views.Camera.RESOLUTIONS[index],
        onSuccess,
        function() {
          index++;
          if (index < camera.views.Camera.RESOLUTIONS.length)
            tryNextResolution();
          else
            onFailure();
        },
        scheduleRetry.bind(this));  // onDisconnected
  }.bind(this);

  tryNextResolution();
};

/**
 * Draws the effects' ribbon.
 * @private
 */
camera.views.Camera.prototype.drawEffectsRibbon_ = function() {
  var notDrawn = [];

  // Draw visible frames only.
  for (var index = 0; index < this.previewProcessors_.length; index++) {
    var processor = this.previewProcessors_[index];
    var effectRect = processor.output.getBoundingClientRect();
    if (effectRect.right >= 0 &&
        effectRect.left < document.body.offsetWidth) {
      processor.processFrame();
    } else {
      notDrawn.push(processor);
    }
  }

  // Additionally, draw one frame which is not visible. This is to avoid stale
  // images when scrolling.
  this.staleEffectsRefreshIndex_++;
  if (notDrawn.length)
    notDrawn[this.staleEffectsRefreshIndex_ % notDrawn.length].processFrame();
};

/**
 * Draws a single frame for the main canvas and effects.
 * @param {camera.views.Camera.DrawMode} mode Drawing mode.
 * @private
 */
camera.views.Camera.prototype.drawCameraFrame_ = function(mode) {
  if (this.frame_ % 10 == 0 || mode == camera.views.Camera.DrawMode.OPTIMIZED) {
    this.mainFastCanvasTexture_.loadContentsOf(this.video_);
    this.mainFastProcessor_.processFrame();
  }

  switch (mode) {
    case camera.views.Camera.DrawMode.OPTIMIZED:
      this.mainCanvas_.parentNode.hidden = true;
      this.mainFastCanvas_.parentNode.hidden = false;
      break;
    case camera.views.Camera.DrawMode.FULL:
      this.mainCanvasTexture_.loadContentsOf(this.video_);
      this.mainProcessor_.processFrame();
      this.mainCanvas_.parentNode.hidden = false;
      this.mainFastCanvas_.parentNode.hidden = true;
      break;
  }
};

/**
 * Handles the animation frame event and refreshes the viewport if necessary.
 * @private
 */
camera.views.Camera.prototype.onAnimationFrame_ = function() {
  // No capturing when the view is inactive.
  if (!this.active)
    return;

  // No capturing while resizing.
  if (this.context.resizing)
    return;

  var finishMeasuring = this.performanceMonitor_.startMeasuring();

  // Copy the video frame to the back buffer. The back buffer is low
  // resolution, since it is only used by the effects' previews.
  if (this.frame_ % camera.views.Camera.PREVIEW_BUFFER_SKIP_FRAMES == 0) {
    var context = this.previewInputCanvas_.getContext('2d');
    // Since the preview input canvas may have a different aspect ratio, cut
    // the center of it.
    var ratio =
        this.previewInputCanvas_.width / this.previewInputCanvas_.height;
    var scale = this.previewInputCanvas_.height / this.video_.height;
    var sh = this.video_.height;
    var sw = Math.round(this.video_.height * ratio);
    var sy = 0;
    var sx = Math.round(this.video_.width / 2 - sw / 2);
    context.drawImage(this.video_,
                      sx,
                      sy,
                      sw,
                      sh,
                      0,
                      0,
                      this.previewInputCanvas_.width,
                      this.previewInputCanvas_.height);
    this.previewCanvasTexture_.loadContentsOf(this.previewInputCanvas_);
  }

  // Request update of the head tracker always if it is used by the active
  // effect, or periodically if used on the visible ribbon only.
  // TODO(mtomasz): Do not call the head tracker when performing any CSS
  // transitions or animations.
  var requestHeadTrackerUpdate = this.mainProcessor_.effect.usesHeadTracker() ||
      (this.expanded_ && this.frame_ %
       camera.views.Camera.RIBBON_HEAD_TRACKER_SKIP_FRAMES == 0);

  // Copy the video frame to the back buffer. The back buffer is low resolution
  // since it is only used by the head tracker. Also, if the currently selected
  // effect does not use head tracking, then use even lower resolution, so we
  // can get higher FPS, when the head tracker is used for tiny effect previews
  // only.
  if (!this.tracker_.busy && requestHeadTrackerUpdate) {
    this.setHeadTrackerQuality_(
        this.mainProcessor_.effect.usesHeadTracker() ?
            camera.views.Camera.HeadTrackerQuality.NORMAL :
            camera.views.Camera.HeadTrackerQuality.LOW);

    // Aspect ratios are required to be same.
    var context = this.trackerInputCanvas_.getContext('2d');
    context.drawImage(this.video_,
                      0,
                      0,
                      this.trackerInputCanvas_.width,
                      this.trackerInputCanvas_.height);

    this.tracker_.detect();
  }

  // Update internal state of the tracker.
  this.tracker_.update();

  // Draw the camera frame. Decrease the rendering resolution when scrolling, or
  // while performing animations.
  if (this.taking_ || this.toolbarEffect_.animating ||
      this.controlsEffect_.animating || this.mainProcessor_.effect.isSlow() ||
      this.context.isUIAnimating() || this.toastEffect_.animating ||
      (this.scrollTracker_.scrolling && this.expanded_)) {
    this.drawCameraFrame_(camera.views.Camera.DrawMode.OPTIMIZED);
  } else {
    this.drawCameraFrame_(camera.views.Camera.DrawMode.FULL);
  }

  // Draw the effects' ribbon.
  // Process effect preview canvases. Ribbon initialization is true before the
  // ribbon is expanded for the first time. This trick is used to fill the
  // ribbon with images as soon as possible.
  if (this.expanded_ && !this.taking_ && !this.controlsEffect_.animating &&
      !this.context.isUIAnimating() && !this.scrollTracker_.scrolling &&
      !this.toastEffect_.animating || this.ribbonInitialization_) {

    // Every third frame draw only the visible effects. Also, other frames
    // are periodically refreshed, to avoid stale pictures.
    if (this.frame_ % camera.views.Camera.PREVIEW_BUFFER_SKIP_FRAMES == 0)
      this.drawEffectsRibbon_();
  }

  this.frame_++;
  finishMeasuring();
};

