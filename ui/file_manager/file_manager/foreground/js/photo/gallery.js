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
 * Gallery for viewing and editing image files.
 *
 * @param {Object} context Object containing the following:
 *     {function(string)} onNameChange Called every time a selected
 *         item name changes (on rename and on selection change).
 *     {AppWindow} appWindow
 *     {function(string)} onBack
 *     {function()} onClose
 *     {function()} onMaximize
 *     {function()} onMinimize
 *     {function(boolean)} onAppRegionChanged
 *     {MetadataCache} metadataCache
 *     {Array.<Object>} shareActions
 *     {string} readonlyDirName Directory name for readonly warning or null.
 *     {DirEntry} saveDirEntry Directory to save to.
 *     {function(string)} displayStringFunction.
 * @param {VolumeManagerWrapper} volumeManager The VolumeManager instance of
 *      the system.
 * @class
 * @constructor
 */
function Gallery(context, volumeManager) {
  this.container_ = document.querySelector('.gallery');
  this.document_ = document;
  this.context_ = context;
  this.metadataCache_ = context.metadataCache;
  this.volumeManager_ = volumeManager;
  this.selectedEntry_ = null;
  this.metadataCacheObserverId_ = null;
  this.onExternallyUnmountedBound_ = this.onExternallyUnmounted_.bind(this);

  this.dataModel_ = new cr.ui.ArrayDataModel([]);
  this.selectionModel_ = new cr.ui.ListSelectionModel();
  loadTimeData.data = context.loadTimeData;

  this.initDom_();
  this.initListeners_();
}

/**
 * Gallery extends cr.EventTarget.
 */
Gallery.prototype.__proto__ = cr.EventTarget.prototype;

/**
 * Creates and initializes a Gallery object based on a context.
 *
 * @param {Object} context Gallery context.
 * @param {VolumeManagerWrapper} volumeManager VolumeManager of the system.
 * @param {Array.<Entry>} entries Array of entries.
 * @param {Array.<Entry>} selectedEntries Array of selected entries.
 */
Gallery.open = function(context, volumeManager, entries, selectedEntries) {
  Gallery.instance = new Gallery(context, volumeManager);
  Gallery.instance.load(entries, selectedEntries);
};

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
 * in the slide mode load faster. In miiliseconds.
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
  this.document_.oncontextmenu = function(e) { e.preventDefault(); };
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
    this.onBack_();
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

  var content = util.createChild(this.container_, 'content');
  content.addEventListener('click', this.onContentClick_.bind(this));

  this.header_ = util.createChild(this.container_, 'header tool dimmable');
  this.toolbar_ = util.createChild(this.container_, 'toolbar tool dimmable');

  var backButton = util.createChild(this.container_,
                                    'back-button tool dimmable');
  util.createChild(backButton);
  backButton.addEventListener('click', this.onBack_.bind(this));

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

  this.filenameSpacer_ = util.createChild(this.toolbar_, 'filename-spacer');
  this.filenameEdit_ = util.createChild(this.filenameSpacer_,
                                        'namebox', 'input');

  this.filenameEdit_.setAttribute('type', 'text');
  this.filenameEdit_.addEventListener('blur',
      this.onFilenameEditBlur_.bind(this));

  this.filenameEdit_.addEventListener('focus',
      this.onFilenameFocus_.bind(this));

  this.filenameEdit_.addEventListener('keydown',
      this.onFilenameEditKeydown_.bind(this));

  util.createChild(this.toolbar_, 'button-spacer');

  this.prompt_ = new ImageEditor.Prompt(this.container_, str);

  this.modeButton_ = util.createChild(this.toolbar_, 'button mode', 'button');
  this.modeButton_.addEventListener('click',
      this.toggleMode_.bind(this, null));

  this.mosaicMode_ = new MosaicMode(content,
                                    this.dataModel_,
                                    this.selectionModel_,
                                    this.metadataCache_,
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

  var deleteButton = this.createToolbarButton_('delete', 'GALLERY_DELETE');
  deleteButton.addEventListener('click', this.delete_.bind(this));

  this.shareButton_ = this.createToolbarButton_('share', 'GALLERY_SHARE');
  this.shareButton_.setAttribute('disabled', '');
  this.shareButton_.addEventListener('click', this.toggleShare_.bind(this));

  this.shareMenu_ = util.createChild(this.container_, 'share-menu');
  this.shareMenu_.hidden = true;
  util.createChild(this.shareMenu_, 'bubble-point');

  this.dataModel_.addEventListener('splice', this.onSplice_.bind(this));
  this.dataModel_.addEventListener('content', this.onContentChange_.bind(this));

  this.selectionModel_.addEventListener('change', this.onSelection_.bind(this));
  this.slideMode_.addEventListener('useraction', this.onUserAction_.bind(this));
};

/**
 * Creates toolbar button.
 *
 * @param {string} className Class to add.
 * @param {string} title Button title.
 * @return {!HTMLElement} Newly created button.
 * @private
 */
Gallery.prototype.createToolbarButton_ = function(className, title) {
  var button = util.createChild(this.toolbar_, className, 'button');
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
  var items = [];
  for (var index = 0; index < entries.length; ++index) {
    items.push(new Gallery.Item(entries[index]));
  }
  this.dataModel_.push.apply(this.dataModel_, items);

  this.selectionModel_.adjustLength(this.dataModel_.length);

  // Comparing Entries by reference is not safe. Therefore we have to use URLs.
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

  var mosaic = this.mosaicMode_ && this.mosaicMode_.getMosaic();

  // Mosaic view should show up if most of the selected files are images.
  var imagesCount = 0;
  for (var i = 0; i !== selectedEntries.length; i++) {
    if (FileType.getMediaType(selectedEntries[i]) === 'image')
      imagesCount++;
  }
  var mostlyImages = imagesCount > (selectedEntries.length / 2.0);

  var forcedMosaic = (this.context_.pageState &&
      this.context_.pageState.gallery === 'mosaic');

  var showMosaic = (mostlyImages && selectedEntries.length > 1) || forcedMosaic;
  if (mosaic && showMosaic) {
    this.setCurrentMode_(this.mosaicMode_);
    mosaic.init();
    mosaic.show();
    this.inactivityWatcher_.check();  // Show the toolbar.
    cr.dispatchSimpleEvent(this, 'loaded');
  } else {
    this.setCurrentMode_(this.slideMode_);
    var maybeLoadMosaic = function() {
      if (mosaic)
        mosaic.init();
      cr.dispatchSimpleEvent(this, 'loaded');
    }.bind(this);
    /* TODO: consider nice blow-up animation for the first image */
    this.slideMode_.enter(null, function() {
        // Flash the toolbar briefly to show it is there.
        this.inactivityWatcher_.kick(Gallery.FIRST_FADE_TIMEOUT);
      }.bind(this),
      maybeLoadMosaic);
  }
};

/**
 * Closes the Gallery and go to Files.app.
 * @private
 */
Gallery.prototype.back_ = function() {
  if (util.isFullScreen(this.context_.appWindow)) {
    util.toggleFullScreen(this.context_.appWindow,
                          false);  // Leave the full screen mode.
  }
  this.context_.onBack(this.getSelectedEntries());
};

/**
 * Handles user's 'Back' action (Escape or a click on the X icon).
 * @private
 */
Gallery.prototype.onBack_ = function() {
  this.executeWhenReady(this.back_.bind(this));
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
  return this.currentMode_.hasActiveTool() ||
      this.isSharing_() || this.isRenaming_();
};

/**
* External user action event handler.
* @private
*/
Gallery.prototype.onUserAction_ = function() {
  this.closeShareMenu_();
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

    // TODO(hirono): Use fileOperationManager.
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
  this.updateShareMenu_();
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
  var wasSharing = this.isSharing_();
  this.closeShareMenu_();

  if (this.currentMode_.onKeyDown(event))
    return;

  switch (util.getKeyModifiers(event) + event.keyIdentifier) {
    case 'U+0008': // Backspace.
      // The default handler would call history.back and close the Gallery.
      event.preventDefault();
      break;

    case 'U+001B':  // Escape
      // Swallow Esc if it closed the Share menu, otherwise close the Gallery.
      if (!wasSharing)
        this.onBack_();
      break;

    case 'U+004D':  // 'm' switches between Slide and Mosaic mode.
      this.toggleMode_(null, event);
      break;

    case 'U+0056':  // 'v'
      this.slideMode_.startSlideshow(SlideMode.SLIDESHOW_INTERVAL_FIRST, event);
      break;

    case 'U+007F':  // Delete
    case 'Shift-U+0033':  // Shift+'3' (Delete key might be missing).
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
    var selectedItem =
        this.dataModel_.item(this.selectionModel_.selectedIndex);
    this.selectedEntry_ = selectedItem.getEntry();
    selectedEntryURL = this.selectedEntry_.toURL();

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
  if (this.filenameEdit_.value && this.filenameEdit_.value[0] === '.') {
    this.prompt_.show('GALLERY_FILE_HIDDEN_NAME', 5000);
    this.filenameEdit_.focus();
    event.stopPropagation();
    event.preventDefault();
    return false;
  }

  var item = this.getSingleSelectedItem();
  if (item) {
    var oldEntry = item.getEntry();

    var onFileExists = function() {
      this.prompt_.show('GALLERY_FILE_EXISTS', 3000);
      this.filenameEdit_.value = name;
      this.filenameEdit_.focus();
    }.bind(this);

    var onSuccess = function() {
      var event = new Event('content');
      event.item = item;
      event.oldEntry = oldEntry;
      event.metadata = null;  // Metadata unchanged.
      this.dataModel_.dispatchEvent(event);
    }.bind(this);

    if (this.filenameEdit_.value) {
      item.rename(
          this.filenameEdit_.value, onSuccess, onFileExists);
    }
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
  this.closeShareMenu_();
  this.filenameEdit_.blur();
};

// Share button support.

/**
 * @return {boolean} True if the Share menu is active.
 * @private
 */
Gallery.prototype.isSharing_ = function() {
  return !this.shareMenu_.hidden;
};

/**
 * Close Share menu if it is open.
 * @private
 */
Gallery.prototype.closeShareMenu_ = function() {
  if (this.isSharing_())
    this.toggleShare_();
};

/**
 * Share button handler.
 * @private
 */
Gallery.prototype.toggleShare_ = function() {
  if (!this.shareButton_.hasAttribute('disabled'))
    this.shareMenu_.hidden = !this.shareMenu_.hidden;
  this.inactivityWatcher_.check();
};

/**
 * Updates available actions list based on the currently selected urls.
 * @private.
 */
Gallery.prototype.updateShareMenu_ = function() {
  var entries = this.getSelectedEntries();

  function isShareAction(task) {
    var taskParts = task.taskId.split('|');
    return taskParts[0] !== chrome.runtime.id;
  }

  var api = Gallery.getFileBrowserPrivate();

  var createShareMenu = function(tasks) {
    var wasHidden = this.shareMenu_.hidden;
    this.shareMenu_.hidden = true;
    var items = this.shareMenu_.querySelectorAll('.item');
    for (var i = 0; i !== items.length; i++) {
      items[i].parentNode.removeChild(items[i]);
    }

    for (var t = 0; t !== tasks.length; t++) {
      var task = tasks[t];
      if (!isShareAction(task)) continue;

      var item = util.createChild(this.shareMenu_, 'item');
      item.textContent = task.title;
      item.style.backgroundImage = 'url(' + task.iconUrl + ')';
      item.addEventListener('click', function(taskId) {
        this.toggleShare_();  // Hide the menu.
        // TODO(hirono): Use entries instead of URLs.
        this.executeWhenReady(
            api.executeTask.bind(
                api,
                taskId,
                util.entriesToURLs(entries),
                function(result) {
                  var alertDialog =
                      new cr.ui.dialogs.AlertDialog(this.container_);
                  util.isTeleported(window).then(function(teleported) {
                    if (teleported)
                      util.showOpenInOtherDesktopAlert(alertDialog, entries);
                  }.bind(this));
                }.bind(this)));
      }.bind(this, task.taskId));
    }

    var empty = this.shareMenu_.querySelector('.item') === null;
    ImageUtil.setAttribute(this.shareButton_, 'disabled', empty);
    this.shareMenu_.hidden = wasHidden || empty;
  }.bind(this);

  // Create or update the share menu with a list of sharing tasks and show
  // or hide the share button.
  // TODO(mtomasz): Pass Entries directly, instead of URLs.
  if (!entries.length)
    createShareMenu([]);  // Empty list of tasks, since there is no selection.
  else
    api.getFileTasks(util.entriesToURLs(entries), createShareMenu);
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
