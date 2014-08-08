// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Called from the main frame when unloading.
 * @param {boolean=} opt_exiting True if the app is exiting.
 */
function unload(opt_exiting) { Gallery.instance.onUnload(opt_exiting); }

/**
 * Overrided metadata worker's path.
 * @type {string}
 * @const
 */
ContentProvider.WORKER_SCRIPT = '/js/metadata_worker.js';

/**
 * Data model for gallery.
 *
 * @constructor
 * @extends {cr.ui.ArrayDataModel}
 */
function GalleryDataModel() {
  cr.ui.ArrayDataModel.call(this, []);
  this.metadataCache_ = null;
}

/**
 * Maximum number of full size image cache.
 * @type {number}
 * @const
 * @private
 */
GalleryDataModel.MAX_FULL_IMAGE_CACHE_ = 3;

/**
 * Maximum number of screen size image cache.
 * @type {number}
 * @const
 * @private
 */
GalleryDataModel.MAX_SCREEN_IMAGE_CACHE_ = 5;

GalleryDataModel.prototype = {
  __proto__: cr.ui.ArrayDataModel.prototype
};

/**
 * Initializes the data model.
 *
 * @param {MetadataCache} metadataCache Metadata cache.
 * @param {Array.<FileEntry>} entries Image entries.
 * @return {Promise} Promise to be fulfilled with after initialization.
 */
GalleryDataModel.prototype.initialize = function(metadataCache, entries) {
  // Store metadata cache.
  this.metadataCache_ = metadataCache;

  // Obtain metadata.
  var metadataPromise = new Promise(function(fulfill) {
    this.metadataCache_.get(entries, Gallery.METADATA_TYPE, fulfill);
  }.bind(this));

  // Initialize the gallery by using the metadata.
  return metadataPromise.then(function(metadata) {
    // Check the length of metadata.
    if (entries.length !== metadata.length)
      return Promise.reject('Failed to obtain metadata for the entries.');

    // Obtains items.
    var items = entries.map(function(entry, i) {
      var clonedMetadata = MetadataCache.cloneMetadata(metadata[i]);
      return new Gallery.Item(
          entry, clonedMetadata, metadataCache, /* original */ true);
    });

    // Update the models.
    this.push.apply(this, items);
  }.bind(this));
};

/**
 * Saves new image.
 *
 * @param {Gallery.Item} item Original gallery item.
 * @param {Canvas} canvas Canvas containing new image.
 * @param {boolean} overwrite Whether to overwrite the image to the item or not.
 * @return {Promise} Promise to be fulfilled with when the operation completes.
 */
GalleryDataModel.prototype.saveItem = function(item, canvas, overwrite) {
  var oldEntry = item.getEntry();
  var oldMetadata = item.getMetadata();
  var metadataEncoder = ImageEncoder.encodeMetadata(
      item.getMetadata(), canvas, 1 /* quality */);
  var newMetadata = ContentProvider.ConvertContentMetadata(
      metadataEncoder.getMetadata(),
      MetadataCache.cloneMetadata(item.getMetadata()));
  if (newMetadata.filesystem)
    newMetadata.filesystem.modificationTime = new Date();
  if (newMetadata.drive)
    newMetadata.drive.present = true;

  return new Promise(function(fulfill, reject) {
    item.saveToFile(
        null,
        overwrite,
        canvas,
        metadataEncoder,
        function(success) {
          if (!success) {
            reject('Failed to save the image.');
            return;
          }

          // The item's entry is updated to the latest entry. Update metadata.
          item.setMetadata(newMetadata);

          // Current entry is updated.
          // Dispatch an event.
          var event = new Event('content');
          event.item = item;
          event.oldEntry = oldEntry;
          event.metadata = newMetadata;
          this.dispatchEvent(event);

          if (util.isSameEntry(oldEntry, item.getEntry())) {
            // Need an update of metdataCache.
            this.metadataCache_.set(
                item.getEntry(),
                Gallery.METADATA_TYPE,
                newMetadata);
          } else {
            // New entry is added and the item now tracks it.
            // Add another item for the old entry.
            var anotherItem = new Gallery.Item(
                oldEntry, oldMetadata, this.metadataCache_, item.isOriginal());
            // The item must be added behind the existing item so that it does
            // not change the index of the existing item.
            // TODO(hirono): Update the item index of the selection model
            // correctly.
            this.splice(this.indexOf(item) + 1, 0, anotherItem);
          }

          fulfill();
        }.bind(this));
  }.bind(this));
};

/**
 * Evicts image caches in the items.
 * @param {Gallery.Item} currentSelectedItem Current selected item.
 */
GalleryDataModel.prototype.evictCache = function(currentSelectedItem) {
  // Sort the item by the last accessed date.
  var sorted = this.slice().sort(function(a, b) {
    return b.getLastAccessedDate() - a.getLastAccessedDate();
  });

  // Evict caches.
  var contentCacheCount = 0;
  var screenCacheCount = 0;
  for (var i = 0; i < sorted.length; i++) {
    if (sorted[i].contentImage) {
      if (++contentCacheCount > GalleryDataModel.MAX_FULL_IMAGE_CACHE_) {
        if (sorted[i].contentImage.parentNode) {
          console.error('The content image has a parent node.');
        } else {
          // Force to free the buffer of the canvas by assigning zero size.
          sorted[i].contentImage.width = 0;
          sorted[i].contentImage.height = 0;
          sorted[i].contentImage = null;
        }
      }
    }
    if (sorted[i].screenImage) {
      if (++screenCacheCount > GalleryDataModel.MAX_SCREEN_IMAGE_CACHE_) {
        if (sorted[i].screenImage.parentNode) {
          console.error('The screen image has a parent node.');
        } else {
          // Force to free the buffer of the canvas by assigning zero size.
          sorted[i].screenImage.width = 0;
          sorted[i].screenImage.height = 0;
          sorted[i].screenImage = null;
        }
      }
    }
  }
};

/**
 * Gallery for viewing and editing image files.
 *
 * @param {!VolumeManager} volumeManager The VolumeManager instance of the
 *     system.
 * @constructor
 */
function Gallery(volumeManager) {
  this.context_ = {
    appWindow: chrome.app.window.current(),
    onClose: function() { close(); },
    onMaximize: function() {
      var appWindow = chrome.app.window.current();
      if (appWindow.isMaximized())
        appWindow.restore();
      else
        appWindow.maximize();
    },
    onMinimize: function() { chrome.app.window.current().minimize(); },
    onAppRegionChanged: function() {},
    metadataCache: MetadataCache.createFull(volumeManager),
    readonlyDirName: '',
    displayStringFunction: function() { return ''; },
    loadTimeData: {}
  };
  this.container_ = document.querySelector('.gallery');
  this.document_ = document;
  this.metadataCache_ = this.context_.metadataCache;
  this.volumeManager_ = volumeManager;
  this.selectedEntry_ = null;
  this.metadataCacheObserverId_ = null;
  this.onExternallyUnmountedBound_ = this.onExternallyUnmounted_.bind(this);

  this.dataModel_ = new GalleryDataModel();
  this.selectionModel_ = new cr.ui.ListSelectionModel();

  this.initDom_();
  this.initListeners_();
}

/**
 * Gallery extends cr.EventTarget.
 */
Gallery.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Tools fade-out timeout in milliseconds.
 * @const
 * @type {number}
 */
Gallery.FADE_TIMEOUT = 3000;

/**
 * First time tools fade-out timeout in milliseconds.
 * @const
 * @type {number}
 */
Gallery.FIRST_FADE_TIMEOUT = 1000;

/**
 * Time until mosaic is initialized in the background. Used to make gallery
 * in the slide mode load faster. In milliseconds.
 * @const
 * @type {number}
 */
Gallery.MOSAIC_BACKGROUND_INIT_DELAY = 1000;

/**
 * Types of metadata Gallery uses (to query the metadata cache).
 * @const
 * @type {string}
 */
Gallery.METADATA_TYPE = 'thumbnail|filesystem|media|streaming|drive';

/**
 * Initializes listeners.
 * @private
 */
Gallery.prototype.initListeners_ = function() {
  this.keyDownBound_ = this.onKeyDown_.bind(this);
  this.document_.body.addEventListener('keydown', this.keyDownBound_);

  this.inactivityWatcher_ = new MouseInactivityWatcher(
      this.container_, Gallery.FADE_TIMEOUT, this.hasActiveTool.bind(this));

  // Search results may contain files from different subdirectories so
  // the observer is not going to work.
  if (!this.context_.searchResults && this.context_.curDirEntry) {
    this.metadataCacheObserverId_ = this.metadataCache_.addObserver(
        this.context_.curDirEntry,
        MetadataCache.CHILDREN,
        'thumbnail',
        this.updateThumbnails_.bind(this));
  }
  this.volumeManager_.addEventListener(
      'externally-unmounted', this.onExternallyUnmountedBound_);
};

/**
 * Closes gallery when a volume containing the selected item is unmounted.
 * @param {!Event} event The unmount event.
 * @private
 */
Gallery.prototype.onExternallyUnmounted_ = function(event) {
  if (!this.selectedEntry_)
    return;

  if (this.volumeManager_.getVolumeInfo(this.selectedEntry_) ===
      event.volumeInfo) {
    close();
  }
};

/**
 * Unloads the Gallery.
 * @param {boolean} exiting True if the app is exiting.
 */
Gallery.prototype.onUnload = function(exiting) {
  if (this.metadataCacheObserverId_ !== null)
    this.metadataCache_.removeObserver(this.metadataCacheObserverId_);
  this.volumeManager_.removeEventListener(
      'externally-unmounted', this.onExternallyUnmountedBound_);
  this.slideMode_.onUnload(exiting);
};

/**
 * Initializes DOM UI
 * @private
 */
Gallery.prototype.initDom_ = function() {
  // Initialize the dialog label.
  cr.ui.dialogs.BaseDialog.OK_LABEL = str('GALLERY_OK_LABEL');
  cr.ui.dialogs.BaseDialog.CANCEL_LABEL = str('GALLERY_CANCEL_LABEL');

  var content = document.querySelector('#content');
  content.addEventListener('click', this.onContentClick_.bind(this));

  this.header_ = document.querySelector('#header');
  this.toolbar_ = document.querySelector('#toolbar');

  var preventDefault = function(event) { event.preventDefault(); };

  var minimizeButton = util.createChild(this.header_,
                                        'minimize-button tool dimmable',
                                        'button');
  minimizeButton.tabIndex = -1;
  minimizeButton.addEventListener('click', this.onMinimize_.bind(this));
  minimizeButton.addEventListener('mousedown', preventDefault);

  var maximizeButton = util.createChild(this.header_,
                                        'maximize-button tool dimmable',
                                        'button');
  maximizeButton.tabIndex = -1;
  maximizeButton.addEventListener('click', this.onMaximize_.bind(this));
  maximizeButton.addEventListener('mousedown', preventDefault);

  var closeButton = util.createChild(this.header_,
                                     'close-button tool dimmable',
                                     'button');
  closeButton.tabIndex = -1;
  closeButton.addEventListener('click', this.onClose_.bind(this));
  closeButton.addEventListener('mousedown', preventDefault);

  this.filenameSpacer_ = this.toolbar_.querySelector('.filename-spacer');
  this.filenameEdit_ = util.createChild(this.filenameSpacer_,
                                        'namebox', 'input');

  this.filenameEdit_.setAttribute('type', 'text');
  this.filenameEdit_.addEventListener('blur',
      this.onFilenameEditBlur_.bind(this));

  this.filenameEdit_.addEventListener('focus',
      this.onFilenameFocus_.bind(this));

  this.filenameEdit_.addEventListener('keydown',
      this.onFilenameEditKeydown_.bind(this));

  var middleSpacer = this.filenameSpacer_ =
      this.toolbar_.querySelector('.middle-spacer');
  var buttonSpacer = this.toolbar_.querySelector('button-spacer');

  this.prompt_ = new ImageEditor.Prompt(this.container_, str);

  this.modeButton_ = this.toolbar_.querySelector('button.mode');
  this.modeButton_.addEventListener('click', this.toggleMode_.bind(this, null));

  this.mosaicMode_ = new MosaicMode(content,
                                    this.dataModel_,
                                    this.selectionModel_,
                                    this.volumeManager_,
                                    this.toggleMode_.bind(this, null));

  this.slideMode_ = new SlideMode(this.container_,
                                  content,
                                  this.toolbar_,
                                  this.prompt_,
                                  this.dataModel_,
                                  this.selectionModel_,
                                  this.context_,
                                  this.toggleMode_.bind(this),
                                  str);

  this.slideMode_.addEventListener('image-displayed', function() {
    cr.dispatchSimpleEvent(this, 'image-displayed');
  }.bind(this));
  this.slideMode_.addEventListener('image-saved', function() {
    cr.dispatchSimpleEvent(this, 'image-saved');
  }.bind(this));

  var deleteButton = this.initToolbarButton_('delete', 'GALLERY_DELETE');
  deleteButton.addEventListener('click', this.delete_.bind(this));

  this.shareButton_ = this.initToolbarButton_('share', 'GALLERY_SHARE');
  this.shareButton_.addEventListener(
      'click', this.onShareButtonClick_.bind(this));

  this.dataModel_.addEventListener('splice', this.onSplice_.bind(this));
  this.dataModel_.addEventListener('content', this.onContentChange_.bind(this));

  this.selectionModel_.addEventListener('change', this.onSelection_.bind(this));
  this.slideMode_.addEventListener('useraction', this.onUserAction_.bind(this));

  this.shareDialog_ = new ShareDialog(this.container_);
};

/**
 * Initializes a toolbar button.
 *
 * @param {string} className Class to add.
 * @param {string} title Button title.
 * @return {!HTMLElement} Newly created button.
 * @private
 */
Gallery.prototype.initToolbarButton_ = function(className, title) {
  var button = this.toolbar_.querySelector('button.' + className);
  button.title = str(title);
  return button;
};

/**
 * Loads the content.
 *
 * @param {!Array.<Entry>} entries Array of entries.
 * @param {!Array.<Entry>} selectedEntries Array of selected entries.
 */
Gallery.prototype.load = function(entries, selectedEntries) {
  this.dataModel_.initialize(this.metadataCache_, entries).then(function() {
    // Apply selection.
    this.selectionModel_.adjustLength(this.dataModel_.length);
    var entryIndexesByURLs = {};
    for (var index = 0; index < entries.length; index++) {
      entryIndexesByURLs[entries[index].toURL()] = index;
    }
    for (var i = 0; i !== selectedEntries.length; i++) {
      var selectedIndex = entryIndexesByURLs[selectedEntries[i].toURL()];
      if (selectedIndex !== undefined)
        this.selectionModel_.setIndexSelected(selectedIndex, true);
      else
        console.error('Cannot select ' + selectedEntries[i]);
    }
    if (this.selectionModel_.selectedIndexes.length === 0)
      this.onSelection_();

    // Determine the initial mode.
    var shouldShowMosaic = selectedEntries.length > 1 ||
                           (this.context_.pageState &&
                            this.context_.pageState.gallery === 'mosaic');
    this.setCurrentMode_(shouldShowMosaic ? this.mosaicMode_ : this.slideMode_);

    // Init mosaic mode.
    var mosaic = this.mosaicMode_.getMosaic();
    mosaic.init();

    // Do the initialization for each mode.
    if (shouldShowMosaic) {
      mosaic.show();
      this.inactivityWatcher_.check();  // Show the toolbar.
      cr.dispatchSimpleEvent(this, 'loaded');
    } else {
      this.slideMode_.enter(
          null,
          function() {
            // Flash the toolbar briefly to show it is there.
            this.inactivityWatcher_.kick(Gallery.FIRST_FADE_TIMEOUT);
          }.bind(this),
          function() {
            cr.dispatchSimpleEvent(this, 'loaded');
          }.bind(this));
    }
  }.bind(this)).catch(function(error) {
    console.error(error.stack || error);
  });
};

/**
 * Handles user's 'Close' action.
 * @private
 */
Gallery.prototype.onClose_ = function() {
  this.executeWhenReady(this.context_.onClose);
};

/**
 * Handles user's 'Maximize' action (Escape or a click on the X icon).
 * @private
 */
Gallery.prototype.onMaximize_ = function() {
  this.executeWhenReady(this.context_.onMaximize);
};

/**
 * Handles user's 'Maximize' action (Escape or a click on the X icon).
 * @private
 */
Gallery.prototype.onMinimize_ = function() {
  this.executeWhenReady(this.context_.onMinimize);
};

/**
 * Executes a function when the editor is done with the modifications.
 * @param {function} callback Function to execute.
 */
Gallery.prototype.executeWhenReady = function(callback) {
  this.currentMode_.executeWhenReady(callback);
};

/**
 * @return {Object} File browser private API.
 */
Gallery.getFileBrowserPrivate = function() {
  return chrome.fileBrowserPrivate || window.top.chrome.fileBrowserPrivate;
};

/**
 * @return {boolean} True if some tool is currently active.
 */
Gallery.prototype.hasActiveTool = function() {
  return (this.currentMode_ && this.currentMode_.hasActiveTool()) ||
      this.isRenaming_();
};

/**
* External user action event handler.
* @private
*/
Gallery.prototype.onUserAction_ = function() {
  // Show the toolbar and hide it after the default timeout.
  this.inactivityWatcher_.kick();
};

/**
 * Sets the current mode, update the UI.
 * @param {Object} mode Current mode.
 * @private
 */
Gallery.prototype.setCurrentMode_ = function(mode) {
  if (mode !== this.slideMode_ && mode !== this.mosaicMode_)
    console.error('Invalid Gallery mode');

  this.currentMode_ = mode;
  this.container_.setAttribute('mode', this.currentMode_.getName());
  this.updateSelectionAndState_();
  this.updateButtons_();
};

/**
 * Mode toggle event handler.
 * @param {function=} opt_callback Callback.
 * @param {Event=} opt_event Event that caused this call.
 * @private
 */
Gallery.prototype.toggleMode_ = function(opt_callback, opt_event) {
  if (!this.modeButton_)
    return;

  if (this.changingMode_) // Do not re-enter while changing the mode.
    return;

  if (opt_event)
    this.onUserAction_();

  this.changingMode_ = true;

  var onModeChanged = function() {
    this.changingMode_ = false;
    if (opt_callback) opt_callback();
  }.bind(this);

  var tileIndex = Math.max(0, this.selectionModel_.selectedIndex);

  var mosaic = this.mosaicMode_.getMosaic();
  var tileRect = mosaic.getTileRect(tileIndex);

  if (this.currentMode_ === this.slideMode_) {
    this.setCurrentMode_(this.mosaicMode_);
    mosaic.transform(
        tileRect, this.slideMode_.getSelectedImageRect(), true /* instant */);
    this.slideMode_.leave(
        tileRect,
        function() {
          // Animate back to normal position.
          mosaic.transform();
          mosaic.show();
          onModeChanged();
        }.bind(this));
  } else {
    this.setCurrentMode_(this.slideMode_);
    this.slideMode_.enter(
        tileRect,
        function() {
          // Animate to zoomed position.
          mosaic.transform(tileRect, this.slideMode_.getSelectedImageRect());
          mosaic.hide();
        }.bind(this),
        onModeChanged);
  }
};

/**
 * Deletes the selected items.
 * @private
 */
Gallery.prototype.delete_ = function() {
  this.onUserAction_();

  // Clone the sorted selected indexes array.
  var indexesToRemove = this.selectionModel_.selectedIndexes.slice();
  if (!indexesToRemove.length)
    return;

  /* TODO(dgozman): Implement Undo delete, Remove the confirmation dialog. */

  var itemsToRemove = this.getSelectedItems();
  var plural = itemsToRemove.length > 1;
  var param = plural ? itemsToRemove.length : itemsToRemove[0].getFileName();

  function deleteNext() {
    if (!itemsToRemove.length)
      return;  // All deleted.

    var entry = itemsToRemove.pop().getEntry();
    entry.remove(deleteNext, function() {
      util.flog('Error deleting: ' + entry.name, deleteNext);
    });
  }

  // Prevent the Gallery from handling Esc and Enter.
  this.document_.body.removeEventListener('keydown', this.keyDownBound_);
  var restoreListener = function() {
    this.document_.body.addEventListener('keydown', this.keyDownBound_);
  }.bind(this);


  var confirm = new cr.ui.dialogs.ConfirmDialog(this.container_);
  confirm.setOkLabel(str('DELETE_BUTTON_LABEL'));
  confirm.show(strf(plural ?
      'GALLERY_CONFIRM_DELETE_SOME' : 'GALLERY_CONFIRM_DELETE_ONE', param),
      function() {
        restoreListener();
        this.selectionModel_.unselectAll();
        this.selectionModel_.leadIndex = -1;
        // Remove items from the data model, starting from the highest index.
        while (indexesToRemove.length)
          this.dataModel_.splice(indexesToRemove.pop(), 1);
        // Delete actual files.
        deleteNext();
      }.bind(this),
      function() {
        // Restore the listener after a timeout so that ESC is processed.
        setTimeout(restoreListener, 0);
      });
};

/**
 * @return {Array.<Gallery.Item>} Current selection.
 */
Gallery.prototype.getSelectedItems = function() {
  return this.selectionModel_.selectedIndexes.map(
      this.dataModel_.item.bind(this.dataModel_));
};

/**
 * @return {Array.<Entry>} Array of currently selected entries.
 */
Gallery.prototype.getSelectedEntries = function() {
  return this.selectionModel_.selectedIndexes.map(function(index) {
    return this.dataModel_.item(index).getEntry();
  }.bind(this));
};

/**
 * @return {?Gallery.Item} Current single selection.
 */
Gallery.prototype.getSingleSelectedItem = function() {
  var items = this.getSelectedItems();
  if (items.length > 1) {
    console.error('Unexpected multiple selection');
    return null;
  }
  return items[0];
};

/**
  * Selection change event handler.
  * @private
  */
Gallery.prototype.onSelection_ = function() {
  this.updateSelectionAndState_();
};

/**
  * Data model splice event handler.
  * @private
  */
Gallery.prototype.onSplice_ = function() {
  this.selectionModel_.adjustLength(this.dataModel_.length);
};

/**
 * Content change event handler.
 * @param {Event} event Event.
 * @private
*/
Gallery.prototype.onContentChange_ = function(event) {
  var index = this.dataModel_.indexOf(event.item);
  if (index !== this.selectionModel_.selectedIndex)
    console.error('Content changed for unselected item');
  this.updateSelectionAndState_();
};

/**
 * Keydown handler.
 *
 * @param {Event} event Event.
 * @private
 */
Gallery.prototype.onKeyDown_ = function(event) {
  if (this.currentMode_.onKeyDown(event))
    return;

  switch (util.getKeyModifiers(event) + event.keyIdentifier) {
    case 'U+0008': // Backspace.
      // The default handler would call history.back and close the Gallery.
      event.preventDefault();
      break;

    case 'U+004D':  // 'm' switches between Slide and Mosaic mode.
      this.toggleMode_(null, event);
      break;

    case 'U+0056':  // 'v'
    case 'MediaPlayPause':
      this.slideMode_.startSlideshow(SlideMode.SLIDESHOW_INTERVAL_FIRST, event);
      break;

    case 'U+007F':  // Delete
    case 'Shift-U+0033':  // Shift+'3' (Delete key might be missing).
    case 'U+0044':  // 'd'
      this.delete_();
      break;
  }
};

// Name box and rename support.

/**
 * Updates the UI related to the selected item and the persistent state.
 *
 * @private
 */
Gallery.prototype.updateSelectionAndState_ = function() {
  var numSelectedItems = this.selectionModel_.selectedIndexes.length;
  var displayName = '';
  var selectedEntryURL = null;

  // If it's selecting something, update the variable values.
  if (numSelectedItems) {
    // Obtains selected item.
    var selectedItem =
        this.dataModel_.item(this.selectionModel_.selectedIndex);
    this.selectedEntry_ = selectedItem.getEntry();
    selectedEntryURL = this.selectedEntry_.toURL();

    // Update cache.
    selectedItem.touch();
    this.dataModel_.evictCache();

    // Update the title and the display name.
    if (numSelectedItems === 1) {
      window.top.document.title = this.selectedEntry_.name;
      displayName = ImageUtil.getDisplayNameFromName(this.selectedEntry_.name);
    } else if (this.context_.curDirEntry) {
      // If the Gallery was opened on search results the search query will not
      // be recorded in the app state and the relaunch will just open the
      // gallery in the curDirEntry directory.
      window.top.document.title = this.context_.curDirEntry.name;
      displayName = strf('GALLERY_ITEMS_SELECTED', numSelectedItems);
    }
  }

  window.top.util.updateAppState(
      null,  // Keep the current directory.
      selectedEntryURL,  // Update the selection.
      {gallery: (this.currentMode_ === this.mosaicMode_ ? 'mosaic' : 'slide')});

  // We can't rename files in readonly directory.
  // We can only rename a single file.
  this.filenameEdit_.disabled = numSelectedItems !== 1 ||
                                this.context_.readonlyDirName;
  this.filenameEdit_.value = displayName;

  // Update the share button.
  var item = this.getSingleSelectedItem();
  this.shareButton_.hidden = !item || !item.isOnDrive();
};

/**
 * Click event handler on filename edit box
 * @private
 */
Gallery.prototype.onFilenameFocus_ = function() {
  ImageUtil.setAttribute(this.filenameSpacer_, 'renaming', true);
  this.filenameEdit_.originalValue = this.filenameEdit_.value;
  setTimeout(this.filenameEdit_.select.bind(this.filenameEdit_), 0);
  this.onUserAction_();
};

/**
 * Blur event handler on filename edit box.
 *
 * @param {Event} event Blur event.
 * @return {boolean} if default action should be prevented.
 * @private
 */
Gallery.prototype.onFilenameEditBlur_ = function(event) {
  var item = this.getSingleSelectedItem();
  if (item) {
    var oldEntry = item.getEntry();

    item.rename(this.filenameEdit_.value).then(function() {
      var event = new Event('content');
      event.item = item;
      event.oldEntry = oldEntry;
      event.metadata = null;  // Metadata unchanged.
      this.dataModel_.dispatchEvent(event);
    }.bind(this), function(error) {
      if (error === 'NOT_CHANGED')
        return;
      this.filenameEdit_.value =
          ImageUtil.getDisplayNameFromName(item.getEntry().name);
      this.filenameEdit_.focus();
      if (typeof error === 'string')
        this.prompt_.showStringAt('center', error, 5000);
      else
        return Promise.reject(error);
    }.bind(this)).catch(function(error) {
      console.error(error.stack || error);
    });
  }

  ImageUtil.setAttribute(this.filenameSpacer_, 'renaming', false);
  this.onUserAction_();
};

/**
 * Keydown event handler on filename edit box
 * @private
 */
Gallery.prototype.onFilenameEditKeydown_ = function() {
  switch (event.keyCode) {
    case 27:  // Escape
      this.filenameEdit_.value = this.filenameEdit_.originalValue;
      this.filenameEdit_.blur();
      break;

    case 13:  // Enter
      this.filenameEdit_.blur();
      break;
  }
  event.stopPropagation();
};

/**
 * @return {boolean} True if file renaming is currently in progress.
 * @private
 */
Gallery.prototype.isRenaming_ = function() {
  return this.filenameSpacer_.hasAttribute('renaming');
};

/**
 * Content area click handler.
 * @private
 */
Gallery.prototype.onContentClick_ = function() {
  this.filenameEdit_.blur();
};

/**
 * Share button handler.
 * @private
 */
Gallery.prototype.onShareButtonClick_ = function() {
  var item = this.getSingleSelectedItem();
  if (!item)
    return;
  this.shareDialog_.show(item.getEntry(), function() {});
};

/**
 * Updates thumbnails.
 * @private
 */
Gallery.prototype.updateThumbnails_ = function() {
  if (this.currentMode_ === this.slideMode_)
    this.slideMode_.updateThumbnails();

  if (this.mosaicMode_) {
    var mosaic = this.mosaicMode_.getMosaic();
    if (mosaic.isInitialized())
      mosaic.reload();
  }
};

/**
 * Updates buttons.
 * @private
 */
Gallery.prototype.updateButtons_ = function() {
  if (this.modeButton_) {
    var oppositeMode =
        this.currentMode_ === this.slideMode_ ? this.mosaicMode_ :
                                                this.slideMode_;
    this.modeButton_.title = str(oppositeMode.getTitle());
  }
};

/**
 * Singleton gallery.
 * @type {Gallery}
 */
var gallery = null;

/**
 * Initialize the window.
 * @param {Object} backgroundComponents Background components.
 */
window.initialize = function(backgroundComponents) {
  window.loadTimeData.data = backgroundComponents.stringData;
  gallery = new Gallery(backgroundComponents.volumeManager);
};

/**
 * Loads entries.
 */
window.loadEntries = function(entries, selectedEntries) {
  gallery.load(entries, selectedEntries);
};
