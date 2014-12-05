// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * PreviewPanel UI class.
 * @param {Element} element DOM Element of preview panel.
 * @param {PreviewPanel.VisibilityType} visibilityType Initial value of the
 *     visibility type.
 * @param {MetadataCache} metadataCache Metadata cache.
 * @param {VolumeManagerWrapper} volumeManager Volume manager.
 * @param {!importer.HistoryLoader} historyLoader
 * @param {function(string): boolean} commandEnabledTest A function
 *     that returns true if the named command is enabled.
 * @constructor
 * @extends {cr.EventTarget}
 */
var PreviewPanel = function(element,
                            visibilityType,
                            metadataCache,
                            volumeManager,
                            historyLoader,
                            commandEnabledTest) {
  /**
   * The cached height of preview panel.
   * @type {number}
   * @private
   */
  this.height_ = 0;

  /**
   * Visibility type of the preview panel.
   * @type {PreviewPanel.VisibilityType}
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
   * @type {Element}
   * @private
   */
  this.element_ = element;

  /**
   * @type {PreviewPanel.Thumbnails}
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

  /**
   * List of command ids that are should be checked when determining
   * auto-visibility.
   *
   * @private {Array.<string>}
   */
  this.autoVisibilityCommandIds_ = [];

  /**
   * A function that returns true if a named command is enabled.
   * This is used when determining visibility of the preview panel.
   * @private {function(string): boolean}
   */
  this.commandEnabled_ = commandEnabledTest;

  cr.EventTarget.call(this);
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
Object.freeze(PreviewPanel.Event);

/**
 * Visibility type of the preview panel.
 * @enum {string}
 * @const
 */
PreviewPanel.VisibilityType = {
  // Preview panel always shows.
  ALWAYS_VISIBLE: 'alwaysVisible',
  // Preview panel shows when the selection property are set.
  AUTO: 'auto',
  // Preview panel does not show.
  ALWAYS_HIDDEN: 'alwaysHidden'
};
Object.freeze(PreviewPanel.VisibilityType);

/**
 * @enum {string}
 * @const
 * @private
 */
PreviewPanel.Visibility_ = {
  VISIBLE: 'visible',
  HIDING: 'hiding',
  HIDDEN: 'hidden'
};
Object.freeze(PreviewPanel.Visibility_);

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

  this.autoVisibilityCommandIds_ = this.findAutoVisibilityCommandIds_();
  this.updateVisibility_();
  // Also update the preview area contents, because the update is suppressed
  // while the visibility is hiding or hidden.
  this.updatePreviewArea_();
};

/**
 * @return {Array.<string>} List of command ids for the "AUTO" visibility type
 * (which currently happen to correspond to "full-page" commands).
 * @private
 */
PreviewPanel.prototype.findAutoVisibilityCommandIds_ = function() {
  if (this.visibilityType_ != PreviewPanel.VisibilityType.AUTO) {
    return [];
  }
  // Find all relevent command elements. Convert the resulting NodeList
  // to an Array.
  var elements = Array.prototype.slice.call(
      this.element_.querySelectorAll('div[class~=buttonbar] button[command]'));

  return elements.map(
      function(e) {
        // We can assume that the command attribute starts with '#';
        return e.getAttribute('command').substring(1);
      });
};

/**
 * @return {boolean} True if one of the known "auto visibility"
 *     (non-dialog mode) commands is enabled.
 * @private
 */
PreviewPanel.prototype.hasEnabledAutoVisibilityCommand_ = function() {
  return this.autoVisibilityCommandIds_.some(
      this.commandEnabled_.bind(this));
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
      newVisible = this.selection_.entries.length !== 0 ||
          this.hasEnabledAutoVisibilityCommand_();
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
  if (opt_transform && opt_transform.rotate90 % 2 == 1) {
    var t = boxWidth;
    boxWidth = boxHeight;
    boxHeight = t;
  }

  if (opt_transform)
    util.applyTransform(zoomedImage, opt_transform);

  var zoomedBox = this.element_.ownerDocument.createElement('div');
  zoomedBox.className = 'popup';
  zoomedBox.style.width = boxWidth + 'px';
  zoomedBox.style.height = boxHeight + 'px';
  zoomedBox.appendChild(zoomedImage);

  this.element_.appendChild(zoomedBox);
  this.element_.classList.add('has-zoom');
  return;
};
