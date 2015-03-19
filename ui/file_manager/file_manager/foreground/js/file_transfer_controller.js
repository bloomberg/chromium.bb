// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Global (placed in the window object) variable name to hold internal
 * file dragging information. Needed to show visual feedback while dragging
 * since DataTransfer object is in protected state. Reachable from other
 * file manager instances.
 */
var DRAG_AND_DROP_GLOBAL_DATA = '__drag_and_drop_global_data';

/**
 * @typedef {{file:File, externalFileUrl:string}}
 */
var FileAsyncData;

/**
 * @param {!HTMLDocument} doc Owning document.
 * @param {!DirectoryTree} directoryTree Directory tree.
 * @param {!ListContainer} listContainer List container.
 * @param {!MultiProfileShareDialog} multiProfileShareDialog Share dialog to be
 *     used to share files from another profile.
 * @param {!ProgressCenter} progressCenter To notify starting copy operation.
 * @param {!FileOperationManager} fileOperationManager File operation manager
 *     instance.
 * @param {!MetadataModel} metadataModel Metadata cache service.
 * @param {!ThumbnailModel} thumbnailModel
 * @param {!DirectoryModel} directoryModel Directory model instance.
 * @param {!VolumeManagerWrapper} volumeManager Volume manager instance.
 * @param {!FileSelectionHandler} selectionHandler Selection handler.
 * @struct
 * @constructor
 */
function FileTransferController(doc,
                                listContainer,
                                directoryTree,
                                multiProfileShareDialog,
                                progressCenter,
                                fileOperationManager,
                                metadataModel,
                                thumbnailModel,
                                directoryModel,
                                volumeManager,
                                selectionHandler) {
  /**
   * @type {!HTMLDocument}
   * @private
   * @const
   */
  this.document_ = doc;

  /**
   * @type {!ListContainer}
   * @private
   * @const
   */
  this.listContainer_ = listContainer;

  /**
   * @type {!FileOperationManager}
   * @private
   * @const
   */
  this.fileOperationManager_ = fileOperationManager;

  /**
   * @type {!MetadataModel}
   * @private
   * @const
   */
  this.metadataModel_ = metadataModel;

  /**
   * @type {!ThumbnailModel}
   * @private
   * @const
   */
  this.thumbnailModel_ = thumbnailModel;

  /**
   * @type {!DirectoryModel}
   * @private
   * @const
   */
  this.directoryModel_ = directoryModel;

  /**
   * @type {!VolumeManagerWrapper}
   * @private
   * @const
   */
  this.volumeManager_ = volumeManager;

  /**
   * @type {!FileSelectionHandler}
   * @private
   * @const
   */
  this.selectionHandler_ = selectionHandler;

  /**
   * @type {!MultiProfileShareDialog}
   * @private
   * @const
   */
  this.multiProfileShareDialog_ = multiProfileShareDialog;

  /**
   * @type {!ProgressCenter}
   * @private
   * @const
   */
  this.progressCenter_ = progressCenter;

  /**
   * The array of pending task ID.
   * @type {Array.<string>}
   */
  this.pendingTaskIds = [];

  /**
   * Promise to be fulfilled with the thumbnail image of selected file in drag
   * operation. Used if only one element is selected.
   * @type {Promise}
   * @private
   */
  this.preloadedThumbnailImagePromise_ = null;

  /**
   * File objects for selected files.
   *
   * @type {Object.<string, FileAsyncData>}
   * @private
   */
  this.selectedAsyncData_ = {};

  /**
   * Drag selector.
   * @type {DragSelector}
   * @private
   */
  this.dragSelector_ = new DragSelector();

  /**
   * Whether a user is touching the device or not.
   * @type {boolean}
   * @private
   */
  this.touching_ = false;

  /**
   * Count of the SourceNotFound error.
   * @type {number}
   * @private
   */
  this.sourceNotFoundErrorCount_ = 0;

  /**
   * @type {!Element}
   * @private
   * @const
   */
  this.copyCommand_ = queryRequiredElement(this.document_, 'command#copy');

  /**
   * @type {DirectoryEntry}
   * @private
   */
  this.destinationEntry_ = null;

  /**
   * @type {EventTarget}
   * @private
   */
  this.lastEnteredTarget_ = null;

  /**
   * @type {Element}
   * @private
   */
  this.dropTarget_ = null;

  /**
   * @type {number}
   */
  this.navigateTimer_ = 0;

  // Register the events.
  selectionHandler.addEventListener(
      FileSelectionHandler.EventType.CHANGE,
      this.onFileSelectionChanged_.bind(this));
  selectionHandler.addEventListener(
      FileSelectionHandler.EventType.CHANGE_THROTTLED,
      this.onFileSelectionChangedThrottled_.bind(this));
  this.attachDragSource_(listContainer.table.list);
  this.attachFileListDropTarget_(listContainer.table.list);
  this.attachDragSource_(listContainer.grid);
  this.attachFileListDropTarget_(listContainer.grid);
  this.attachTreeDropTarget_(directoryTree);
  this.attachCopyPasteHandlers_();

  // Allow to drag external files to the browser window.
  chrome.fileManagerPrivate.enableExternalFileScheme();
}

/**
 * Size of drag thumbnail for image files.
 *
 * @type {number}
 * @const
 * @private
 */
FileTransferController.DRAG_THUMBNAIL_SIZE_ = 64;

/**
 * Converts list of urls to list of Entries with granting R/W permissions to
 * them, which is essential when pasting files from a different profile.
 *
 * @param {!Array<string>} urls Urls to be converted.
 * @return {Promise<!Array<string>>}
 */
FileTransferController.URLsToEntriesWithAccess = function(urls) {
  return new Promise(function(resolve, reject) {
    chrome.fileManagerPrivate.grantAccess(urls, resolve.bind(null, undefined));
  }).then(function() {
    return util.URLsToEntries(urls);
  });
};

/**
 * @param {!cr.ui.List} list Items in the list will be draggable.
 * @private
 */
FileTransferController.prototype.attachDragSource_ = function(list) {
  list.style.webkitUserDrag = 'element';
  list.addEventListener('dragstart', this.onDragStart_.bind(this, list));
  list.addEventListener('dragend', this.onDragEnd_.bind(this, list));
  list.addEventListener('touchstart', this.onTouchStart_.bind(this));
  list.ownerDocument.addEventListener(
      'touchend', this.onTouchEnd_.bind(this), true);
  list.ownerDocument.addEventListener(
      'touchcancel', this.onTouchEnd_.bind(this), true);
};

/**
 * @param {!cr.ui.List} list List itself and its directory items will could
 *                          be drop target.
 * @param {boolean=} opt_onlyIntoDirectories If true only directory list
 *     items could be drop targets. Otherwise any other place of the list
 *     accetps files (putting it into the current directory).
 * @private
 */
FileTransferController.prototype.attachFileListDropTarget_ =
    function(list, opt_onlyIntoDirectories) {
  list.addEventListener('dragover', this.onDragOver_.bind(this,
      !!opt_onlyIntoDirectories, list));
  list.addEventListener('dragenter',
      this.onDragEnterFileList_.bind(this, list));
  list.addEventListener('dragleave', this.onDragLeave_.bind(this, list));
  list.addEventListener('drop',
      this.onDrop_.bind(this, !!opt_onlyIntoDirectories));
};

/**
 * @param {!DirectoryTree} tree Its sub items will could be drop target.
 * @private
 */
FileTransferController.prototype.attachTreeDropTarget_ = function(tree) {
  tree.addEventListener('dragover', this.onDragOver_.bind(this, true, tree));
  tree.addEventListener('dragenter', this.onDragEnterTree_.bind(this, tree));
  tree.addEventListener('dragleave', this.onDragLeave_.bind(this, tree));
  tree.addEventListener('drop', this.onDrop_.bind(this, true));
};

/**
 * Attach handlers of copy, cut and paste operations to the document.
 * @private
 */
FileTransferController.prototype.attachCopyPasteHandlers_ = function() {
  this.document_.addEventListener('beforecopy',
                                  this.onBeforeCopy_.bind(this));
  this.document_.addEventListener('copy',
                                  this.onCopy_.bind(this));
  this.document_.addEventListener('beforecut',
                                  this.onBeforeCut_.bind(this));
  this.document_.addEventListener('cut',
                                  this.onCut_.bind(this));
  this.document_.addEventListener('beforepaste',
                                  this.onBeforePaste_.bind(this));
  this.document_.addEventListener('paste',
                                  this.onPaste_.bind(this));
};

/**
 * Write the current selection to system clipboard.
 *
 * @param {!ClipboardData} clipboardData ClipboardData from the event.
 * @param {string} effectAllowed Value must be valid for the
 *     |clipboardData.effectAllowed| property.
 * @private
 */
FileTransferController.prototype.cutOrCopy_ =
    function(clipboardData, effectAllowed) {
  // Existence of the volumeInfo is checked in canXXX methods.
  var volumeInfo = this.volumeManager_.getVolumeInfo(
      this.directoryModel_.getCurrentDirEntry());
  // Tag to check it's filemanager data.
  clipboardData.setData('fs/tag', 'filemanager-data');
  clipboardData.setData('fs/sourceRootURL',
                       volumeInfo.fileSystem.root.toURL());
  var sourceURLs = util.entriesToURLs(this.selectionHandler_.selection.entries);
  clipboardData.setData('fs/sources', sourceURLs.join('\n'));
  clipboardData.effectAllowed = effectAllowed;
  clipboardData.setData('fs/effectallowed', effectAllowed);
  clipboardData.setData('fs/missingFileContents',
                       (!this.selectionHandler_.isAvailable()).toString());
  var externalFileUrl;
  for (var i = 0; i < this.selectionHandler_.selection.entries.length; i++) {
    var url = this.selectionHandler_.selection.entries[i].toURL();
    if (!this.selectedAsyncData_[url])
      continue;
    if (this.selectedAsyncData_[url].file)
      clipboardData.items.add(this.selectedAsyncData_[url].file);
    if (!externalFileUrl)
      externalFileUrl = this.selectedAsyncData_[url].externalFileUrl;
  }
  if (externalFileUrl)
    clipboardData.setData('text/uri-list', externalFileUrl);
};

/**
 * @return {Object.<string, string>} Drag and drop global data object.
 * @private
 */
FileTransferController.prototype.getDragAndDropGlobalData_ = function() {
  if (window[DRAG_AND_DROP_GLOBAL_DATA])
    return window[DRAG_AND_DROP_GLOBAL_DATA];

  // Dragging from other tabs/windows.
  var views = chrome && chrome.extension ? chrome.extension.getViews() : [];
  for (var i = 0; i < views.length; i++) {
    if (views[i][DRAG_AND_DROP_GLOBAL_DATA])
      return views[i][DRAG_AND_DROP_GLOBAL_DATA];
  }
  return null;
};

/**
 * Extracts source root URL from the |clipboardData| object.
 *
 * @param {!ClipboardData} clipboardData DataTransfer object from the event.
 * @return {string} URL or an empty string (if unknown).
 * @private
 */
FileTransferController.prototype.getSourceRootURL_ = function(clipboardData) {
  var sourceRootURL = clipboardData.getData('fs/sourceRootURL');
  if (sourceRootURL)
    return sourceRootURL;

  // |clipboardData| in protected mode.
  var globalData = this.getDragAndDropGlobalData_();
  if (globalData)
    return globalData.sourceRootURL;

  // Unknown source.
  return '';
};

/**
 * @param {!ClipboardData} clipboardData DataTransfer object from the event.
 * @return {boolean} Returns true when missing some file contents.
 * @private
 */
FileTransferController.prototype.isMissingFileContents_ =
    function(clipboardData) {
  var data = clipboardData.getData('fs/missingFileContents');
  if (!data) {
    // |clipboardData| in protected mode.
    var globalData = this.getDragAndDropGlobalData_();
    if (globalData)
      data = globalData.missingFileContents;
  }
  return data === 'true';
};

/**
 * Obtains entries that need to share with me.
 * The method also observers child entries of the given entries.
 * @param {Array.<Entry>} entries Entries.
 * @return {Promise} Promise to be fulfilled with the entries that need to
 *     share.
 * @private
 */
FileTransferController.prototype.getMultiProfileShareEntries_ =
    function(entries) {
  // Utility function to concat arrays.
  var concatArrays = function(arrays) {
    return Array.prototype.concat.apply([], arrays);
  };

  // Call processEntry for each item of entries.
  var processEntries = function(entries) {
    var files = entries.filter(function(entry) {return entry.isFile;});
    var dirs = entries.filter(function(entry) {return !entry.isFile;});
    var promises = dirs.map(processDirectoryEntry);
    if (files.length > 0)
      promises.push(processFileEntries(files));
    return Promise.all(promises).then(concatArrays);
  };

  // Check all file entries and keeps only those need sharing operation.
  var processFileEntries = function(entries) {
    return new Promise(function(callback) {
      // TODO(mtomasz): Move conversion from entry to url to custom bindings.
      // crbug.com/345527.
      var urls = util.entriesToURLs(entries);
      // Do not use metadata cache here because the urls come from the different
      // profile.
      chrome.fileManagerPrivate.getEntryProperties(
          urls, ['hosted', 'sharedWithMe'], callback);
    }).then(function(metadatas) {
      return entries.filter(function(entry, i) {
        var metadata = metadatas[i];
        return metadata && metadata.hosted && !metadata.sharedWithMe;
      });
    });
  };

  // Check child entries.
  var processDirectoryEntry = function(entry) {
    return readEntries(entry.createReader());
  };

  // Read entries from DirectoryReader and call processEntries for the chunk
  // of entries.
  var readEntries = function(reader) {
    return new Promise(reader.readEntries.bind(reader)).then(
        function(entries) {
          if (entries.length > 0) {
            return Promise.all(
                [processEntries(entries), readEntries(reader)]).
                then(concatArrays);
          } else {
            return [];
          }
        },
        function(error) {
          console.warn(
              'Error happens while reading directory.', error);
          return [];
        });
  }.bind(this);

  // Filter entries that is owned by the current user, and call
  // processEntries.
  return processEntries(entries.filter(function(entry) {
    // If the volumeInfo is found, the entry belongs to the current user.
    return !this.volumeManager_.getVolumeInfo(entry);
  }.bind(this)));
};

/**
 * Queue up a file copy operation based on the current system clipboard.
 *
 * @param {!ClipboardData} clipboardData System data transfer object.
 * @param {DirectoryEntry=} opt_destinationEntry Paste destination.
 * @param {string=} opt_effect Desired drop/paste effect. Could be
 *     'move'|'copy' (default is copy). Ignored if conflicts with
 *     |clipboardData.effectAllowed|.
 * @return {string} Either "copy" or "move".
 */
FileTransferController.prototype.paste =
    function(clipboardData, opt_destinationEntry, opt_effect) {
  var sourceURLs = clipboardData.getData('fs/sources') ?
      clipboardData.getData('fs/sources').split('\n') : [];
  // effectAllowed set in copy/paste handlers stay uninitialized. DnD handlers
  // work fine.
  var effectAllowed = clipboardData.effectAllowed !== 'uninitialized' ?
      clipboardData.effectAllowed : clipboardData.getData('fs/effectallowed');
  var toMove = util.isDropEffectAllowed(effectAllowed, 'move') &&
      (!util.isDropEffectAllowed(effectAllowed, 'copy') ||
       opt_effect === 'move');
  var destinationEntry =
      opt_destinationEntry ||
      /** @type {DirectoryEntry} */ (this.directoryModel_.getCurrentDirEntry());
  var entries = [];
  var failureUrls;
  var taskId = this.fileOperationManager_.generateTaskId();

  FileTransferController.URLsToEntriesWithAccess(sourceURLs)
      .then(
          /**
           * @param {Object} result
           * @this {FileTransferController}
           */
          function(result) {
            failureUrls = result.failureUrls;
            return this.fileOperationManager_.filterSameDirectoryEntry(
                result.entries, destinationEntry, toMove);
          }.bind(this))
      .then(
          /**
           * @param {!Array<Entry>} filteredEntries
           * @this {FileTransferController}
           * @return {!Promise<Array<Entry>>}
           */
          function(filteredEntries) {
            entries = filteredEntries;
            if (entries.length === 0)
              return Promise.reject('ABORT');

            this.pendingTaskIds.push(taskId);
            var item = new ProgressCenterItem();
            item.id = taskId;
            if (toMove) {
              item.type = ProgressItemType.MOVE;
              if (entries.length === 1)
                item.message = strf('MOVE_FILE_NAME', entries[0].name);
              else
                item.message = strf('MOVE_ITEMS_REMAINING', entries.length);
            } else {
              item.type = ProgressItemType.COPY;
              if (entries.length === 1)
                item.message = strf('COPY_FILE_NAME', entries[0].name);
              else
                item.message = strf('COPY_ITEMS_REMAINING', entries.length);
            }
            this.progressCenter_.updateItem(item);
            // Check if cross share is needed or not.
            return this.getMultiProfileShareEntries_(entries);
          }.bind(this))
      .then(
          /**
           * @param {!Array<Entry>} shareEntries
           * @this {FileTransferController}
           * @return {!Promise<Array<Entry>>|undefined}
           */
          function(shareEntries) {
            if (shareEntries.length === 0)
              return;
            return this.multiProfileShareDialog_.
                showMultiProfileShareDialog(shareEntries.length > 1).then(
                    /**
                     * @param {string} dialogResult
                     * @this {FileTransferController}
                     * @return {!Promise<undefined>|undefined}
                     */
                    function(dialogResult) {
                      if (dialogResult === 'cancel')
                        return Promise.reject('ABORT');
                      // Do cross share.
                      // TODO(hirono): Make the loop cancellable.
                      var requestDriveShare = function(index) {
                        if (index >= shareEntries.length)
                          return;
                        return new Promise(function(fulfill) {
                          chrome.fileManagerPrivate.requestDriveShare(
                              shareEntries[index].toURL(),
                              dialogResult,
                              function() {
                                // TODO(hirono): Check chrome.runtime.lastError
                                // here.
                                fulfill(undefined);
                              });
                        }).then(requestDriveShare.bind(null, index + 1));
                      };
                      return requestDriveShare(0);
                    });
          }.bind(this))
      .then(
          /**
           * @this {FileTransferController}
           */
          function() {
            // Start the pasting operation.
            this.fileOperationManager_.paste(
                entries, destinationEntry, toMove, taskId);
            this.pendingTaskIds.splice(this.pendingTaskIds.indexOf(taskId), 1);

            // Publish source not found error item.
            for (var i = 0; i < failureUrls.length; i++) {
              var fileName =
                  decodeURIComponent(failureUrls[i].replace(/^.+\//, ''));
              var item = new ProgressCenterItem();
              item.id = 'source-not-found-' + this.sourceNotFoundErrorCount_;
              if (toMove)
                item.message = strf('MOVE_SOURCE_NOT_FOUND_ERROR', fileName);
              else
                item.message = strf('COPY_SOURCE_NOT_FOUND_ERROR', fileName);
              item.state = ProgressItemState.ERROR;
              this.progressCenter_.updateItem(item);
              this.sourceNotFoundErrorCount_++;
            }
          }.bind(this))
      .catch(
          function(error) {
            if (error !== 'ABORT')
              console.error(error.stack ? error.stack : error);
          });
  return toMove ? 'move' : 'copy';
};

/**
 * Preloads an image thumbnail for the specified file entry.
 *
 * @param {Entry} entry Entry to preload a thumbnail for.
 * @private
 */
FileTransferController.prototype.preloadThumbnailImage_ = function(entry) {
  var imagePromise = this.thumbnailModel_.get([entry]).then(function(metadata) {
    return new Promise(function(fulfill, reject) {
      var loader = new ThumbnailLoader(
          entry, ThumbnailLoader.LoaderType.IMAGE, metadata[0]);
      loader.loadDetachedImage(function(result) {
        if (result)
          fulfill(loader.getImage());
      });
    });
  });

  imagePromise.then(function(image) {
    // Store the image so that we can obtain the image synchronously.
    imagePromise.value = image;
  });

  this.preloadedThumbnailImagePromise_ = imagePromise;
};

/**
 * Renders a drag-and-drop thumbnail.
 *
 * @return {!Element} Element containing the thumbnail.
 * @private
 */
FileTransferController.prototype.renderThumbnail_ = function() {
  var length = this.selectionHandler_.selection.entries.length;
  var container = this.document_.querySelector('#drag-container');
  var contents = this.document_.createElement('div');
  contents.className = 'drag-contents';
  container.appendChild(contents);

  // Option 1. Multiple selection, render only a label.
  if (length > 1) {
    var label = this.document_.createElement('div');
    label.className = 'label';
    label.textContent = strf('DRAGGING_MULTIPLE_ITEMS', length);
    contents.appendChild(label);
    return container;
  }

  // Option 2. Thumbnail image available, then render it without
  // a label.
  if (this.preloadedThumbnailImagePromise_ &&
      this.preloadedThumbnailImagePromise_.value) {
    var thumbnailImage = this.preloadedThumbnailImagePromise_.value;

    // Resize the image to canvas.
    var canvas = document.createElement('canvas');
    canvas.width = FileTransferController.DRAG_THUMBNAIL_SIZE_;
    canvas.height = FileTransferController.DRAG_THUMBNAIL_SIZE_;

    var minScale = Math.min(
        thumbnailImage.width / canvas.width,
        thumbnailImage.height / canvas.height);
    var srcWidth = Math.min(canvas.width * minScale, thumbnailImage.width);
    var srcHeight = Math.min(canvas.height * minScale, thumbnailImage.height);

    var context = canvas.getContext('2d');
    context.drawImage(thumbnailImage,
                      (thumbnailImage.width - srcWidth) / 2,
                      (thumbnailImage.height - srcHeight) / 2,
                      srcWidth,
                      srcHeight,
                      0,
                      0,
                      canvas.width,
                      canvas.height);
    contents.classList.add('for-image');
    contents.appendChild(canvas);
    return container;
  }

  // Option 3. Thumbnail not available. Render an icon and a label.
  var entry = this.selectionHandler_.selection.entries[0];
  var icon = this.document_.createElement('div');
  icon.className = 'detail-icon';
  icon.setAttribute('file-type-icon', FileType.getIcon(entry));
  contents.appendChild(icon);
  var label = this.document_.createElement('div');
  label.className = 'label';
  label.textContent = entry.name;
  contents.appendChild(label);
  return container;
};

/**
 * @param {!cr.ui.List} list Drop target list
 * @param {!Event} event A dragstart event of DOM.
 * @private
 */
FileTransferController.prototype.onDragStart_ = function(list, event) {
  // Check if a drag selection should be initiated or not.
  if (list.shouldStartDragSelection(event)) {
    event.preventDefault();
    // If this drag operation is initiated by mouse, start selecting area.
    if (!this.touching_)
      this.dragSelector_.startDragSelection(list, event);
    return;
  }

  // Nothing selected.
  if (!this.selectionHandler_.selection.entries.length) {
    event.preventDefault();
    return;
  }

  var dt = event.dataTransfer;
  var canCopy = this.canCopyOrDrag_();
  var canCut = this.canCutOrDrag_();
  if (canCopy || canCut) {
    if (canCopy && canCut) {
      this.cutOrCopy_(dt, 'all');
    } else if (canCopy) {
      this.cutOrCopy_(dt, 'copyLink');
    } else {
      this.cutOrCopy_(dt, 'move');
    }
  } else {
    event.preventDefault();
    return;
  }

  var dragThumbnail = this.renderThumbnail_();
  dt.setDragImage(dragThumbnail, 0, 0);

  window[DRAG_AND_DROP_GLOBAL_DATA] = {
    sourceRootURL: dt.getData('fs/sourceRootURL'),
    missingFileContents: dt.getData('fs/missingFileContents')
  };
};

/**
 * @param {!cr.ui.List} list Drop target list.
 * @param {!Event} event A dragend event of DOM.
 * @private
 */
FileTransferController.prototype.onDragEnd_ = function(list, event) {
  // TODO(fukino): This is workaround for crbug.com/373125.
  // This should be removed after the bug is fixed.
  this.touching_ = false;

  var container = this.document_.querySelector('#drag-container');
  container.textContent = '';
  this.clearDropTarget_();
  delete window[DRAG_AND_DROP_GLOBAL_DATA];
};

/**
 * @param {boolean} onlyIntoDirectories True if the drag is only into
 *     directories.
 * @param {(!cr.ui.List|!DirectoryTree)} list Drop target list.
 * @param {Event} event A dragover event of DOM.
 * @private
 */
FileTransferController.prototype.onDragOver_ =
    function(onlyIntoDirectories, list, event) {
  event.preventDefault();
  var entry = this.destinationEntry_;
  if (!entry && !onlyIntoDirectories)
    entry = this.directoryModel_.getCurrentDirEntry();
  event.dataTransfer.dropEffect = this.selectDropEffect_(event, entry);
  event.preventDefault();
};

/**
 * @param {(!cr.ui.List|!DirectoryTree)} list Drop target list.
 * @param {!Event} event A dragenter event of DOM.
 * @private
 */
FileTransferController.prototype.onDragEnterFileList_ = function(list, event) {
  event.preventDefault();  // Required to prevent the cursor flicker.
  this.lastEnteredTarget_ = event.target;
  var item = list.getListItemAncestor(
      /** @type {HTMLElement} */ (event.target));
  item = item && list.isItem(item) ? item : null;
  if (item === this.dropTarget_)
    return;

  var entry = item && list.dataModel.item(item.listIndex);
  if (entry)
    this.setDropTarget_(item, event.dataTransfer, entry);
  else
    this.clearDropTarget_();
};

/**
 * @param {!DirectoryTree} tree Drop target tree.
 * @param {!Event} event A dragenter event of DOM.
 * @private
 */
FileTransferController.prototype.onDragEnterTree_ = function(tree, event) {
  event.preventDefault();  // Required to prevent the cursor flicker.
  this.lastEnteredTarget_ = event.target;
  var item = event.target;
  while (item && !(item instanceof cr.ui.TreeItem)) {
    item = item.parentNode;
  }

  if (item === this.dropTarget_)
    return;

  var entry = item && item.entry;
  if (entry) {
    this.setDropTarget_(item, event.dataTransfer, entry);
  } else {
    this.clearDropTarget_();
  }
};

/**
 * @param {*} list Drop target list.
 * @param {Event} event A dragleave event of DOM.
 * @private
 */
FileTransferController.prototype.onDragLeave_ = function(list, event) {
  // If mouse moves from one element to another the 'dragenter'
  // event for the new element comes before the 'dragleave' event for
  // the old one. In this case event.target !== this.lastEnteredTarget_
  // and handler of the 'dragenter' event has already caried of
  // drop target. So event.target === this.lastEnteredTarget_
  // could only be if mouse goes out of listened element.
  if (event.target === this.lastEnteredTarget_) {
    this.clearDropTarget_();
    this.lastEnteredTarget_ = null;
  }
};

/**
 * @param {boolean} onlyIntoDirectories True if the drag is only into
 *     directories.
 * @param {!Event} event A dragleave event of DOM.
 * @private
 */
FileTransferController.prototype.onDrop_ =
    function(onlyIntoDirectories, event) {
  if (onlyIntoDirectories && !this.dropTarget_)
    return;
  var destinationEntry = this.destinationEntry_ ||
                         this.directoryModel_.getCurrentDirEntry();
  if (!this.canPasteOrDrop_(event.dataTransfer, destinationEntry))
    return;
  event.preventDefault();
  this.paste(event.dataTransfer,
             /** @type {DirectoryEntry} */ (destinationEntry),
             this.selectDropEffect_(event, destinationEntry));
  this.clearDropTarget_();
};

/**
 * Sets the drop target.
 *
 * @param {Element} domElement Target of the drop.
 * @param {!ClipboardData} clipboardData Data transfer object.
 * @param {!DirectoryEntry} destinationEntry Destination entry.
 * @private
 */
FileTransferController.prototype.setDropTarget_ =
    function(domElement, clipboardData, destinationEntry) {
  if (this.dropTarget_ === domElement)
    return;

  // Remove the old drop target.
  this.clearDropTarget_();

  // Set the new drop target.
  this.dropTarget_ = domElement;

  if (!domElement ||
      !destinationEntry.isDirectory ||
      !this.canPasteOrDrop_(clipboardData, destinationEntry)) {
    return;
  }

  // Add accept class if the domElement can accept the drag.
  domElement.classList.add('accepts');
  this.destinationEntry_ = destinationEntry;

  // Start timer changing the directory.
  this.navigateTimer_ = setTimeout(function() {
    if (domElement instanceof DirectoryItem) {
      // Do custom action.
      /** @type {DirectoryItem} */ (domElement).doDropTargetAction();
    }
    this.directoryModel_.changeDirectoryEntry(destinationEntry);
  }.bind(this), 2000);
};

/**
 * Handles touch start.
 * @private
 */
FileTransferController.prototype.onTouchStart_ = function() {
  this.touching_ = true;
};

/**
 * Handles touch end.
 * @private
 */
FileTransferController.prototype.onTouchEnd_ = function() {
  // TODO(fukino): We have to check if event.touches.length be 0 to support
  // multi-touch operations, but event.touches has incorrect value by a bug
  // (crbug.com/373125).
  // After the bug is fixed, we should check event.touches.
  this.touching_ = false;
};

/**
 * Clears the drop target.
 * @private
 */
FileTransferController.prototype.clearDropTarget_ = function() {
  if (this.dropTarget_ && this.dropTarget_.classList.contains('accepts'))
    this.dropTarget_.classList.remove('accepts');
  this.dropTarget_ = null;
  this.destinationEntry_ = null;
  if (this.navigateTimer_ !== undefined) {
    clearTimeout(this.navigateTimer_);
    this.navigateTimer_ = 0;
  }
};

/**
 * @return {boolean} Returns false if {@code <input type="text">} element is
 *     currently active. Otherwise, returns true.
 * @private
 */
FileTransferController.prototype.isDocumentWideEvent_ = function() {
  return this.document_.activeElement.nodeName.toLowerCase() !== 'input' ||
      this.document_.activeElement.type.toLowerCase() !== 'text';
};

/**
 * @param {!Event} event
 * @private
 */
FileTransferController.prototype.onCopy_ = function(event) {
  if (!this.isDocumentWideEvent_() ||
      !this.canCopyOrDrag_()) {
    return;
  }
  event.preventDefault();
  this.cutOrCopy_(assert(event.clipboardData), 'copy');
  this.blinkSelection_();
};

/**
 * @param {!Event} event
 * @private
 */
FileTransferController.prototype.onBeforeCopy_ = function(event) {
  if (!this.isDocumentWideEvent_())
    return;

  // queryCommandEnabled returns true if event.defaultPrevented is true.
  if (this.canCopyOrDrag_())
    event.preventDefault();
};

/**
 * @return {boolean} Returns true if some files are selected and all the file
 *     on drive is available to be copied. Otherwise, returns false.
 * @private
 */
FileTransferController.prototype.canCopyOrDrag_ = function() {
  return this.selectionHandler_.isAvailable() &&
      this.selectionHandler_.selection.entries.length > 0;
};

/**
 * @param {!Event} event
 * @private
 */
FileTransferController.prototype.onCut_ = function(event) {
  if (!this.isDocumentWideEvent_() ||
      !this.canCutOrDrag_()) {
    return;
  }
  event.preventDefault();
  this.cutOrCopy_(assert(event.clipboardData), 'move');
  this.blinkSelection_();
};

/**
 * @param {!Event} event
 * @private
 */
FileTransferController.prototype.onBeforeCut_ = function(event) {
  if (!this.isDocumentWideEvent_())
    return;
  // queryCommandEnabled returns true if event.defaultPrevented is true.
  if (this.canCutOrDrag_())
    event.preventDefault();
};

/**
 * @return {boolean} Returns true if the current directory is not read only.
 * @private
 */
FileTransferController.prototype.canCutOrDrag_ = function() {
  return !this.directoryModel_.isReadOnly() &&
      this.selectionHandler_.selection.entries.length > 0;
};

/**
 * @param {!Event} event
 * @private
 */
FileTransferController.prototype.onPaste_ = function(event) {
  // If the event has destDirectory property, paste files into the directory.
  // This occurs when the command fires from menu item 'Paste into folder'.
  var destination =
      event.destDirectory || this.directoryModel_.getCurrentDirEntry();

  // Need to update here since 'beforepaste' doesn't fire.
  if (!this.isDocumentWideEvent_() ||
      !this.canPasteOrDrop_(assert(event.clipboardData), destination)) {
    return;
  }
  event.preventDefault();
  var effect = this.paste(assert(event.clipboardData), destination);

  // On cut, we clear the clipboard after the file is pasted/moved so we don't
  // try to move/delete the original file again.
  if (effect === 'move') {
    this.simulateCommand_('cut', function(event) {
      event.preventDefault();
      event.clipboardData.setData('fs/clear', '');
    });
  }
};

/**
 * @param {!Event} event
 * @private
 */
FileTransferController.prototype.onBeforePaste_ = function(event) {
  if (!this.isDocumentWideEvent_())
    return;
  // queryCommandEnabled returns true if event.defaultPrevented is true.
  if (this.canPasteOrDrop_(assert(event.clipboardData),
                           this.directoryModel_.getCurrentDirEntry())) {
    event.preventDefault();
  }
};

/**
 * @param {!ClipboardData} clipboardData Clipboard data object.
 * @param {DirectoryEntry|FakeEntry} destinationEntry Destination entry.
 * @return {boolean} Returns true if items stored in {@code clipboardData} can
 *     be pasted to {@code destinationEntry}. Otherwise, returns false.
 * @private
 */
FileTransferController.prototype.canPasteOrDrop_ =
    function(clipboardData, destinationEntry) {
  if (!destinationEntry)
    return false;
  var destinationLocationInfo =
      this.volumeManager_.getLocationInfo(destinationEntry);
  if (!destinationLocationInfo || destinationLocationInfo.isReadOnly)
    return false;
  if (!clipboardData.types || clipboardData.types.indexOf('fs/tag') === -1)
    return false;  // Unsupported type of content.

  // Copying between different sources requires all files to be available.
  if (this.getSourceRootURL_(clipboardData) !==
      destinationLocationInfo.volumeInfo.fileSystem.root.toURL() &&
      this.isMissingFileContents_(clipboardData))
    return false;

  return true;
};

/**
 * Execute paste command.
 *
 * @return {boolean}  Returns true, the paste is success. Otherwise, returns
 *     false.
 */
FileTransferController.prototype.queryPasteCommandEnabled = function() {
  if (!this.isDocumentWideEvent_()) {
    return false;
  }

  // HACK(serya): return this.document_.queryCommandEnabled('paste')
  // should be used.
  var result;
  this.simulateCommand_('paste', function(event) {
    result = this.canPasteOrDrop_(event.clipboardData,
                                  this.directoryModel_.getCurrentDirEntry());
  }.bind(this));
  return result;
};

/**
 * Allows to simulate commands to get access to clipboard.
 *
 * @param {string} command 'copy', 'cut' or 'paste'.
 * @param {function(Event)} handler Event handler.
 * @private
 */
FileTransferController.prototype.simulateCommand_ = function(command, handler) {
  var iframe = this.document_.querySelector('#command-dispatcher');
  var doc = iframe.contentDocument;
  doc.addEventListener(command, handler);
  doc.execCommand(command);
  doc.removeEventListener(command, handler);
};

/**
 * @private
 */
FileTransferController.prototype.onFileSelectionChanged_ = function() {
  this.preloadedThumbnailImagePromise_ = null;
  this.selectedAsyncData_ = {};
};

/**
 * @private
 */
FileTransferController.prototype.onFileSelectionChangedThrottled_ = function() {
  var entries = this.selectionHandler_.selection.entries;
  var asyncData = this.selectedAsyncData_;
  var fileEntries = [];
  for (var i = 0; i < entries.length; i++) {
    if (entries[i].isFile)
      fileEntries.push(entries[i]);
    asyncData[entries[i].toURL()] = {externalFileUrl: '', file: null};
  }
  var containsDirectory = this.selectionHandler_.selection.directoryCount > 0;

  // File object must be prepeared in advance for clipboard operations
  // (copy, paste and drag). DataTransfer object closes for write after
  // returning control from that handlers so they may not have
  // asynchronous operations.
  if (!containsDirectory) {
    for (var i = 0; i < fileEntries.length; i++) {
      (function(fileEntry) {
        fileEntry.file(function(file) {
          asyncData[fileEntry.toURL()].file = file;
        });
      })(fileEntries[i]);
    }
  }

  if (entries.length === 1) {
    // For single selection, the dragged element is created in advance,
    // otherwise an image may not be loaded at the time the 'dragstart' event
    // comes.
    this.preloadThumbnailImage_(entries[0]);
  }

  this.metadataModel_.get(entries, ['externalFileUrl']).then(
      function(metadataList) {
        // |Copy| is the only menu item affected by allDriveFilesAvailable_.
        // It could be open right now, update its UI.
        this.copyCommand_.disabled = !this.canCopyOrDrag_();
        for (var i = 0; i < entries.length; i++) {
          if (entries[i].isFile) {
            asyncData[entries[i].toURL()].externalFileUrl =
                metadataList[i].externalFileUrl;
          }
        }
      }.bind(this));
};

/**
 * @param {!Event} event Drag event.
 * @param {DirectoryEntry|FakeEntry} destinationEntry Destination entry.
 * @return {string}  Returns the appropriate drop query type ('none', 'move'
 *     or copy') to the current modifiers status and the destination.
 * @private
 */
FileTransferController.prototype.selectDropEffect_ =
    function(event, destinationEntry) {
  if (!destinationEntry)
    return 'none';
  var destinationLocationInfo =
      this.volumeManager_.getLocationInfo(destinationEntry);
  if (!destinationLocationInfo)
    return 'none';
  if (destinationLocationInfo.isReadOnly)
    return 'none';
  if (util.isDropEffectAllowed(event.dataTransfer.effectAllowed, 'move')) {
    if (!util.isDropEffectAllowed(event.dataTransfer.effectAllowed, 'copy'))
      return 'move';
    // TODO(mtomasz): Use volumeId instead of comparing roots, as soon as
    // volumeId gets unique.
    if (this.getSourceRootURL_(event.dataTransfer) ===
            destinationLocationInfo.volumeInfo.fileSystem.root.toURL() &&
        !event.ctrlKey) {
      return 'move';
    }
    if (event.shiftKey) {
      return 'move';
    }
  }
  return 'copy';
};

/**
 * Blinks the selection. Used to give feedback when copying or cutting the
 * selection.
 * @private
 */
FileTransferController.prototype.blinkSelection_ = function() {
  var selection = this.selectionHandler_.selection;
  if (!selection || selection.totalCount == 0)
    return;

  var listItems = [];
  for (var i = 0; i < selection.entries.length; i++) {
    var selectedIndex = selection.indexes[i];
    var listItem =
        this.listContainer_.currentList.getListItemByIndex(selectedIndex);
    if (listItem) {
      listItem.classList.add('blink');
      listItems.push(listItem);
    }
  }

  setTimeout(function() {
    for (var i = 0; i < listItems.length; i++) {
      listItems[i].classList.remove('blink');
    }
  }, 100);
};
