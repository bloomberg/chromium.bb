// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Slide mode displays a single image and has a set of controls to navigate
 * between the images and to edit an image.
 *
 * @param {Element} container Main container element.
 * @param {Element} content Content container element.
 * @param {Element} toolbar Toolbar element.
 * @param {ImageEditor.Prompt} prompt Prompt.
 * @param {cr.ui.ArrayDataModel} dataModel Data model.
 * @param {cr.ui.ListSelectionModel} selectionModel Selection model.
 * @param {Object} context Context.
 * @param {function(function())} toggleMode Function to toggle the Gallery mode.
 * @param {function(string):string} displayStringFunction String formatting
 *     function.
 * @constructor
 */
function SlideMode(container, content, toolbar, prompt,
                   dataModel, selectionModel, context,
                   toggleMode, displayStringFunction) {
  this.container_ = container;
  this.document_ = container.ownerDocument;
  this.content = content;
  this.toolbar_ = toolbar;
  this.prompt_ = prompt;
  this.dataModel_ = dataModel;
  this.selectionModel_ = selectionModel;
  this.context_ = context;
  this.metadataCache_ = context.metadataCache;
  this.toggleMode_ = toggleMode;
  this.displayStringFunction_ = displayStringFunction;

  this.onSelectionBound_ = this.onSelection_.bind(this);
  this.onSpliceBound_ = this.onSplice_.bind(this);

  // Unique numeric key, incremented per each load attempt used to discard
  // old attempts. This can happen especially when changing selection fast or
  // Internet connection is slow.
  this.currentUniqueKey_ = 0;

  this.initListeners_();
  this.initDom_();
}

/**
 * List of available editor modes.
 * @type {Array.<ImageEditor.Mode>}
 * @const
 */
SlideMode.EDITOR_MODES = Object.freeze([
  new ImageEditor.Mode.InstantAutofix(),
  new ImageEditor.Mode.Crop(),
  new ImageEditor.Mode.Exposure(),
  new ImageEditor.Mode.OneClick(
      'rotate_left', 'GALLERY_ROTATE_LEFT', new Command.Rotate(-1)),
  new ImageEditor.Mode.OneClick(
      'rotate_right', 'GALLERY_ROTATE_RIGHT', new Command.Rotate(1))
]);

/**
 * Map of the key identifier and offset delta.
 * @type {Object.<string, Array.<number>})
 * @const
 */
SlideMode.KEY_OFFSET_MAP = Object.freeze({
  'Up': Object.freeze([0, 20]),
  'Down': Object.freeze([0, -20]),
  'Left': Object.freeze([20, 0]),
  'Right': Object.freeze([-20, 0])
});

/**
 * SlideMode extends cr.EventTarget.
 */
SlideMode.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * @return {string} Mode name.
 */
SlideMode.prototype.getName = function() { return 'slide'; };

/**
 * @return {string} Mode title.
 */
SlideMode.prototype.getTitle = function() { return 'GALLERY_SLIDE'; };

/**
 * @return {Viewport} Viewport.
 */
SlideMode.prototype.getViewport = function() { return this.viewport_; };

/**
 * Initialize the listeners.
 * @private
 */
SlideMode.prototype.initListeners_ = function() {
  window.addEventListener('resize', this.onResize_.bind(this));
};

/**
 * Initialize the UI.
 * @private
 */
SlideMode.prototype.initDom_ = function() {
  // Container for displayed image.
  this.imageContainer_ = util.createChild(
      this.document_.querySelector('.content'), 'image-container');
  this.imageContainer_.addEventListener('click', this.onClick_.bind(this));

  this.document_.addEventListener('click', this.onDocumentClick_.bind(this));

  // Overwrite options and info bubble.
  this.options_ = util.createChild(
      this.toolbar_.querySelector('.filename-spacer'), 'options');

  this.savedLabel_ = util.createChild(this.options_, 'saved');
  this.savedLabel_.textContent = this.displayStringFunction_('GALLERY_SAVED');

  var overwriteOriginalBox =
      util.createChild(this.options_, 'overwrite-original');

  this.overwriteOriginal_ = util.createChild(
      overwriteOriginalBox, '', 'input');
  this.overwriteOriginal_.type = 'checkbox';
  this.overwriteOriginal_.id = 'overwrite-checkbox';
  util.platform.getPreference(SlideMode.OVERWRITE_KEY, function(value) {
    // Out-of-the box default is 'true'
    this.overwriteOriginal_.checked =
        (typeof value !== 'string' || value === 'true');
  }.bind(this));
  this.overwriteOriginal_.addEventListener('click',
      this.onOverwriteOriginalClick_.bind(this));

  var overwriteLabel = util.createChild(overwriteOriginalBox, '', 'label');
  overwriteLabel.textContent =
      this.displayStringFunction_('GALLERY_OVERWRITE_ORIGINAL');
  overwriteLabel.setAttribute('for', 'overwrite-checkbox');

  this.bubble_ = util.createChild(this.toolbar_, 'bubble');
  this.bubble_.hidden = true;

  var bubbleContent = util.createChild(this.bubble_);
  bubbleContent.innerHTML = this.displayStringFunction_(
      'GALLERY_OVERWRITE_BUBBLE');

  util.createChild(this.bubble_, 'pointer bottom', 'span');

  var bubbleClose = util.createChild(this.bubble_, 'close-x');
  bubbleClose.addEventListener('click', this.onCloseBubble_.bind(this));

  // Ribbon and related controls.
  this.arrowBox_ = util.createChild(this.container_, 'arrow-box');

  this.arrowLeft_ =
      util.createChild(this.arrowBox_, 'arrow left tool dimmable');
  this.arrowLeft_.addEventListener('click',
      this.advanceManually.bind(this, -1));
  util.createChild(this.arrowLeft_);

  util.createChild(this.arrowBox_, 'arrow-spacer');

  this.arrowRight_ =
      util.createChild(this.arrowBox_, 'arrow right tool dimmable');
  this.arrowRight_.addEventListener('click',
      this.advanceManually.bind(this, 1));
  util.createChild(this.arrowRight_);

  this.ribbonSpacer_ = this.toolbar_.querySelector('.ribbon-spacer');
  this.ribbon_ = new Ribbon(
      this.document_, this.dataModel_, this.selectionModel_);
  this.ribbonSpacer_.appendChild(this.ribbon_);

  // Error indicator.
  var errorWrapper = util.createChild(this.container_, 'prompt-wrapper');
  errorWrapper.setAttribute('pos', 'center');

  this.errorBanner_ = util.createChild(errorWrapper, 'error-banner');

  util.createChild(this.container_, 'spinner');

  var slideShowButton = this.toolbar_.querySelector('button.slideshow');
  slideShowButton.title = this.displayStringFunction_('GALLERY_SLIDESHOW');
  slideShowButton.addEventListener('click',
      this.startSlideshow.bind(this, SlideMode.SLIDESHOW_INTERVAL_FIRST));

  var slideShowToolbar =
      util.createChild(this.container_, 'tool slideshow-toolbar');
  util.createChild(slideShowToolbar, 'slideshow-play').
      addEventListener('click', this.toggleSlideshowPause_.bind(this));
  util.createChild(slideShowToolbar, 'slideshow-end').
      addEventListener('click', this.stopSlideshow_.bind(this));

  // Editor.

  this.editButton_ = this.toolbar_.querySelector('button.edit');
  this.editButton_.title = this.displayStringFunction_('GALLERY_EDIT');
  this.editButton_.setAttribute('disabled', '');  // Disabled by default.
  this.editButton_.addEventListener('click', this.toggleEditor.bind(this));

  this.printButton_ = this.toolbar_.querySelector('button.print');
  this.printButton_.title = this.displayStringFunction_('GALLERY_PRINT');
  this.printButton_.setAttribute('disabled', '');  // Disabled by default.
  this.printButton_.addEventListener('click', this.print_.bind(this));

  this.editBarSpacer_ = this.toolbar_.querySelector('.edit-bar-spacer');
  this.editBarMain_ = util.createChild(this.editBarSpacer_, 'edit-main');

  this.editBarMode_ = util.createChild(this.container_, 'edit-modal');
  this.editBarModeWrapper_ = util.createChild(
      this.editBarMode_, 'edit-modal-wrapper');
  this.editBarModeWrapper_.hidden = true;

  // Objects supporting image display and editing.
  this.viewport_ = new Viewport();

  this.imageView_ = new ImageView(
      this.imageContainer_,
      this.viewport_);

  this.editor_ = new ImageEditor(
      this.viewport_,
      this.imageView_,
      this.prompt_,
      {
        root: this.container_,
        image: this.imageContainer_,
        toolbar: this.editBarMain_,
        mode: this.editBarModeWrapper_
      },
      SlideMode.EDITOR_MODES,
      this.displayStringFunction_,
      this.onToolsVisibilityChanged_.bind(this));

  this.touchHandlers_ = new TouchHandler(this.imageContainer_, this);
};

/**
 * Load items, display the selected item.
 * @param {Rect} zoomFromRect Rectangle for zoom effect.
 * @param {function} displayCallback Called when the image is displayed.
 * @param {function} loadCallback Called when the image is displayed.
 */
SlideMode.prototype.enter = function(
    zoomFromRect, displayCallback, loadCallback) {
  this.sequenceDirection_ = 0;
  this.sequenceLength_ = 0;

  var loadDone = function(loadType, delay) {
    this.active_ = true;

    this.selectionModel_.addEventListener('change', this.onSelectionBound_);
    this.dataModel_.addEventListener('splice', this.onSpliceBound_);

    ImageUtil.setAttribute(this.arrowBox_, 'active', this.getItemCount_() > 1);
    this.ribbon_.enable();

    // Wait 1000ms after the animation is done, then prefetch the next image.
    this.requestPrefetch(1, delay + 1000);

    if (loadCallback) loadCallback();
  }.bind(this);

  // The latest |leave| call might have left the image animating. Remove it.
  this.unloadImage_();

  new Promise(function(fulfill) {
    // If the items are empty, just show the error message.
    if (this.getItemCount_() === 0) {
      this.displayedIndex_ = -1;
      //TODO(hirono) Show this message in the grid mode too.
      this.showErrorBanner_('GALLERY_NO_IMAGES');
      fulfill();
      return;
    }

    // Remember the selection if it is empty or multiple. It will be restored
    // in |leave| if the user did not changing the selection manually.
    var currentSelection = this.selectionModel_.selectedIndexes;
    if (currentSelection.length === 1)
      this.savedSelection_ = null;
    else
      this.savedSelection_ = currentSelection;

    // Ensure valid single selection.
    // Note that the SlideMode object is not listening to selection change yet.
    this.select(Math.max(0, this.getSelectedIndex()));
    this.displayedIndex_ = this.getSelectedIndex();

    // Show the selected item ASAP, then complete the initialization
    // (loading the ribbon thumbnails can take some time).
    var selectedItem = this.getSelectedItem();

    // Load the image of the item.
    this.loadItem_(
        selectedItem,
        zoomFromRect && this.imageView_.createZoomEffect(zoomFromRect),
        displayCallback,
        function(loadType, delay) {
          fulfill(delay);
        });
  }.bind(this)).then(function(delay) {
    // Turn the mode active.
    this.active_ = true;
    ImageUtil.setAttribute(this.arrowBox_, 'active', this.getItemCount_() > 1);
    this.ribbon_.enable();

    // Register handlers.
    this.selectionModel_.addEventListener('change', this.onSelectionBound_);
    this.dataModel_.addEventListener('splice', this.onSpliceBound_);
    this.touchHandlers_.enabled = true;

    // Wait 1000ms after the animation is done, then prefetch the next image.
    this.requestPrefetch(1, delay + 1000);

    // Call load callback.
    if (loadCallback)
      loadCallback();
  }.bind(this)).catch(function(error) {
    console.error(error.stack, error);
  });
};

/**
 * Leave the mode.
 * @param {Rect} zoomToRect Rectangle for zoom effect.
 * @param {function} callback Called when the image is committed and
 *   the zoom-out animation has started.
 */
SlideMode.prototype.leave = function(zoomToRect, callback) {
  var commitDone = function() {
      this.stopEditing_();
      this.stopSlideshow_();
      ImageUtil.setAttribute(this.arrowBox_, 'active', false);
      this.selectionModel_.removeEventListener(
          'change', this.onSelectionBound_);
      this.dataModel_.removeEventListener('splice', this.onSpliceBound_);
      this.ribbon_.disable();
      this.active_ = false;
      if (this.savedSelection_)
        this.selectionModel_.selectedIndexes = this.savedSelection_;
      this.unloadImage_(zoomToRect);
      callback();
    }.bind(this);

  this.viewport_.resetView();
  if (this.getItemCount_() === 0) {
    this.showErrorBanner_(false);
    commitDone();
  } else {
    this.commitItem_(commitDone);
  }

  // Disable the slide-mode only buttons when leaving.
  this.editButton_.setAttribute('disabled', '');
  this.printButton_.setAttribute('disabled', '');

  // Disable touch operation.
  this.touchHandlers_.enabled = false;
};


/**
 * Execute an action when the editor is not busy.
 *
 * @param {function} action Function to execute.
 */
SlideMode.prototype.executeWhenReady = function(action) {
  this.editor_.executeWhenReady(action);
};

/**
 * @return {boolean} True if the mode has active tools (that should not fade).
 */
SlideMode.prototype.hasActiveTool = function() {
  return this.isEditing();
};

/**
 * @return {number} Item count.
 * @private
 */
SlideMode.prototype.getItemCount_ = function() {
  return this.dataModel_.length;
};

/**
 * @param {number} index Index.
 * @return {Gallery.Item} Item.
 */
SlideMode.prototype.getItem = function(index) {
  return this.dataModel_.item(index);
};

/**
 * @return {Gallery.Item} Selected index.
 */
SlideMode.prototype.getSelectedIndex = function() {
  return this.selectionModel_.selectedIndex;
};

/**
 * @return {Rect} Screen rectangle of the selected image.
 */
SlideMode.prototype.getSelectedImageRect = function() {
  if (this.getSelectedIndex() < 0)
    return null;
  else
    return this.viewport_.getImageBoundsOnScreen();
};

/**
 * @return {Gallery.Item} Selected item.
 */
SlideMode.prototype.getSelectedItem = function() {
  return this.getItem(this.getSelectedIndex());
};

/**
 * Toggles the full screen mode.
 * @private
 */
SlideMode.prototype.toggleFullScreen_ = function() {
  util.toggleFullScreen(this.context_.appWindow,
                        !util.isFullScreen(this.context_.appWindow));
};

/**
 * Selection change handler.
 *
 * Commits the current image and displays the newly selected image.
 * @private
 */
SlideMode.prototype.onSelection_ = function() {
  if (this.selectionModel_.selectedIndexes.length === 0)
    return;  // Temporary empty selection.

  // Forget the saved selection if the user changed the selection manually.
  if (!this.isSlideshowOn_())
    this.savedSelection_ = null;

  if (this.getSelectedIndex() === this.displayedIndex_)
    return;  // Do not reselect.

  this.commitItem_(this.loadSelectedItem_.bind(this));
};

/**
 * Handles changes in tools visibility, and if the header is dimmed, then
 * requests disabling the draggable app region.
 *
 * @private
 */
SlideMode.prototype.onToolsVisibilityChanged_ = function() {
  var headerDimmed =
      this.document_.querySelector('.header').hasAttribute('dimmed');
  this.context_.onAppRegionChanged(!headerDimmed);
};

/**
 * Change the selection.
 *
 * @param {number} index New selected index.
 * @param {number=} opt_slideHint Slide animation direction (-1|1).
 */
SlideMode.prototype.select = function(index, opt_slideHint) {
  this.slideHint_ = opt_slideHint;
  this.selectionModel_.selectedIndex = index;
  this.selectionModel_.leadIndex = index;
};

/**
 * Load the selected item.
 *
 * @private
 */
SlideMode.prototype.loadSelectedItem_ = function() {
  var slideHint = this.slideHint_;
  this.slideHint_ = undefined;

  var index = this.getSelectedIndex();
  if (index === this.displayedIndex_)
    return;  // Do not reselect.

  var step = slideHint || (index - this.displayedIndex_);

  if (Math.abs(step) != 1) {
    // Long leap, the sequence is broken, we have no good prefetch candidate.
    this.sequenceDirection_ = 0;
    this.sequenceLength_ = 0;
  } else if (this.sequenceDirection_ === step) {
    // Keeping going in sequence.
    this.sequenceLength_++;
  } else {
    // Reversed the direction. Reset the counter.
    this.sequenceDirection_ = step;
    this.sequenceLength_ = 1;
  }

  this.displayedIndex_ = index;
  var selectedItem = this.getSelectedItem();

  if (this.sequenceLength_ <= 1) {
    // We have just broke the sequence. Touch the current image so that it stays
    // in the cache longer.
    this.imageView_.prefetch(selectedItem);
  }

  function shouldPrefetch(loadType, step, sequenceLength) {
    // Never prefetch when selecting out of sequence.
    if (Math.abs(step) != 1)
      return false;

    // Always prefetch if the previous load was from cache.
    if (loadType === ImageView.LOAD_TYPE_CACHED_FULL)
      return true;

    // Prefetch if we have been going in the same direction for long enough.
    return sequenceLength >= 3;
  }

  this.currentUniqueKey_++;
  var selectedUniqueKey = this.currentUniqueKey_;

  // Discard, since another load has been invoked after this one.
  if (selectedUniqueKey != this.currentUniqueKey_)
    return;

  this.loadItem_(
      selectedItem,
      new ImageView.Effect.Slide(step, this.isSlideshowPlaying_()),
      function() {} /* no displayCallback */,
      function(loadType, delay) {
        // Discard, since another load has been invoked after this one.
        if (selectedUniqueKey != this.currentUniqueKey_)
          return;
        if (shouldPrefetch(loadType, step, this.sequenceLength_))
          this.requestPrefetch(step, delay);
        if (this.isSlideshowPlaying_())
          this.scheduleNextSlide_();
      }.bind(this));
};

/**
 * Unload the current image.
 *
 * @param {Rect} zoomToRect Rectangle for zoom effect.
 * @private
 */
SlideMode.prototype.unloadImage_ = function(zoomToRect) {
  this.imageView_.unload(zoomToRect);
};

/**
 * Data model 'splice' event handler.
 * @param {Event} event Event.
 * @private
 */
SlideMode.prototype.onSplice_ = function(event) {
  ImageUtil.setAttribute(this.arrowBox_, 'active', this.getItemCount_() > 1);

  // Splice invalidates saved indices, drop the saved selection.
  this.savedSelection_ = null;

  if (event.removed.length != 1)
    return;

  // Delay the selection to let the ribbon splice handler work first.
  setTimeout(function() {
    if (event.index < this.dataModel_.length) {
      // There is the next item, select it.
      // The next item is now at the same index as the removed one, so we need
      // to correct displayIndex_ so that loadSelectedItem_ does not think
      // we are re-selecting the same item (and does right-to-left slide-in
      // animation).
      this.displayedIndex_ = event.index - 1;
      this.select(event.index);
    } else if (this.dataModel_.length) {
      // Removed item is the rightmost, but there are more items.
      this.select(event.index - 1);  // Select the new last index.
    } else {
      // No items left. Unload the image and show the banner.
      this.commitItem_(function() {
        this.unloadImage_();
        this.showErrorBanner_('GALLERY_NO_IMAGES');
      }.bind(this));
    }
  }.bind(this), 0);
};

/**
 * @param {number} direction -1 for left, 1 for right.
 * @return {number} Next index in the given direction, with wrapping.
 * @private
 */
SlideMode.prototype.getNextSelectedIndex_ = function(direction) {
  function advance(index, limit) {
    index += (direction > 0 ? 1 : -1);
    if (index < 0)
      return limit - 1;
    if (index === limit)
      return 0;
    return index;
  }

  // If the saved selection is multiple the Slideshow should cycle through
  // the saved selection.
  if (this.isSlideshowOn_() &&
      this.savedSelection_ && this.savedSelection_.length > 1) {
    var pos = advance(this.savedSelection_.indexOf(this.getSelectedIndex()),
        this.savedSelection_.length);
    return this.savedSelection_[pos];
  } else {
    return advance(this.getSelectedIndex(), this.getItemCount_());
  }
};

/**
 * Advance the selection based on the pressed key ID.
 * @param {string} keyID Key identifier.
 */
SlideMode.prototype.advanceWithKeyboard = function(keyID) {
  var prev = (keyID === 'Up' ||
              keyID === 'Left' ||
              keyID === 'MediaPreviousTrack');
  this.advanceManually(prev ? -1 : 1);
};

/**
 * Advance the selection as a result of a user action (as opposed to an
 * automatic change in the slideshow mode).
 * @param {number} direction -1 for left, 1 for right.
 */
SlideMode.prototype.advanceManually = function(direction) {
  if (this.isSlideshowPlaying_())
    this.pauseSlideshow_();
  cr.dispatchSimpleEvent(this, 'useraction');
  this.selectNext(direction);
};

/**
 * Select the next item.
 * @param {number} direction -1 for left, 1 for right.
 */
SlideMode.prototype.selectNext = function(direction) {
  this.select(this.getNextSelectedIndex_(direction), direction);
};

/**
 * Select the first item.
 */
SlideMode.prototype.selectFirst = function() {
  this.select(0);
};

/**
 * Select the last item.
 */
SlideMode.prototype.selectLast = function() {
  this.select(this.getItemCount_() - 1);
};

// Loading/unloading

/**
 * Load and display an item.
 *
 * @param {Gallery.Item} item Item.
 * @param {Object} effect Transition effect object.
 * @param {function} displayCallback Called when the image is displayed
 *     (which can happen before the image load due to caching).
 * @param {function} loadCallback Called when the image is fully loaded.
 * @private
 */
SlideMode.prototype.loadItem_ = function(
    item, effect, displayCallback, loadCallback) {
  var entry = item.getEntry();
  var metadata = item.getMetadata();
  this.showSpinner_(true);

  var loadDone = function(loadType, delay, error) {
    this.showSpinner_(false);
    if (loadType === ImageView.LOAD_TYPE_ERROR) {
      // if we have a specific error, then display it
      if (error) {
        this.showErrorBanner_(error);
      } else {
        // otherwise try to infer general error
        this.showErrorBanner_('GALLERY_IMAGE_ERROR');
      }
    } else if (loadType === ImageView.LOAD_TYPE_OFFLINE) {
      this.showErrorBanner_('GALLERY_IMAGE_OFFLINE');
    }

    ImageUtil.metrics.recordUserAction(ImageUtil.getMetricName('View'));

    var toMillions = function(number) {
      return Math.round(number / (1000 * 1000));
    };

    ImageUtil.metrics.recordSmallCount(ImageUtil.getMetricName('Size.MB'),
        toMillions(metadata.filesystem.size));

    var canvas = this.imageView_.getCanvas();
    ImageUtil.metrics.recordSmallCount(ImageUtil.getMetricName('Size.MPix'),
        toMillions(canvas.width * canvas.height));

    var extIndex = entry.name.lastIndexOf('.');
    var ext = extIndex < 0 ? '' :
        entry.name.substr(extIndex + 1).toLowerCase();
    if (ext === 'jpeg') ext = 'jpg';
    ImageUtil.metrics.recordEnum(
        ImageUtil.getMetricName('FileType'), ext, ImageUtil.FILE_TYPES);

    // Enable or disable buttons for editing and printing.
    if (error) {
      this.editButton_.setAttribute('disabled', '');
      this.printButton_.setAttribute('disabled', '');
    } else {
      this.editButton_.removeAttribute('disabled');
      this.printButton_.removeAttribute('disabled');
    }

    // For once edited image, disallow the 'overwrite' setting change.
    ImageUtil.setAttribute(this.options_, 'saved',
        !this.getSelectedItem().isOriginal());

    util.platform.getPreference(SlideMode.OVERWRITE_BUBBLE_KEY,
        function(value) {
          var times = typeof value === 'string' ? parseInt(value, 10) : 0;
          if (times < SlideMode.OVERWRITE_BUBBLE_MAX_TIMES) {
            this.bubble_.hidden = false;
            if (this.isEditing()) {
              util.platform.setPreference(
                  SlideMode.OVERWRITE_BUBBLE_KEY, times + 1);
            }
          }
        }.bind(this));

    loadCallback(loadType, delay);
  }.bind(this);

  var displayDone = function() {
    cr.dispatchSimpleEvent(this, 'image-displayed');
    displayCallback();
  }.bind(this);

  this.editor_.openSession(
      item, effect, this.saveCurrentImage_.bind(this), displayDone, loadDone);
};

/**
 * Commit changes to the current item and reset all messages/indicators.
 *
 * @param {function} callback Callback.
 * @private
 */
SlideMode.prototype.commitItem_ = function(callback) {
  this.showSpinner_(false);
  this.showErrorBanner_(false);
  this.editor_.getPrompt().hide();
  this.editor_.closeSession(callback);
};

/**
 * Request a prefetch for the next image.
 *
 * @param {number} direction -1 or 1.
 * @param {number} delay Delay in ms. Used to prevent the CPU-heavy image
 *   loading from disrupting the animation that might be still in progress.
 */
SlideMode.prototype.requestPrefetch = function(direction, delay) {
  if (this.getItemCount_() <= 1) return;

  var index = this.getNextSelectedIndex_(direction);
  this.imageView_.prefetch(this.getItem(index), delay);
};

// Event handlers.

/**
 * Unload handler, to be called from the top frame.
 * @param {boolean} exiting True if the app is exiting.
 */
SlideMode.prototype.onUnload = function(exiting) {
};

/**
 * Click handler for the image container.
 *
 * @param {Event} event Mouse click event.
 * @private
 */
SlideMode.prototype.onClick_ = function(event) {
};

/**
 * Click handler for the entire document.
 * @param {Event} e Mouse click event.
 * @private
 */
SlideMode.prototype.onDocumentClick_ = function(e) {
  // Close the bubble if clicked outside of it and if it is visible.
  if (!this.bubble_.contains(e.target) &&
      !this.editButton_.contains(e.target) &&
      !this.arrowLeft_.contains(e.target) &&
      !this.arrowRight_.contains(e.target) &&
      !this.bubble_.hidden) {
    this.bubble_.hidden = true;
  }
};

/**
 * Keydown handler.
 *
 * @param {Event} event Event.
 * @return {boolean} True if handled.
 */
SlideMode.prototype.onKeyDown = function(event) {
  var keyID = util.getKeyModifiers(event) + event.keyIdentifier;

  if (this.isSlideshowOn_()) {
    switch (keyID) {
      case 'U+001B':  // Escape exits the slideshow.
      case 'MediaStop':
        this.stopSlideshow_(event);
        break;

      case 'U+0020':  // Space pauses/resumes the slideshow.
      case 'MediaPlayPause':
        this.toggleSlideshowPause_();
        break;

      case 'Up':
      case 'Down':
      case 'Left':
      case 'Right':
      case 'MediaNextTrack':
      case 'MediaPreviousTrack':
        this.advanceWithKeyboard(keyID);
        break;
    }
    return true;  // Consume all keystrokes in the slideshow mode.
  }

  if (this.isEditing() && this.editor_.onKeyDown(event))
    return true;

  switch (keyID) {
    case 'Ctrl-U+0050':  // Ctrl+'p' prints the current image.
      if (!this.printButton_.hasAttribute('disabled'))
        this.print_();
      break;

    case 'U+0045':  // 'e' toggles the editor.
      if (!this.editButton_.hasAttribute('disabled'))
        this.toggleEditor(event);
      break;

    case 'U+001B':  // Escape
      if (this.isEditing()) {
        this.toggleEditor(event);
      } else if (this.viewport_.isZoomed()) {
        this.viewport_.resetView();
        this.touchHandlers_.stopOperation();
        this.imageView_.applyViewportChange();
      } else {
        return false;  // Not handled.
      }
      break;

    case 'Home':
      this.selectFirst();
      break;
    case 'End':
      this.selectLast();
      break;
    case 'Up':
    case 'Down':
    case 'Left':
    case 'Right':
      if (!this.isEditing() && this.viewport_.isZoomed()) {
        var delta = SlideMode.KEY_OFFSET_MAP[keyID];
        this.viewport_.setOffset(
            ~~(this.viewport_.getOffsetX() +
               delta[0] * this.viewport_.getZoom()),
            ~~(this.viewport_.getOffsetY() +
               delta[1] * this.viewport_.getZoom()));
        this.touchHandlers_.stopOperation();
        this.imageView_.applyViewportChange();
      } else {
        this.advanceWithKeyboard(keyID);
      }
      break;
    case 'MediaNextTrack':
    case 'MediaPreviousTrack':
      this.advanceWithKeyboard(keyID);
      break;

    case 'Ctrl-U+00BB':  // Ctrl+'=' zoom in.
      if (!this.isEditing()) {
        this.viewport_.zoomIn();
        this.touchHandlers_.stopOperation();
        this.imageView_.applyViewportChange();
      }
      break;

    case 'Ctrl-U+00BD':  // Ctrl+'-' zoom out.
      if (!this.isEditing()) {
        this.viewport_.zoomOut();
        this.touchHandlers_.stopOperation();
        this.imageView_.applyViewportChange();
      }
      break;

    case 'Ctrl-U+0030': // Ctrl+'0' zoom reset.
      if (!this.isEditing()) {
        this.viewport_.setZoom(1.0);
        this.touchHandlers_.stopOperation();
        this.imageView_.applyViewportChange();
      }
      break;
  }

  return true;
};

/**
 * Resize handler.
 * @private
 */
SlideMode.prototype.onResize_ = function() {
  this.viewport_.setScreenSize(
      this.container_.clientWidth, this.container_.clientHeight);
  this.touchHandlers_.stopOperation();
  this.editor_.getBuffer().draw();
};

/**
 * Update thumbnails.
 */
SlideMode.prototype.updateThumbnails = function() {
  this.ribbon_.reset();
  if (this.active_)
    this.ribbon_.redraw();
};

// Saving

/**
 * Save the current image to a file.
 *
 * @param {function} callback Callback.
 * @private
 */
SlideMode.prototype.saveCurrentImage_ = function(callback) {
  this.showSpinner_(true);

  var item = this.getSelectedItem();
  var savedPromise = this.dataModel_.saveItem(
      item,
      this.imageView_.getCanvas(),
      this.shouldOverwriteOriginal_());

  savedPromise.catch(function(error) {
    // TODO(hirono): Implement write error handling.
    // Until then pretend that the save succeeded.
    console.error(error.stack || error);
  }).then(function() {
    this.showSpinner_(false);
    this.flashSavedLabel_();

    // Allow changing the 'Overwrite original' setting only if the user
    // used Undo to restore the original image AND it is not a copy.
    // Otherwise lock the setting in its current state.
    var mayChangeOverwrite = !this.editor_.canUndo() && item.isOriginal();
    ImageUtil.setAttribute(this.options_, 'saved', !mayChangeOverwrite);

    // Record UMA for the first edit.
    if (this.imageView_.getContentRevision() === 1)
      ImageUtil.metrics.recordUserAction(ImageUtil.getMetricName('Edit'));

    callback();
    cr.dispatchSimpleEvent(this, 'image-saved');
  }.bind(this)).catch(function(error) {
    console.error(error.stack || error);
  });
};

/**
 * Flash 'Saved' label briefly to indicate that the image has been saved.
 * @private
 */
SlideMode.prototype.flashSavedLabel_ = function() {
  var setLabelHighlighted =
      ImageUtil.setAttribute.bind(null, this.savedLabel_, 'highlighted');
  setTimeout(setLabelHighlighted.bind(null, true), 0);
  setTimeout(setLabelHighlighted.bind(null, false), 300);
};

/**
 * Local storage key for the 'Overwrite original' setting.
 * @type {string}
 */
SlideMode.OVERWRITE_KEY = 'gallery-overwrite-original';

/**
 * Local storage key for the number of times that
 * the overwrite info bubble has been displayed.
 * @type {string}
 */
SlideMode.OVERWRITE_BUBBLE_KEY = 'gallery-overwrite-bubble';

/**
 * Max number that the overwrite info bubble is shown.
 * @type {number}
 */
SlideMode.OVERWRITE_BUBBLE_MAX_TIMES = 5;

/**
 * @return {boolean} True if 'Overwrite original' is set.
 * @private
 */
SlideMode.prototype.shouldOverwriteOriginal_ = function() {
   return this.overwriteOriginal_.checked;
};

/**
 * 'Overwrite original' checkbox handler.
 * @param {Event} event Event.
 * @private
 */
SlideMode.prototype.onOverwriteOriginalClick_ = function(event) {
  util.platform.setPreference(SlideMode.OVERWRITE_KEY, event.target.checked);
};

/**
 * Overwrite info bubble close handler.
 * @private
 */
SlideMode.prototype.onCloseBubble_ = function() {
  this.bubble_.hidden = true;
  util.platform.setPreference(SlideMode.OVERWRITE_BUBBLE_KEY,
      SlideMode.OVERWRITE_BUBBLE_MAX_TIMES);
};

// Slideshow

/**
 * Slideshow interval in ms.
 */
SlideMode.SLIDESHOW_INTERVAL = 5000;

/**
 * First slideshow interval in ms. It should be shorter so that the user
 * is not guessing whether the button worked.
 */
SlideMode.SLIDESHOW_INTERVAL_FIRST = 1000;

/**
 * Empirically determined duration of the fullscreen toggle animation.
 */
SlideMode.FULLSCREEN_TOGGLE_DELAY = 500;

/**
 * @return {boolean} True if the slideshow is on.
 * @private
 */
SlideMode.prototype.isSlideshowOn_ = function() {
  return this.container_.hasAttribute('slideshow');
};

/**
 * Starts the slideshow.
 * @param {number=} opt_interval First interval in ms.
 * @param {Event=} opt_event Event.
 */
SlideMode.prototype.startSlideshow = function(opt_interval, opt_event) {
  // Reset zoom.
  this.viewport_.resetView();
  this.imageView_.applyViewportChange();

  // Disable touch operation.
  this.touchHandlers_.enabled = false;

  // Set the attribute early to prevent the toolbar from flashing when
  // the slideshow is being started from the mosaic view.
  this.container_.setAttribute('slideshow', 'playing');

  if (this.active_) {
    this.stopEditing_();
  } else {
    // We are in the Mosaic mode. Toggle the mode but remember to return.
    this.leaveAfterSlideshow_ = true;
    this.toggleMode_(this.startSlideshow.bind(
        this, SlideMode.SLIDESHOW_INTERVAL, opt_event));
    return;
  }

  if (opt_event)  // Caused by user action, notify the Gallery.
    cr.dispatchSimpleEvent(this, 'useraction');

  this.fullscreenBeforeSlideshow_ = util.isFullScreen(this.context_.appWindow);
  if (!this.fullscreenBeforeSlideshow_) {
    // Wait until the zoom animation from the mosaic mode is done.
    setTimeout(this.toggleFullScreen_.bind(this),
               ImageView.ZOOM_ANIMATION_DURATION);
    opt_interval = (opt_interval || SlideMode.SLIDESHOW_INTERVAL) +
        SlideMode.FULLSCREEN_TOGGLE_DELAY;
  }

  this.resumeSlideshow_(opt_interval);
};

/**
 * Stops the slideshow.
 * @param {Event=} opt_event Event.
 * @private
 */
SlideMode.prototype.stopSlideshow_ = function(opt_event) {
  if (!this.isSlideshowOn_())
    return;

  if (opt_event)  // Caused by user action, notify the Gallery.
    cr.dispatchSimpleEvent(this, 'useraction');

  this.pauseSlideshow_();
  this.container_.removeAttribute('slideshow');

  // Do not restore fullscreen if we exited fullscreen while in slideshow.
  var fullscreen = util.isFullScreen(this.context_.appWindow);
  var toggleModeDelay = 0;
  if (!this.fullscreenBeforeSlideshow_ && fullscreen) {
    this.toggleFullScreen_();
    toggleModeDelay = SlideMode.FULLSCREEN_TOGGLE_DELAY;
  }
  if (this.leaveAfterSlideshow_) {
    this.leaveAfterSlideshow_ = false;
    setTimeout(this.toggleMode_.bind(this), toggleModeDelay);
  }

  // Re-enable touch operation.
  this.touchHandlers_.enabled = true;
};

/**
 * @return {boolean} True if the slideshow is playing (not paused).
 * @private
 */
SlideMode.prototype.isSlideshowPlaying_ = function() {
  return this.container_.getAttribute('slideshow') === 'playing';
};

/**
 * Pauses/resumes the slideshow.
 * @private
 */
SlideMode.prototype.toggleSlideshowPause_ = function() {
  cr.dispatchSimpleEvent(this, 'useraction');  // Show the tools.
  if (this.isSlideshowPlaying_()) {
    this.pauseSlideshow_();
  } else {
    this.resumeSlideshow_(SlideMode.SLIDESHOW_INTERVAL_FIRST);
  }
};

/**
 * @param {number=} opt_interval Slideshow interval in ms.
 * @private
 */
SlideMode.prototype.scheduleNextSlide_ = function(opt_interval) {
  console.assert(this.isSlideshowPlaying_(), 'Inconsistent slideshow state');

  if (this.slideShowTimeout_)
    clearTimeout(this.slideShowTimeout_);

  this.slideShowTimeout_ = setTimeout(function() {
        this.slideShowTimeout_ = null;
        this.selectNext(1);
      }.bind(this),
      opt_interval || SlideMode.SLIDESHOW_INTERVAL);
};

/**
 * Resumes the slideshow.
 * @param {number=} opt_interval Slideshow interval in ms.
 * @private
 */
SlideMode.prototype.resumeSlideshow_ = function(opt_interval) {
  this.container_.setAttribute('slideshow', 'playing');
  this.scheduleNextSlide_(opt_interval);
};

/**
 * Pauses the slideshow.
 * @private
 */
SlideMode.prototype.pauseSlideshow_ = function() {
  this.container_.setAttribute('slideshow', 'paused');
  if (this.slideShowTimeout_) {
    clearTimeout(this.slideShowTimeout_);
    this.slideShowTimeout_ = null;
  }
};

/**
 * @return {boolean} True if the editor is active.
 */
SlideMode.prototype.isEditing = function() {
  return this.container_.hasAttribute('editing');
};

/**
 * Stops editing.
 * @private
 */
SlideMode.prototype.stopEditing_ = function() {
  if (this.isEditing())
    this.toggleEditor();
};

/**
 * Activate/deactivate editor.
 * @param {Event=} opt_event Event.
 */
SlideMode.prototype.toggleEditor = function(opt_event) {
  if (opt_event)  // Caused by user action, notify the Gallery.
    cr.dispatchSimpleEvent(this, 'useraction');

  if (!this.active_) {
    this.toggleMode_(this.toggleEditor.bind(this));
    return;
  }

  this.stopSlideshow_();

  ImageUtil.setAttribute(this.container_, 'editing', !this.isEditing());

  if (this.isEditing()) { // isEditing has just been flipped to a new value.
    // Reset zoom.
    this.viewport_.resetView();
    this.imageView_.applyViewportChange();
    if (this.context_.readonlyDirName) {
      this.editor_.getPrompt().showAt(
          'top', 'GALLERY_READONLY_WARNING', 0, this.context_.readonlyDirName);
    }
    this.touchHandlers_.enabled = false;
  } else {
    this.editor_.getPrompt().hide();
    this.editor_.leaveModeGently();
    this.touchHandlers_.enabled = true;
  }
};

/**
 * Prints the current item.
 * @private
 */
SlideMode.prototype.print_ = function() {
  cr.dispatchSimpleEvent(this, 'useraction');
  window.print();
};

/**
 * Displays the error banner.
 * @param {string} message Message.
 * @private
 */
SlideMode.prototype.showErrorBanner_ = function(message) {
  if (message) {
    this.errorBanner_.textContent = this.displayStringFunction_(message);
  }
  ImageUtil.setAttribute(this.container_, 'error', !!message);
};

/**
 * Shows/hides the busy spinner.
 *
 * @param {boolean} on True if show, false if hide.
 * @private
 */
SlideMode.prototype.showSpinner_ = function(on) {
  if (this.spinnerTimer_) {
    clearTimeout(this.spinnerTimer_);
    this.spinnerTimer_ = null;
  }

  if (on) {
    this.spinnerTimer_ = setTimeout(function() {
      this.spinnerTimer_ = null;
      ImageUtil.setAttribute(this.container_, 'spinner', true);
    }.bind(this), 1000);
  } else {
    ImageUtil.setAttribute(this.container_, 'spinner', false);
  }
};

/**
 * Apply the change of viewport.
 */
SlideMode.prototype.applyViewportChange = function() {
  this.imageView_.applyViewportChange();
};

/**
 * Touch handlers of the slide mode.
 * @param {DOMElement} targetElement Event source.
 * @param {SlideMode} slideMode Slide mode to be operated by the handler.
 * @constructor
 */
function TouchHandler(targetElement, slideMode) {
  /**
   * Event source.
   * @type {DOMElement}
   */
  this.targetElement_ = targetElement;

  /**
   * Target of touch operations.
   * @type {SlideMode}
   * @private
   */
  this.slideMode_ = slideMode;

  /**
   * Flag to enable/disable touch operation.
   * @type {boolean}
   */
  this.enabled_ = true;

  /**
   * Whether it is in a touch operation that is started from targetElement or
   * not.
   * @type {boolean}
   * @private
   */
  this.touchStarted_ = false;

  /**
   * The swipe action that should happen only once in an operation is already
   * done or not.
   * @type {boolean}
   * @private
   */
  this.done_ = false;

  /**
   * Event on beginning of the current gesture.
   * The variable is updated when the number of touch finger changed.
   * @type {TouchEvent}
   * @private
   */
  this.gestureStartEvent_ = null;

  /**
   * Rotation value on beginning of the current gesture.
   * @type {number}
   * @private
   */
  this.gestureStartRotation_ = 0;

  /**
   * Last touch event.
   * @type {TouchEvent}
   * @private
   */
  this.lastEvent_ = null;

  /**
   * Zoom value just after last touch event.
   * @type {number}
   * @private
   */
  this.lastZoom_ = 1.0;

  targetElement.addEventListener('touchstart', this.onTouchStart_.bind(this));
  var onTouchEventBound = this.onTouchEvent_.bind(this);
  targetElement.ownerDocument.addEventListener('touchmove', onTouchEventBound);
  targetElement.ownerDocument.addEventListener('touchend', onTouchEventBound);
}

/**
 * If the user touched the image and moved the finger more than SWIPE_THRESHOLD
 * horizontally it's considered as a swipe gesture (change the current image).
 * @type {number}
 * @const
 */
TouchHandler.SWIPE_THRESHOLD = 100;

/**
 * Rotation threshold in degrees.
 * @type {number}
 * @const
 */
TouchHandler.ROTATION_THRESHOLD = 25;

/**
 * Obtains distance between fingers.
 * @param {TouchEvent} event Touch event. It should include more than two
 *     touches.
 * @return {boolean} Distance between touch[0] and touch[1].
 */
TouchHandler.getDistance = function(event) {
  var touch1 = event.touches[0];
  var touch2 = event.touches[1];
  var dx = touch1.clientX - touch2.clientX;
  var dy = touch1.clientY - touch2.clientY;
  return Math.sqrt(dx * dx + dy * dy);
};

TouchHandler.prototype = {
  /**
   * @param {boolean} flag New value.
   */
  set enabled(flag) {
    this.enabled_ = flag;
    if (!this.enabled_)
      this.stopOperation();
  }
};

/**
 * Obtains the degrees of the pinch twist angle.
 * @param {TouchEvent} event1 Start touch event. It should include more than two
 *     touches.
 * @param {TouchEvent} event2 Current touch event. It should include more than
 *     two touches.
 * @return {number} Degrees of the pinch twist angle.
 */
TouchHandler.getTwistAngle = function(event1, event2) {
  var dx1 = event1.touches[1].clientX - event1.touches[0].clientX;
  var dy1 = event1.touches[1].clientY - event1.touches[0].clientY;
  var dx2 = event2.touches[1].clientX - event2.touches[0].clientX;
  var dy2 = event2.touches[1].clientY - event2.touches[0].clientY;
  var innerProduct = dx1 * dx2 + dy1 * dy2;  // |v1| * |v2| * cos(t) = x / r
  var outerProduct = dx1 * dy2 - dy1 * dx2;  // |v1| * |v2| * sin(t) = y / r
  return Math.atan2(outerProduct, innerProduct) * 180 / Math.PI;  // atan(y / x)
};

/**
 * Stops the current touch operation.
 */
TouchHandler.prototype.stopOperation = function() {
  this.touchStarted_ = false;
  this.done_ = false;
  this.gestureStartEvent_ = null;
  this.lastEvent_ = null;
  this.lastZoom_ = 1.0;
};

TouchHandler.prototype.onTouchStart_ = function(event) {
  if (this.enabled_ && event.touches.length === 1)
    this.touchStarted_ = true;
};

/**
 * @param {event} event Touch event.
 */
TouchHandler.prototype.onTouchEvent_ = function(event) {
  // Check if the current touch operation started from the target element or
  // not.
  if (!this.touchStarted_)
    return;

  // Check if the current touch operation ends with the event.
  if (event.touches.length === 0) {
    this.stopOperation();
    return;
  }

  // Check if a new gesture started or not.
  var viewport = this.slideMode_.getViewport();
  if (!this.lastEvent_ ||
      this.lastEvent_.touches.length !== event.touches.length) {
    if (event.touches.length === 2 ||
        event.touches.length === 1) {
      this.gestureStartEvent_ = event;
      this.gestureStartRotation_ = viewport.getRotation();
      this.lastEvent_ = event;
      this.lastZoom_ = viewport.getZoom();
    } else {
      this.gestureStartEvent_ = null;
      this.gestureStartRotation_ = 0;
      this.lastEvent_ = null;
      this.lastZoom_ = 1.0;
    }
    return;
  }

  // Handle the gesture movement.
  switch (event.touches.length) {
    case 1:
      if (viewport.isZoomed()) {
        // Scrolling an image by swipe.
        var dx = event.touches[0].screenX - this.lastEvent_.touches[0].screenX;
        var dy = event.touches[0].screenY - this.lastEvent_.touches[0].screenY;
        viewport.setOffset(
            viewport.getOffsetX() + dx, viewport.getOffsetY() + dy);
        this.slideMode_.applyViewportChange();
      } else {
        // Traversing images by swipe.
        if (this.done_)
          break;
        var dx =
            event.touches[0].clientX -
            this.gestureStartEvent_.touches[0].clientX;
        if (dx > TouchHandler.SWIPE_THRESHOLD) {
          this.slideMode_.advanceManually(-1);
          this.done_ = true;
        } else if (dx < -TouchHandler.SWIPE_THRESHOLD) {
          this.slideMode_.advanceManually(1);
          this.done_ = true;
        }
      }
      break;

    case 2:
      // Pinch zoom.
      var distance1 = TouchHandler.getDistance(this.lastEvent_);
      var distance2 = TouchHandler.getDistance(event);
      if (distance1 === 0)
        break;
      var zoom = distance2 / distance1 * this.lastZoom_;
      viewport.setZoom(zoom);

      // Pinch rotation.
      var angle = TouchHandler.getTwistAngle(this.gestureStartEvent_, event);
      if (angle > TouchHandler.ROTATION_THRESHOLD)
        viewport.setRotation(this.gestureStartRotation_ + 1);
      else if (angle < -TouchHandler.ROTATION_THRESHOLD)
        viewport.setRotation(this.gestureStartRotation_ - 1);
      else
        viewport.setRotation(this.gestureStartRotation_);
      this.slideMode_.applyViewportChange();
      break;
  }

  // Update the last event.
  this.lastEvent_ = event;
  this.lastZoom_ = viewport.getZoom();
};
