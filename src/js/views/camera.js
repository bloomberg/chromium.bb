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
        var listWrapper = document.querySelector('#effects-wrapper');
        if (args) {
          toolbar.classList.add('expanded');
          listWrapper.setAttribute('tabIndex', 0);
        } else {
          toolbar.classList.remove('expanded');
          // Make the list not-focusable if it is out of the screen.
          listWrapper.removeAttribute('tabIndex');
          if (document.activeElement == listWrapper)
            document.querySelector('#filters-toggle').focus();
        }
        camera.util.waitForTransitionCompletion(
            document.querySelector('#toolbar'), 500, function() {
          // If the filters button was previously selected, then advance to
          // the ribbon.
          if (args && document.activeElement ==
              document.querySelector('#filters-toggle')) {
            listWrapper.focus();
          }
          callback();
        });
      });

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
      this.scroller_, function() {}, function() {});  // No callbacks.

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

  // End of properties, seal the object.
  Object.seal(this);

  // Synchronize bounds of the video now, when window is resized or if the
  // video dimensions are loaded.
  this.video_.addEventListener('loadedmetadata',
      this.synchronizeBounds_.bind(this));
  this.synchronizeBounds_();

  // Sets dimensions of the input canvas for the effects' preview on the ribbon.
  // Keep in sync with CSS.
  this.previewInputCanvas_.width = 80
  this.previewInputCanvas_.height = 80;

  // Handle the 'Take' button.
  document.querySelector('#take-picture').addEventListener(
      'click', this.takePicture_.bind(this));

  document.querySelector('#toolbar #album-enter').addEventListener('click',
      this.onAlbumEnterClicked_.bind(this));

  document.querySelector('#toolbar #filters-toggle').addEventListener('click',
      this.onFiltersToggleClicked_.bind(this));

  // Hide window controls when moving outside of the window.
  window.addEventListener('mouseout', this.onWindowMouseOut_.bind(this));

  // Hide window controls when any key pressed.
  // TODO(mtomasz): Move managing window controls to main.js.
  window.addEventListener('keydown', this.onWindowKeyDown_.bind(this));

  // Load the shutter sound.
  this.shutterSound_.src = '../sounds/shutter.ogg';
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
 * Number of frames to be skipped between optimized effects' ribbon refreshes
 * and the head detection invocations (which both use the preview back buffer).
 *
 * @type {number}
 * @const
 */
camera.views.Camera.PREVIEW_BUFFER_SKIP_FRAMES = 3;

/**
 * Number of frames to be skipped between effects' ribbon full refreshes.
 * @type {number}
 * @const
 */
camera.views.Camera.EFFECTS_RIBBON_FULL_REFRESH_SKIP_FRAMES = 30;

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
        this.mainCanvasTexture_,
        this.mainCanvas_,
        this.mainCanvas_);
    this.mainFastProcessor_ = new camera.Processor(
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
    this.addEffect_(new camera.effects.Normal(this.tracker_));
    this.addEffect_(new camera.effects.Vintage(this.tracker_));
    this.addEffect_(new camera.effects.Cinema(this.tracker_));
    this.addEffect_(new camera.effects.TiltShift(this.tracker_));
    this.addEffect_(new camera.effects.Retro30(this.tracker_));
    this.addEffect_(new camera.effects.Retro50(this.tracker_));
    this.addEffect_(new camera.effects.Retro60(this.tracker_));
    this.addEffect_(new camera.effects.BigHead(this.tracker_));
    this.addEffect_(new camera.effects.BigJaw(this.tracker_));
    this.addEffect_(new camera.effects.BigEyes(this.tracker_));
    this.addEffect_(new camera.effects.BunnyHead(this.tracker_));
    this.addEffect_(new camera.effects.Grayscale(this.tracker_));
    this.addEffect_(new camera.effects.Sepia(this.tracker_));
    this.addEffect_(new camera.effects.Colorize(this.tracker_));
    this.addEffect_(new camera.effects.Beauty(this.tracker_));
    this.addEffect_(new camera.effects.Newspaper(this.tracker_));
    this.addEffect_(new camera.effects.Funky(this.tracker_));
    this.addEffect_(new camera.effects.Swirl(this.tracker_));

    // Select the default effect.
    // TODO(mtomasz): Move to chrome.storage.local.sync, after implementing
    // syncing of the gallery.
    chrome.storage.local.get('effectIndex', function(values) {
      if (values.effectIndex !== undefined &&
          values.effectIndex < this.previewProcessors_.length) {
        this.setCurrentEffect_(values.effectIndex);
      } else {
        this.setCurrentEffect_(0);
      }
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
  this.scrollTracker_.start();
  this.performanceMonitor_.start();
  this.tracker_.start();
  this.onResize();
};

/**
 * Leaves the view.
 * @override
 */
camera.views.Camera.prototype.onLeave = function() {
  this.scrollTracker_.stop();
  this.performanceMonitor_.stop();
  this.tracker_.stop();
};

/**
 * @override
 */
camera.views.Camera.prototype.onActivate = function() {
  window.focus();
};

/**
 * Handles clicking on the album button.
 * @param {Event} event Mouse event
 * @private
 */
camera.views.Camera.prototype.onAlbumEnterClicked_ = function(event) {
  this.router.navigate(camera.Router.ViewIdentifier.ALBUM);
};

/**
 * Handles clicking on the toggle filters button.
 * @param {Event} event Mouse event
 * @private
 */
camera.views.Camera.prototype.onFiltersToggleClicked_ = function(event) {
  this.setExpanded_(!this.expanded_);
};

/**
 * Handles releasing the puller on the ribbon, and toggles it.
 * @param {number} distance Pulled distance in pixels.
 * @private
 */
camera.views.Camera.prototype.onRibbonPullReleased_ = function(distance) {
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
 * Handles pointer actions, such as mouse or touch activity.
 * @param {Event} event Activity event.
 * @private
 */
camera.views.Camera.prototype.onPointerActivity_ = function(event) {
  // Show the window controls.
  this.setControlsVisible_(true);

  // Update the ribbon's visibility.
  switch (event.type) {
    case 'mousedown':
    case 'touchstart':
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

  // Assign events.
  item.addEventListener('click',
      this.setCurrentEffect_.bind(this, effectIndex));

  // Create the preview processor.
  var processor = new camera.Processor(
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

  var effect = document.querySelector('#effects #effect-' + effectIndex);
  effect.setAttribute('selected', '');
  effect.setAttribute('aria-selected', 'true');
  camera.util.ensureVisible(effect, this.scroller_);

  if (this.currentEffectIndex_ == effectIndex)
    this.previewProcessors_[effectIndex].effect.randomize();
  this.mainProcessor_.effect = this.previewProcessors_[effectIndex].effect;
  this.mainFastProcessor_.effect = this.previewProcessors_[effectIndex].effect;

  var listWrapper = document.querySelector('#effects-wrapper');
  listWrapper.setAttribute('aria-activedescendant', effect.id);
  listWrapper.setAttribute('aria-labelledby', effect.id);
  this.currentEffectIndex_ = effectIndex;

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
    case 'U+0020':
      this.takePicture_();
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
 * Toggles the toolbar visibility.
 * @param {boolean} expanded True to show the toolbar, false to hide.
 * @private
 */
camera.views.Camera.prototype.setExpanded_ = function(expanded) {
  if (this.collapseTimer_) {
    clearTimeout(this.collapseTimer_);
    this.collapseTimer_ = null;
  }
  if (expanded) {
    this.collapseTimer_ = setTimeout(this.setExpanded_.bind(this, false), 3000);
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
 * Takes the picture, saves and puts to the album with a nice animation.
 * @private
 */
camera.views.Camera.prototype.takePicture_ = function() {
  if (!this.running_)
    return;

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

    // Call the callback with the picture.
    this.model_.addPicture(dataURL);
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
  var windowRatio = width / height;
  var videoRatio = this.video_.videoWidth / this.video_.videoHeight;

  this.video_.width = this.video_.videoWidth;
  this.video_.height = this.video_.videoHeight;

  // Add 1 pixel to avoid artifacts.
  var zoom = (width + 1) / this.video_.videoWidth;

  // Set resolution of the low-resolution tracker input canvas.
  if (videoRatio < 1.5) {
    // For resolutions: 800x600.
    this.trackerInputCanvas_.width = 120;
    this.trackerInputCanvas_.height = 90;
  } else {
    // For wide resolutions (any other).
    this.trackerInputCanvas_.width = 192;
    this.trackerInputCanvas_.height = 108;
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
          clearTimeout(this.watchdog_);
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
      onSuccess(resolution[0], resolution[1]);
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
 * Starts capturing the camera with the highest possible resolution.
 * @private
 */
camera.views.Camera.prototype.start_ = function() {
  var index = 0;

  var onSuccess = function(width, height) {
    // Set the default dimensions to at most half of the available width
    // and to the compatible aspect ratio. 640/360 dimensions are used to
    // detect that the window has never been opened.
    var bounds = chrome.app.window.current().getBounds();
    var targetAspectRatio = width / height;
    var targetWidth = Math.round(Math.min(width, screen.width * 0.8));
    var targetHeight = Math.round(targetWidth / targetAspectRatio);
    if (bounds.width == 640 && bounds.height == 360) {
      chrome.app.window.current().resizeTo(targetWidth, targetHeight);
      chrome.app.window.current().moveTo(
          bounds.left - (targetWidth - bounds.width) / 2,
          bounds.top - (targetHeight - bounds.height) / 2);
    }

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
 * @param {camera.views.Camera.DrawMode} mode Drawing mode.
 * @private
 */
camera.views.Camera.prototype.drawEffectsRibbon_ = function(mode) {
  for (var index = 0; index < this.previewProcessors_.length; index++) {
    var processor = this.previewProcessors_[index];
    var effectRect = processor.output.getBoundingClientRect();
    switch (mode) {
      case camera.views.Camera.DrawMode.OPTIMIZED:
        if (effectRect.right >= 0 &&
            effectRect.left < document.body.offsetWidth) {
          processor.processFrame();
        }
        break;
      case camera.views.Camera.DrawMode.FULL:
        processor.processFrame();
        break;
    }
  }
 };

/**
 * Draws a single frame for the main canvas and effects.
 * @param {camera.views.Camera.DrawMode} mode Drawing mode.
 * @private
 */
camera.views.Camera.prototype.drawCameraFrame_ = function(mode) {
  switch (mode) {
    case camera.views.Camera.DrawMode.OPTIMIZED:
      this.mainFastCanvasTexture_.loadContentsOf(this.video_);
      this.mainFastProcessor_.processFrame();
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

  // Copy the video frame to the back buffer. The back buffer is low
  // resolution, since it is only used by the head tracker.
  if (!this.tracker_.busy) {
    var context = this.trackerInputCanvas_.getContext('2d');
    // Aspect ratios are required to be same.
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
      this.context.isUIAnimating() ||
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
      !this.context.isUIAnimating() && !this.scrollTracker_.scrolling ||
      this.ribbonInitialization_) {

    // Every third frame draw only the visible effects. Every 30-th frame, draw
    // all of them, to avoid displaying old effects when scrolling.
    if (this.frame_ %
        camera.views.Camera.EFFECTS_RIBBON_FULL_REFRESH_SKIP_FRAMES == 0) {
      this.drawEffectsRibbon_(camera.views.Camera.DrawMode.FULL);
    } else if (this.frame_ %
        camera.views.Camera.PREVIEW_BUFFER_SKIP_FRAMES == 0) {
      this.drawEffectsRibbon_(camera.views.Camera.DrawMode.OPTIMIZED);
    }
  }

  this.frame_++;
  finishMeasuring();
};

