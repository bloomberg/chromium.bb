// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * PreviewPanel UI class.
 * @param {!Element} element DOM Element of preview panel.
 * @param {PreviewPanelModel.VisibilityType} visibilityType Initial value of the
 *     visibility type.
 * @param {MetadataCache} metadataCache Metadata cache.
 * @param {VolumeManagerWrapper} volumeManager Volume manager.
 * @param {!importer.HistoryLoader} historyLoader
 * @constructor
 * @extends {cr.EventTarget}
 */
var PreviewPanel = function(element,
                            visibilityType,
                            metadataCache,
                            volumeManager,
                            historyLoader) {
  cr.EventTarget.call(this);

  /**
   * The cached height of preview panel.
   * @type {number}
   * @private
   */
  this.height_ = 0;

  /**
   * Visibility type of the preview panel.
   * @type {!PreviewPanelModel}
   * @const
   * @private
   */
  this.model_ = new PreviewPanelModel(visibilityType, [
    util.queryDecoratedElement('#share', cr.ui.Command),
    util.queryDecoratedElement('#cloud-import', cr.ui.Command)
  ]);

  /**
   * Dom element of the preview panel.
   * @type {!Element}
   * @const
   * @private
   */
  this.element_ = element;

  /**
   * @type {!PreviewPanel.Thumbnails}
   * @const
   */
  this.thumbnails = new PreviewPanel.Thumbnails(
      element.querySelector('.preview-thumbnails'),
      metadataCache,
      volumeManager,
      historyLoader);

  /**
   * @type {Element}
   * @private
   */
  this.summaryElement_ = element.querySelector('.preview-summary');

  /**
   * @type {PreviewPanel.CalculatingSizeLabel}
   * @private
   */
  this.calculatingSizeLabel_ = new PreviewPanel.CalculatingSizeLabel(
      this.summaryElement_.querySelector('.calculating-size'));

  /**
   * @type {Element}
   * @private
   */
  this.previewText_ = element.querySelector('.preview-text');

  /**
   * FileSelection to be displayed.
   * @type {FileSelection}
   * @private
   */
  this.selection_ = /** @type {FileSelection} */
      ({entries: [], computeBytes: function() {}, totalCount: 0});

  /**
   * @type {!PromiseSlot}
   * @const
   * @private
   */
  this.visibilityPromiseSlot_ = new PromiseSlot(function(visible) {
    if (this.element_.getAttribute('visibility') ===
        PreviewPanel.Visibility_.HIDING) {
      this.element_.setAttribute('visibility', PreviewPanel.Visibility_.HIDDEN);
    }
    cr.dispatchSimpleEvent(this, PreviewPanel.Event.VISIBILITY_CHANGE);
  }.bind(this), function(error) { console.error(error.stack || error); });

  /**
   * Sequence value that is incremented by every selection update and is used to
   * check if the callback is up to date or not.
   * @type {number}
   * @private
   */
  this.sequence_ = 0;

  /**
   * @type {VolumeManagerWrapper}
   * @private
   */
  this.volumeManager_ = volumeManager;

  cr.EventTarget.call(this);
  this.model_.addEventListener(
      PreviewPanelModel.EventType.CHANGE, this.onModelChanged_.bind(this));
  this.onModelChanged_();
};

/**
 * Name of PreviewPanels's event.
 * @enum {string}
 * @const
 */
PreviewPanel.Event = {
  // Event to be triggered at the end of visibility change.
  VISIBILITY_CHANGE: 'visibilityChange'
};

/**
 * @enum {string}
 * @const
 */
PreviewPanel.Visibility_ = {
  VISIBLE: 'visible',
  HIDING: 'hiding',
  HIDDEN: 'hidden'
};

PreviewPanel.prototype = {
  __proto__: cr.EventTarget.prototype,

  /**
   * Setter for the visibility type.
   * @param {PreviewPanelModel.VisibilityType} visibilityType New value of
   *     visibility type.
   */
  set visibilityType(visibilityType) {
    this.model_.setVisibilityType(visibilityType);
  },

  get visible() {
    return this.element_.getAttribute('visibility') ==
        PreviewPanel.Visibility_.VISIBLE;
  },

  /**
   * Obtains the height of preview panel.
   * @return {number} Height of preview panel.
   */
  get height() {
    this.height_ = this.height_ || this.element_.clientHeight;
    return this.height_;
  }
};

/**
 * Apply the selection and update the view of the preview panel.
 * @param {FileSelection} selection Selection to be applied.
 */
PreviewPanel.prototype.setSelection = function(selection) {
  this.sequence_++;
  this.selection_ = selection;
  this.model_.setSelection(selection);
  this.updatePreviewArea_();
};

/**
 * webkitTransitionEnd does not always fire (e.g. when animation is aborted or
 * when no paint happens during the animation).  This function sets up a timer
 * and call the fulfill callback of the returned promise when the timer expires.
 * @private
 */
PreviewPanel.prototype.waitForTransitionEnd_ = function() {
  // Keep this sync with CSS.
  var PREVIEW_PANEL_TRANSITION_MS = 220;

  return new Promise(function(fulfill) {
    var timeoutId;
    var onTransitionEnd = function(event) {
      if (event &&
          (event.target !== this.element_ ||
           event.propertyName !== 'opacity')) {
        return;
      }
      this.element_.removeEventListener('webkitTransitionEnd', onTransitionEnd);
      clearTimeout(timeoutId);
      fulfill();
    }.bind(this);
    this.element_.addEventListener('webkitTransitionEnd', onTransitionEnd);
    timeoutId = setTimeout(onTransitionEnd, PREVIEW_PANEL_TRANSITION_MS);
  }.bind(this));
};

/**
 * Handles the model change event and update the visibility of the preview
 * panel.
 * @private
 */
PreviewPanel.prototype.onModelChanged_ = function() {
  var promise;
  if (this.model_.visible) {
    this.element_.setAttribute(
        'visibility', PreviewPanel.Visibility_.VISIBLE);
    this.updatePreviewArea_();
    promise = Promise.resolve();
  } else {
    this.element_.setAttribute('visibility', PreviewPanel.Visibility_.HIDING);
    promise = this.waitForTransitionEnd_();
  }
  this.visibilityPromiseSlot_.setPromise(promise);
};

/**
 * Update the text in the preview panel.
 * @private
 */
PreviewPanel.prototype.updatePreviewArea_ = function() {
  // If the preview panel is hiding, does not update the current view.
  if (!this.visible)
    return;
  var selection = this.selection_;

  // If no item is selected, no information is displayed on the footer.
  if (selection.totalCount === 0) {
    this.thumbnails.hidden = true;
    this.calculatingSizeLabel_.hidden = true;
    this.previewText_.textContent = '';
    return;
  }

  // If one item is selected, show thumbnail and entry name of the item.
  if (selection.totalCount === 1) {
    this.thumbnails.hidden = false;
    this.thumbnails.selection = selection;
    this.calculatingSizeLabel_.hidden = true;
    this.previewText_.textContent = util.getEntryLabel(
        this.volumeManager_.getLocationInfo(selection.entries[0]),
        selection.entries[0]);
    return;
  }

  // Update thumbnails.
  this.thumbnails.hidden = false;
  this.thumbnails.selection = selection;

  // Obtains the preview text.
  var text;
  if (selection.directoryCount == 0)
    text = strf('MANY_FILES_SELECTED', selection.fileCount);
  else if (selection.fileCount == 0)
    text = strf('MANY_DIRECTORIES_SELECTED', selection.directoryCount);
  else
    text = strf('MANY_ENTRIES_SELECTED', selection.totalCount);

  // Obtains the size of files.
  this.calculatingSizeLabel_.hidden = selection.bytesKnown;
  if (selection.bytesKnown && selection.showBytes)
    text += ', ' + util.bytesToString(selection.bytes);

  // Set the preview text to the element.
  this.previewText_.textContent = text;

  // Request the byte calculation if needed.
  if (!selection.bytesKnown) {
    this.selection_.computeBytes(function(sequence) {
      // Selection has been already updated.
      if (this.sequence_ != sequence)
        return;
      this.updatePreviewArea_();
    }.bind(this, this.sequence_));
  }
};

/**
 * Animating label that is shown during the bytes of selection entries is being
 * calculated.
 *
 * This label shows dots and varying the number of dots every
 * CalculatingSizeLabel.PERIOD milliseconds.
 * @param {Element} element DOM element of the label.
 * @constructor
 */
PreviewPanel.CalculatingSizeLabel = function(element) {
  this.element_ = element;
  this.count_ = 0;
  this.intervalID_ = 0;
  Object.seal(this);
};

/**
 * Time period in milliseconds.
 * @const {number}
 */
PreviewPanel.CalculatingSizeLabel.PERIOD = 500;

PreviewPanel.CalculatingSizeLabel.prototype = {
  /**
   * Set visibility of the label.
   * When it is displayed, the text is animated.
   * @param {boolean} hidden Whether to hide the label or not.
   */
  set hidden(hidden) {
    this.element_.hidden = hidden;
    if (!hidden) {
      if (this.intervalID_ !== 0)
        return;
      this.count_ = 2;
      this.intervalID_ =
          setInterval(this.onStep_.bind(this),
                      PreviewPanel.CalculatingSizeLabel.PERIOD);
      this.onStep_();
    } else {
      if (this.intervalID_ === 0)
        return;
      clearInterval(this.intervalID_);
      this.intervalID_ = 0;
    }
  }
};

/**
 * Increments the counter and updates the number of dots.
 * @private
 */
PreviewPanel.CalculatingSizeLabel.prototype.onStep_ = function() {
  var text = str('CALCULATING_SIZE');
  for (var i = 0; i < ~~(this.count_ / 2) % 4; i++) {
    text += '.';
  }
  this.element_.textContent = text;
  this.count_++;
};

/**
 * Thumbnails on the preview panel.
 *
 * @param {Element} element DOM Element of thumbnail container.
 * @param {MetadataCache} metadataCache MetadataCache.
 * @param {VolumeManagerWrapper} volumeManager Volume manager instance.
 * @param {!importer.HistoryLoader} historyLoader
 * @constructor
 */
PreviewPanel.Thumbnails = function(
    element, metadataCache, volumeManager, historyLoader) {

  /** @private {Element} */
  this.element_ = element;

  /** @private {MetadataCache} */
  this.metadataCache_ = metadataCache;

  /** @private {VolumeManagerWrapper} */
  this.volumeManager_ = volumeManager;

  /** @private {!importer.HistoryLoader} */
  this.historyLoader_ = historyLoader;

  /** @private {string} */
  this.lastEntriesHash_ = '';

  Object.seal(this);
};

/**
 * Maximum number of thumbnails.
 * @const {number}
 */
PreviewPanel.Thumbnails.MAX_THUMBNAIL_COUNT = 4;

/**
 * Edge length of the thumbnail square.
 * @const {number}
 */
PreviewPanel.Thumbnails.THUMBNAIL_SIZE = 35;

/**
 * Longer edge length of zoomed thumbnail rectangle.
 * @const {number}
 */
PreviewPanel.Thumbnails.ZOOMED_THUMBNAIL_SIZE = 200;

PreviewPanel.Thumbnails.prototype = {
  /**
   * Sets entries to be displayed in the view.
   * @param {FileSelection} value Entries.
   */
  set selection(value) {
    this.loadThumbnails_(value);
  },

  /**
   * Set visibility of the thumbnails.
   * @param {boolean} value Whether to hide the thumbnails or not.
   */
  set hidden(value) {
    this.element_.hidden = value;
  }
};

/**
 * Loads thumbnail images.
 * @param {FileSelection} selection Selection containing entries that are
 *     sources of images.
 * @private
 */
PreviewPanel.Thumbnails.prototype.loadThumbnails_ = function(selection) {
  var entries = selection.entries;
  var length = Math.min(
      entries.length, PreviewPanel.Thumbnails.MAX_THUMBNAIL_COUNT);
  var clickHandler = selection.tasks &&
      selection.tasks.executeDefault.bind(selection.tasks);
  var hash = selection.entries.
      slice(0, PreviewPanel.Thumbnails.MAX_THUMBNAIL_COUNT).
      map(function(entry) { return entry.toURL(); }).
      sort().
      join('\n');
  var entrySetChanged = hash !== this.lastEntriesHash_;

  this.lastEntriesHash_ = hash;
  this.element_.classList.remove('has-zoom');

  // Create new thumbnail image if the selection is changed.
  var boxList;
  if (entrySetChanged) {
    this.element_.innerText = '';
    boxList = [];
    for (var i = 0; i < length; i++) {
      // Create a box.
      var box = this.element_.ownerDocument.createElement('div');
      box.style.zIndex = PreviewPanel.Thumbnails.MAX_THUMBNAIL_COUNT + 1 - i;

      // Register the click handler.
      if (clickHandler) {
        box.addEventListener('click', function(event) {
          clickHandler();
        });
      }

      // Append
      this.element_.appendChild(box);
      boxList.push(box);
    }
  } else {
    boxList = this.element_.querySelectorAll('.img-container');
    assert(length === boxList.length);
  }

  // Update images in the boxes.
  for (var i = 0; i < length; i++) {
    // Load the image.
    if (entries[i]) {
      FileGrid.decorateThumbnailBox(
          boxList[i],
          entries[i],
          this.metadataCache_,
          this.volumeManager_,
          this.historyLoader_,
          ThumbnailLoader.FillMode.FILL,
          FileGrid.ThumbnailQuality.LOW,
          /* animation */ entrySetChanged,
          i == 0 && length == 1 ? this.setZoomedImage_.bind(this) : undefined);
    }
  }
};

/**
 * Create the zoomed version of image and set it to the DOM element to show the
 * zoomed image.
 *
 * @param {HTMLImageElement} image Image to be source of the zoomed image.
 * @param {util.Transform=} opt_transform Transformation to be applied to the
 *     image.
 * @private
 */
PreviewPanel.Thumbnails.prototype.setZoomedImage_ = function(image,
                                                             opt_transform) {
  if (!image)
    return;
  var width = image.width || 0;
  var height = image.height || 0;
  if (width == 0 ||
      height == 0 ||
      (width < PreviewPanel.Thumbnails.THUMBNAIL_SIZE * 2 &&
       height < PreviewPanel.Thumbnails.THUMBNAIL_SIZE * 2))
    return;

  var scale = Math.min(1,
                       PreviewPanel.Thumbnails.ZOOMED_THUMBNAIL_SIZE /
                           Math.max(width, height));
  var imageWidth = ~~(width * scale);
  var imageHeight = ~~(height * scale);
  var zoomedBox =
      this.element_.querySelector('.popup') ||
      this.element_.ownerDocument.createElement('div');
  var zoomedImage =
      this.element_.querySelector('.popup img') ||
      this.element_.ownerDocument.createElement('img');

  if (scale < 0.3) {
    // Scaling large images kills animation. Downscale it in advance.
    // Canvas scales images with liner interpolation. Make a larger
    // image (but small enough to not kill animation) and let IMAGE
    // scale it smoothly.
    var INTERMEDIATE_SCALE = 3;
    var canvas = this.element_.ownerDocument.createElement('canvas');
    canvas.width = imageWidth * INTERMEDIATE_SCALE;
    canvas.height = imageHeight * INTERMEDIATE_SCALE;
    var ctx = canvas.getContext('2d');
    ctx.drawImage(image, 0, 0, canvas.width, canvas.height);
    // Using bigger than default compression reduces image size by
    // several times. Quality degradation compensated by greater resolution.
    zoomedImage.src = canvas.toDataURL('image/jpeg', 0.6);
  } else {
    zoomedImage.src = image.src;
  }

  var boxWidth = Math.max(PreviewPanel.Thumbnails.THUMBNAIL_SIZE, imageWidth);
  var boxHeight = Math.max(PreviewPanel.Thumbnails.THUMBNAIL_SIZE, imageHeight);
  if (opt_transform && opt_transform.rotate90 % 2 == 1) {
    var t = boxWidth;
    boxWidth = boxHeight;
    boxHeight = t;
  }

  if (opt_transform)
    util.applyTransform(zoomedImage, opt_transform);

  zoomedBox.className = 'popup';
  zoomedBox.style.width = boxWidth + 'px';
  zoomedBox.style.height = boxHeight + 'px';
  zoomedBox.appendChild(zoomedImage);

  this.element_.appendChild(zoomedBox);
  this.element_.classList.add('has-zoom');
  return;
};
