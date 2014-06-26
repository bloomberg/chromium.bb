// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * PreviewPanel UI class.
 * @param {HTMLElement} element DOM Element of preview panel.
 * @param {PreviewPanel.VisibilityType} visibilityType Initial value of the
 *     visibility type.
 * @param {MetadataCache} metadataCache Metadata cache.
 * @param {VolumeManagerWrapper} volumeManager Volume manager.
 * @constructor
 * @extends {cr.EventTarget}
 */
var PreviewPanel = function(element,
                            visibilityType,
                            metadataCache,
                            volumeManager) {
  /**
   * The cached height of preview panel.
   * @type {number}
   * @private
   */
  this.height_ = 0;

  /**
   * Visibility type of the preview panel.
   * @type {PreviewPanel.VisiblityType}
   * @private
   */
  this.visibilityType_ = visibilityType;

  /**
   * Current entry to be displayed.
   * @type {Entry}
   * @private
   */
  this.currentEntry_ = null;

  /**
   * Dom element of the preview panel.
   * @type {HTMLElement}
   * @private
   */
  this.element_ = element;

  /**
   * @type {BreadcrumbsController}
   */
  this.breadcrumbs = new BreadcrumbsController(
      element.querySelector('#search-breadcrumbs'),
      metadataCache,
      volumeManager);

  /**
   * @type {PreviewPanel.Thumbnails}
   */
  this.thumbnails = new PreviewPanel.Thumbnails(
      element.querySelector('.preview-thumbnails'),
      metadataCache,
      volumeManager);

  /**
   * @type {HTMLElement}
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
   * @type {HTMLElement}
   * @private
   */
  this.previewText_ = element.querySelector('.preview-text');

  /**
   * FileSelection to be displayed.
   * @type {FileSelection}
   * @private
   */
  this.selection_ = {entries: [], computeBytes: function() {}};

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
};

/**
 * Name of PreviewPanels's event.
 * @enum {string}
 * @const
 */
PreviewPanel.Event = Object.freeze({
  // Event to be triggered at the end of visibility change.
  VISIBILITY_CHANGE: 'visibilityChange'
});

/**
 * Visibility type of the preview panel.
 */
PreviewPanel.VisibilityType = Object.freeze({
  // Preview panel always shows.
  ALWAYS_VISIBLE: 'alwaysVisible',
  // Preview panel shows when the selection property are set.
  AUTO: 'auto',
  // Preview panel does not show.
  ALWAYS_HIDDEN: 'alwaysHidden'
});

/**
 * @private
 */
PreviewPanel.Visibility_ = Object.freeze({
  VISIBLE: 'visible',
  HIDING: 'hiding',
  HIDDEN: 'hidden'
});

PreviewPanel.prototype = {
  __proto__: cr.EventTarget.prototype,

  /**
   * Setter for the current entry.
   * @param {Entry} entry New entry.
   */
  set currentEntry(entry) {
    if (util.isSameEntry(this.currentEntry_, entry))
      return;
    this.currentEntry_ = entry;
    this.updateVisibility_();
    this.updatePreviewArea_();
  },

  /**
   * Setter for the visibility type.
   * @param {PreviewPanel.VisibilityType} visibilityType New value of visibility
   *     type.
   */
  set visibilityType(visibilityType) {
    this.visibilityType_ = visibilityType;
    this.updateVisibility_();
    // Also update the preview area contents, because the update is suppressed
    // while the visibility is hiding or hidden.
    this.updatePreviewArea_();
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
 * Initializes the element.
 */
PreviewPanel.prototype.initialize = function() {
  this.element_.addEventListener('webkitTransitionEnd',
                                 this.onTransitionEnd_.bind(this));
  this.updateVisibility_();
  // Also update the preview area contents, because the update is suppressed
  // while the visibility is hiding or hidden.
  this.updatePreviewArea_();
};

/**
 * Apply the selection and update the view of the preview panel.
 * @param {FileSelection} selection Selection to be applied.
 */
PreviewPanel.prototype.setSelection = function(selection) {
  this.sequence_++;
  this.selection_ = selection;
  this.updateVisibility_();
  this.updatePreviewArea_();
};

/**
 * Update the visibility of the preview panel.
 * @private
 */
PreviewPanel.prototype.updateVisibility_ = function() {
  // Get the new visibility value.
  var visibility = this.element_.getAttribute('visibility');
  var newVisible = null;
  switch (this.visibilityType_) {
    case PreviewPanel.VisibilityType.ALWAYS_VISIBLE:
      newVisible = true;
      break;
    case PreviewPanel.VisibilityType.AUTO:
      newVisible =
          this.selection_.entries.length !== 0 ||
          (this.currentEntry_ &&
           this.volumeManager_.getLocationInfo(this.currentEntry_) &&
           !this.volumeManager_.getLocationInfo(
               this.currentEntry_).isRootEntry);
      break;
    case PreviewPanel.VisibilityType.ALWAYS_HIDDEN:
      newVisible = false;
      break;
    default:
      console.error('Invalid visibilityType.');
      return;
  }

  // If the visibility has been already the new value, just return.
  if ((visibility == PreviewPanel.Visibility_.VISIBLE && newVisible) ||
      (visibility == PreviewPanel.Visibility_.HIDDEN && !newVisible))
    return;

  // Set the new visibility value.
  if (newVisible) {
    this.element_.setAttribute('visibility', PreviewPanel.Visibility_.VISIBLE);
    cr.dispatchSimpleEvent(this, PreviewPanel.Event.VISIBILITY_CHANGE);
  } else {
    this.element_.setAttribute('visibility', PreviewPanel.Visibility_.HIDING);
  }
};

/**
 * Update the text in the preview panel.
 *
 * @param {boolean} breadCrumbsVisible Whether the bread crumbs is visible or
 *     not.
 * @private
 */
PreviewPanel.prototype.updatePreviewArea_ = function(breadCrumbsVisible) {
  // If the preview panel is hiding, does not update the current view.
  if (!this.visible)
    return;
  var selection = this.selection_;

  // Update thumbnails.
  this.thumbnails.selection = selection.totalCount !== 0 ?
      selection : {entries: [this.currentEntry_]};

  // Check if the breadcrumb list should show instead on the preview text.
  var entry;
  if (this.selection_.totalCount == 1)
    entry = this.selection_.entries[0];
  else if (this.selection_.totalCount == 0)
    entry = this.currentEntry_;

  if (entry) {
    this.breadcrumbs.show(entry);
    this.calculatingSizeLabel_.hidden = true;
    this.previewText_.textContent = '';
    return;
  }
  this.breadcrumbs.hide();

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
 * Event handler to be called at the end of hiding transition.
 * @param {Event} event The webkitTransitionEnd event.
 * @private
 */
PreviewPanel.prototype.onTransitionEnd_ = function(event) {
  if (event.target != this.element_ || event.propertyName != 'opacity')
    return;
  var visibility = this.element_.getAttribute('visibility');
  if (visibility != PreviewPanel.Visibility_.HIDING)
    return;
  this.element_.setAttribute('visibility', PreviewPanel.Visibility_.HIDDEN);
  cr.dispatchSimpleEvent(this, PreviewPanel.Event.VISIBILITY_CHANGE);
};

/**
 * Animating label that is shown during the bytes of selection entries is being
 * calculated.
 *
 * This label shows dots and varying the number of dots every
 * CalculatingSizeLabel.PERIOD milliseconds.
 * @param {HTMLElement} element DOM element of the label.
 * @constructor
 */
PreviewPanel.CalculatingSizeLabel = function(element) {
  this.element_ = element;
  this.count_ = 0;
  this.intervalID_ = null;
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
      if (this.intervalID_ != null)
        return;
      this.count_ = 2;
      this.intervalID_ =
          setInterval(this.onStep_.bind(this),
                      PreviewPanel.CalculatingSizeLabel.PERIOD);
      this.onStep_();
    } else {
      if (this.intervalID_ == null)
        return;
      clearInterval(this.intervalID_);
      this.intervalID_ = null;
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
 * @param {HTMLElement} element DOM Element of thumbnail container.
 * @param {MetadataCache} metadataCache MetadataCache.
 * @param {VolumeManagerWrapper} volumeManager Volume manager instance.
 * @constructor
 */
PreviewPanel.Thumbnails = function(element, metadataCache, volumeManager) {
  this.element_ = element;
  this.metadataCache_ = metadataCache;
  this.volumeManager_ = volumeManager;
  this.sequence_ = 0;
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
   * @param {Array.<Entry>} value Entries.
   */
  set selection(value) {
    this.sequence_++;
    this.loadThumbnails_(value);
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
  this.element_.classList.remove('has-zoom');
  this.element_.innerText = '';
  var clickHandler = selection.tasks &&
      selection.tasks.executeDefault.bind(selection.tasks);
  var length = Math.min(entries.length,
                        PreviewPanel.Thumbnails.MAX_THUMBNAIL_COUNT);
  for (var i = 0; i < length; i++) {
    // Create a box.
    var box = this.element_.ownerDocument.createElement('div');
    box.style.zIndex = PreviewPanel.Thumbnails.MAX_THUMBNAIL_COUNT + 1 - i;

    // Load the image.
    if (entries[i]) {
      FileGrid.decorateThumbnailBox(box,
                                    entries[i],
                                    this.metadataCache_,
                                    this.volumeManager_,
                                    ThumbnailLoader.FillMode.FILL,
                                    FileGrid.ThumbnailQuality.LOW,
                                    i == 0 && length == 1 &&
                                        this.setZoomedImage_.bind(this));
    }

    // Register the click handler.
    if (clickHandler)
      box.addEventListener('click', clickHandler);

    // Append
    this.element_.appendChild(box);
  }
};

/**
 * Create the zoomed version of image and set it to the DOM element to show the
 * zoomed image.
 *
 * @param {Image} image Image to be source of the zoomed image.
 * @param {transform} transform Transformation to be applied to the image.
 * @private
 */
PreviewPanel.Thumbnails.prototype.setZoomedImage_ = function(image, transform) {
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
  var zoomedImage = this.element_.ownerDocument.createElement('img');

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
  if (transform && transform.rotate90 % 2 == 1) {
    var t = boxWidth;
    boxWidth = boxHeight;
    boxHeight = t;
  }

  util.applyTransform(zoomedImage, transform);

  var zoomedBox = this.element_.ownerDocument.createElement('div');
  zoomedBox.className = 'popup';
  zoomedBox.style.width = boxWidth + 'px';
  zoomedBox.style.height = boxHeight + 'px';
  zoomedBox.appendChild(zoomedImage);

  this.element_.appendChild(zoomedBox);
  this.element_.classList.add('has-zoom');
  return;
};
