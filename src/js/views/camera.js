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
 * @constructor
 */
camera.views.Camera = function(context) {
  camera.View.call(this, context);

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
  this.mainCanvas_ = fx.canvas();

  /**
   * The main (full screen canvas) for fast capture.
   * @type {fx.Canvas}
   * @private
   */
  this.mainFastCanvas_ = fx.canvas();

  /**
   * The main (full screen) processor in the full quality mode.
   * @type {camera.Processor}
   * @private
   */
  this.mainProcessor_ = new camera.Processor(this.video_, this.mainCanvas_);

  /**
   * The main (full screen) processor in the fast mode.
   * @type {camera.Processor}
   * @private
   */
  this.mainFastProcessor_ = new camera.Processor(
      this.video_, this.mainFastCanvas_, camera.Processor.Mode.FAST);

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
  this.tracker_ = new camera.Tracker(this.previewInputCanvas_);

  /**
   * Current frame.
   * @type {number}
   * @private
   */
  this.frame_ = 0;

  /**
   * If the tools are expanded.
   * @type {boolean}
   * @private
   */
  this.expanded_ = false;

  /**
   * Tools animation effect wrapper.
   * @type {camera.util.StyleEffect}
   * @private
   */
  this.toolsEffect_ = new camera.util.StyleEffect(
      function(args, callback) {
        if (args)
          document.body.classList.add('expanded');
        else
          document.body.classList.remove('expanded');
        camera.util.waitForTransitionCompletion(
            document.querySelector('#toolbar'), 500, callback);
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

  // End of properties, seal the object.
  Object.seal(this);

  // Insert the main canvas to its container.
  document.querySelector('#main-canvas-wrapper').appendChild(this.mainCanvas_);
  document.querySelector('#main-fast-canvas-wrapper').appendChild(
      this.mainFastCanvas_);

  // Set the default effect.
  this.mainProcessor_.effect = new camera.effects.Swirl();

  // Synchronize bounds of the video now, when window is resized or if the
  // video dimensions are loaded.
  this.video_.addEventListener('loadedmetadata',
      this.synchronizeBounds_.bind(this));
  this.synchronizeBounds_();

  // Prepare effect previews.
  this.addEffect_(new camera.effects.Normal(this.tracker_));
  this.addEffect_(new camera.effects.Vintage(this.tracker_));
  this.addEffect_(new camera.effects.BigHead(this.tracker_));
  this.addEffect_(new camera.effects.BigJaw(this.tracker_));
  this.addEffect_(new camera.effects.BunnyHead(this.tracker_));
  this.addEffect_(new camera.effects.Swirl(this.tracker_));
  this.addEffect_(new camera.effects.Grayscale(this.tracker_));
  this.addEffect_(new camera.effects.Sepia(this.tracker_));
  this.addEffect_(new camera.effects.Colorize(this.tracker_));
  this.addEffect_(new camera.effects.Newspaper(this.tracker_));
  this.addEffect_(new camera.effects.Funky(this.tracker_));
  this.addEffect_(new camera.effects.TiltShift(this.tracker_));
  this.addEffect_(new camera.effects.Cinema(this.tracker_));

  // Select the default effect.
  this.setCurrentEffect_(0);

 // Show tools on touch or mouse move/click.
  document.body.addEventListener(
      'mousemove', this.setExpanded_.bind(this, true));
  document.body.addEventListener(
      'touchmove', this.setExpanded_.bind(this, true));
  document.body.addEventListener(
      'click', this.setExpanded_.bind(this, true));

  // Make the preview ribbon possible to scroll by dragging with mouse.
  document.querySelector('#effects').addEventListener(
      'mousemove', this.onRibbonMouseMove_.bind(this));

  // Handle the 'Take' button.
  document.querySelector('#take-picture').addEventListener(
      'click', this.takePicture_.bind(this));

  // Load the shutter sound.
  this.shutterSound_.src = '../sounds/shutter.wav';

  // Start the camera capture.
  // TODO(mtomasz): Consider moving to enter() for a lighter constructor.
  this.start();
};

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
 * Enters the view.
 * @override
 */
camera.views.Camera.prototype.onEnter = function() {
  this.onResize();
};

/**
 * Leaves the view.
 * @override
 */
camera.views.Camera.prototype.onLeave = function() {
};


/**
 * Adds an effect to the user interface.
 * @param {camera.Effect} effect Effect to be added.
 * @private
 */
camera.views.Camera.prototype.addEffect_ = function(effect) {
  // Create the preview on the ribbon.
  var list = document.querySelector('#effects');
  var wrapper = document.createElement('div');
  wrapper.className = 'preview-canvas-wrapper';
  var canvas = fx.canvas();
  var padder = document.createElement('div');
  padder.className = 'preview-canvas-padder';
  padder.appendChild(canvas);
  wrapper.appendChild(padder);
  var item = document.createElement('li');
  item.appendChild(wrapper);
  list.appendChild(item);
  var label = document.createElement('span');
  label.textContent = effect.getTitle();
  item.appendChild(label);

  // Calculate the effect index.
  var effectIndex = this.previewProcessors_.length;
  item.id = 'effect-' + effectIndex;

  // Assign events.
  item.addEventListener('click',
      this.setCurrentEffect_.bind(this, effectIndex));

  // Create the preview processor.
  var processor = new camera.Processor(this.previewInputCanvas_, canvas);
  processor.effect = effect;
  this.previewProcessors_.push(processor);
};

/**
 * Sets the current effect.
 * @param {number} effectIndex Effect index.
 * @private
 */
camera.views.Camera.prototype.setCurrentEffect_ = function(effectIndex) {
  document.querySelector('#effects #effect-' + this.currentEffectIndex_).
      removeAttribute('selected');
  var element = document.querySelector('#effects #effect-' + effectIndex);
  element.setAttribute('selected', '');
  camera.util.ensureVisible(element, document.querySelector('#effects'));
  if (this.currentEffectIndex_ == effectIndex)
    this.previewProcessors_[effectIndex].effect.randomize();
  this.mainProcessor_.effect = this.previewProcessors_[effectIndex].effect;
  this.mainFastProcessor_.effect = this.previewProcessors_[effectIndex].effect;
  this.currentEffectIndex_ = effectIndex;
};

/**
 * @override
 */
camera.views.Camera.prototype.onResize = function() {
  this.synchronizeBounds_();
};

/**
 * @override
 */
camera.views.Camera.prototype.onKeyPressed = function(event) {
  switch (event.keyIdentifier) {
    case 'Left':
      this.setCurrentEffect_(
          (this.currentEffectIndex_ + this.previewProcessors_.length - 1) %
              this.previewProcessors_.length);
      break;
    case 'Right':
      this.setCurrentEffect_(
          (this.currentEffectIndex_ + 1) % this.previewProcessors_.length);
      break;
    case 'Home':
      this.setCurrentEffect_(0);
      break;
    case 'End':
      this.setCurrentEffect_(this.previewProcessors_.length - 1);
      break;
    case 'Enter':
    case 'U+0020':
      this.takePicture_();
      event.stopPropagation();
      event.preventDefault();
      break;
  }
};

/**
 * Handles scrolling via mouse on the effects ribbon.
 * @param {Event} event Mouse move event.
 * @private
 */
camera.views.Camera.prototype.onRibbonMouseMove_ = function(event) {
  if (event.which != 1)
    return;
  var ribbon = document.querySelector('#effects');
  ribbon.scrollLeft = ribbon.scrollLeft - event.webkitMovementX;
};

/**
 * Toggles the tools visibility.
 * @param {boolean} expanded True to show the tools, false to hide.
 * @private
 */
camera.views.Camera.prototype.setExpanded_ = function(expanded) {
  if (this.collapseTimer_) {
    clearTimeout(this.collapseTimer_);
    this.collapseTimer_ = null;
  }
  if (expanded) {
    this.collapseTimer_ = setTimeout(this.setExpanded_.bind(this, false), 2000);
    if (!this.expanded_) {
      this.toolsEffect_.invoke(true, function() {
        this.expanded_ = true;
      }.bind(this));
    }
  } else {
    if (this.expanded_) {
      this.expanded_ = false;
      this.toolsEffect_.invoke(false, function() {});
    }
  }
};

/**
 * Chooses a file stream to override the camera stream. Used for debugging.
 */
camera.views.Camera.prototype.chooseFileStream = function() {
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
 * Takes the picture, saves and puts to the gallery with a nice animation.
 * @private
 */
camera.views.Camera.prototype.takePicture_ = function() {
  if (!this.running_)
    return;

  // Lock refreshing for smoother experience.
  this.taking_ = true;

  // Show flashing animation with the shutter sound.
  document.body.classList.add('show-shutter');
  var galleryButton = document.querySelector('#toolbar .gallery-button');
  camera.util.setAnimationClass(galleryButton, galleryButton, 'flash');
  setTimeout(function() {
    document.body.classList.remove('show-shutter');
  }.bind(this), 200);

  this.shutterSound_.currentTime = 0;
  this.shutterSound_.play();

  setTimeout(function() {
    this.mainProcessor_.processFrame();
    var dataURL = this.mainCanvas_.toDataURL('image/jpeg');

    // Create a picture preview animation.
    var picturePreview = document.querySelector('#picture-preview');
    var img = document.createElement('img');
    img.src = dataURL;
    img.style.webkitTransform = 'rotate(' + (Math.random() * 40 - 20) + 'deg)';
    picturePreview.appendChild(img);
    camera.util.waitForAnimationCompletion(img, 2500, function() {
      img.parentNode.removeChild(img);
     this.taking_ = false;
    }.bind(this));

    // Call the callback with the picture.
    this.context.onPictureTaken(dataURL);
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

  // Set resolution of the low-resolution preview input canvas.
  if (videoRatio < 1.5) {
    // For resolutions: 800x600.
    this.previewInputCanvas_.width = 120;
    this.previewInputCanvas_.height = 90;
  } else {
    // For wide resolutions (any other).
    this.previewInputCanvas_.width = 192;
    this.previewInputCanvas_.height = 108;
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
    this.video_.src = window.URL.createObjectURL(stream);
    this.video_.play();
    this.capturing_ = true;
    var onAnimationFrame = function() {
      if (!this.running_)
        return;
      this.drawFrame_();
      requestAnimationFrame(onAnimationFrame);
    }.bind(this);
    onAnimationFrame();
    onSuccess(resolution[0], resolution[1]);
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
 */
camera.views.Camera.prototype.start = function() {
  var index = 0;

  var onSuccess = function(width, height) {
    // Set the default dimensions to at most half of the available width
    // and to the compatible aspect ratio. 640/360 dimensions are used to
    // detect that the window has never been opened.
    var windowWidth = document.body.offsetWidth;
    var windowHeight = document.body.offsetHeight;
    var targetAspectRatio = width / height;
    var targetWidth = Math.round(Math.min(width, screen.width / 2));
    var targetHeight = Math.round(targetWidth / targetAspectRatio);
    if (windowWidth == 640 && windowHeight == 360)
      chrome.app.window.current().resizeTo(targetWidth, targetHeight);
    chrome.app.window.current().show();
    // Show tools after some small delay to make it more visible.
    setTimeout(function() {
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
    this.retryStartTimer_ = setTimeout(this.start.bind(this), 1000);
  }.bind(this);

  var onFailure = function() {
    chrome.app.window.current().show();
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
 * Draws a single frame for the main canvas and effects.
 * @private
 */
camera.views.Camera.prototype.drawFrame_ = function() {
  // No capturing when the view is inactive.
  if (!this.active)
    return;

  // No capturing while resizing.
  if (this.context.resizing)
    return;

  // Copy the video frame to the back buffer. The back buffer is low resolution,
  // since it is only used by preview windows.
  var context = this.previewInputCanvas_.getContext('2d');
  context.drawImage(this.video_,
                    0,
                    0,
                    this.previewInputCanvas_.width,
                    this.previewInputCanvas_.height);

  // Detect and track faces.
  if (this.frame_ % 3 == 0)
    this.tracker_.detect();

  // Update internal state of the tracker.
  this.tracker_.update();

  // Process effect preview canvases. Ribbin initialization is true before the
  // ribbon is expanded for the first time. This trick is used to fill the
  // ribbon with images as soon as possible.
  if (this.frame_ % 3 == 0 && this.expanded_ && !this.taking_ ||
      this.ribbonInitialization_) {
    for (var index = 0; index < this.previewProcessors_.length; index++) {
      this.previewProcessors_[index].processFrame();
    }
  }
  this.frame_++;

  // Process the full resolution frame. Decrease FPS when expanding for smooth
  // animations.
  if (this.taking_ || this.toolsEffect_.isActive() ||
      this.mainProcessor_.effect.isSlow()) {
    this.mainFastProcessor_.processFrame();
    this.mainCanvas_.parentNode.hidden = true;
    this.mainFastCanvas_.parentNode.hidden = false;
  } else {
    this.mainProcessor_.processFrame();
    this.mainCanvas_.parentNode.hidden = false;
    this.mainFastCanvas_.parentNode.hidden = true;
   }
};

