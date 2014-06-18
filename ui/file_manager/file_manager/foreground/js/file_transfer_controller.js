// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * Global (placed in the window object) variable name to hold internal
 * file dragging information. Needed to show visual feedback while dragging
 * since DataTransfer object is in protected state. Reachable from other
 * file manager instances.
 */
var DRAG_AND_DROP_GLOBAL_DATA = '__drag_and_drop_global_data';

/**
 * @param {HTMLDocument} doc Owning document.
 * @param {FileOperationManager} fileOperationManager File operation manager
 *     instance.
 * @param {MetadataCache} metadataCache Metadata cache service.
 * @param {DirectoryModel} directoryModel Directory model instance.
 * @param {VolumeManagerWrapper} volumeManager Volume manager instance.
 * @param {MultiProfileShareDialog} multiProfileShareDialog Share dialog to be
 *     used to share files from another profile.
 * @constructor
 */
function FileTransferController(doc,
                                fileOperationManager,
                                metadataCache,
                                directoryModel,
                                volumeManager,
                                multiProfileShareDialog) {
  this.document_ = doc;
  this.fileOperationManager_ = fileOperationManager;
  this.metadataCache_ = metadataCache;
  this.directoryModel_ = directoryModel;
  this.volumeManager_ = volumeManager;
  this.multiProfileShareDialog_ = multiProfileShareDialog;

  this.directoryModel_.getFileList().addEventListener(
      'change', function(event) {
    if (this.directoryModel_.getFileListSelection().
        getIndexSelected(event.index)) {
      this.onSelectionChanged_();
    }
  }.bind(this));
  this.directoryModel_.getFileListSelection().addEventListener('change',
      this.onSelectionChanged_.bind(this));

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
   * @type {Array.<File>}
   * @private
   */
  this.selectedFileObjects_ = [];

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
}

/**
 * Size of drag thumbnail for image files.
 *
 * @type {number}
 * @const
 * @private
 */
FileTransferController.DRAG_THUMBNAIL_SIZE_ = 64;

FileTransferController.prototype = {
  __proto__: cr.EventTarget.prototype,

  /**
   * @this {FileTransferController}
   * @param {cr.ui.List} list Items in the list will be draggable.
   */
  attachDragSource: function(list) {
    list.style.webkitUserDrag = 'element';
    list.addEventListener('dragstart', this.onDragStart_.bind(this, list));
    list.addEventListener('dragend', this.onDragEnd_.bind(this, list));
    list.addEventListener('touchstart', this.onTouchStart_.bind(this));
    list.ownerDocument.addEventListener(
        'touchend', this.onTouchEnd_.bind(this), true);
    list.ownerDocument.addEventListener(
        'touchcancel', this.onTouchEnd_.bind(this), true);
  },

  /**
   * @this {FileTransferController}
   * @param {cr.ui.List} list List itself and its directory items will could
   *                          be drop target.
   * @param {boolean=} opt_onlyIntoDirectories If true only directory list
   *     items could be drop targets. Otherwise any other place of the list
   *     accetps files (putting it into the current directory).
   */
  attachFileListDropTarget: function(list, opt_onlyIntoDirectories) {
    list.addEventListener('dragover', this.onDragOver_.bind(this,
        !!opt_onlyIntoDirectories, list));
    list.addEventListener('dragenter',
        this.onDragEnterFileList_.bind(this, list));
    list.addEventListener('dragleave', this.onDragLeave_.bind(this, list));
    list.addEventListener('drop',
        this.onDrop_.bind(this, !!opt_onlyIntoDirectories));
  },

  /**
   * @this {FileTransferController}
   * @param {DirectoryTree} tree Its sub items will could be drop target.
   */
  attachTreeDropTarget: function(tree) {
    tree.addEventListener('dragover', this.onDragOver_.bind(this, true, tree));
    tree.addEventListener('dragenter', this.onDragEnterTree_.bind(this, tree));
    tree.addEventListener('dragleave', this.onDragLeave_.bind(this, tree));
    tree.addEventListener('drop', this.onDrop_.bind(this, true));
  },

  /**
   * @this {FileTransferController}
   * @param {NavigationList} tree Its sub items will could be drop target.
   */
  attachNavigationListDropTarget: function(list) {
    list.addEventListener('dragover',
        this.onDragOver_.bind(this, true /* onlyIntoDirectories */, list));
    list.addEventListener('dragenter',
        this.onDragEnterVolumesList_.bind(this, list));
    list.addEventListener('dragleave', this.onDragLeave_.bind(this, list));
    list.addEventListener('drop',
        this.onDrop_.bind(this, true /* onlyIntoDirectories */));
  },

  /**
   * Attach handlers of copy, cut and paste operations to the document.
   *
   * @this {FileTransferController}
   */
  attachCopyPasteHandlers: function() {
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
    this.copyCommand_ = this.document_.querySelector('command#copy');
  },

  /**
   * Write the current selection to system clipboard.
   *
   * @this {FileTransferController}
   * @param {DataTransfer} dataTransfer DataTransfer from the event.
   * @param {string} effectAllowed Value must be valid for the
   *     |dataTransfer.effectAllowed| property.
   */
  cutOrCopy_: function(dataTransfer, effectAllowed) {
    // Existence of the volumeInfo is checked in canXXX methods.
    var volumeInfo = this.volumeManager_.getVolumeInfo(
        this.currentDirectoryContentEntry);
    // Tag to check it's filemanager data.
    dataTransfer.setData('fs/tag', 'filemanager-data');
    dataTransfer.setData('fs/sourceRootURL',
                         volumeInfo.fileSystem.root.toURL());
    var sourceURLs = util.entriesToURLs(this.selectedEntries_);
    dataTransfer.setData('fs/sources', sourceURLs.join('\n'));
    dataTransfer.effectAllowed = effectAllowed;
    dataTransfer.setData('fs/effectallowed', effectAllowed);
    dataTransfer.setData('fs/missingFileContents',
                         !this.isAllSelectedFilesAvailable_());

    for (var i = 0; i < this.selectedFileObjects_.length; i++) {
      dataTransfer.items.add(this.selectedFileObjects_[i]);
    }
  },

  /**
   * @this {FileTransferController}
   * @return {Object.<string, string>} Drag and drop global data object.
   */
  getDragAndDropGlobalData_: function() {
    if (window[DRAG_AND_DROP_GLOBAL_DATA])
      return window[DRAG_AND_DROP_GLOBAL_DATA];

    // Dragging from other tabs/windows.
    var views = chrome && chrome.extension ? chrome.extension.getViews() : [];
    for (var i = 0; i < views.length; i++) {
      if (views[i][DRAG_AND_DROP_GLOBAL_DATA])
        return views[i][DRAG_AND_DROP_GLOBAL_DATA];
    }
    return null;
  },

  /**
   * Extracts source root URL from the |dataTransfer| object.
   *
   * @this {FileTransferController}
   * @param {DataTransfer} dataTransfer DataTransfer object from the event.
   * @return {string} URL or an empty string (if unknown).
   */
  getSourceRootURL_: function(dataTransfer) {
    var sourceRootURL = dataTransfer.getData('fs/sourceRootURL');
    if (sourceRootURL)
      return sourceRootURL;

    // |dataTransfer| in protected mode.
    var globalData = this.getDragAndDropGlobalData_();
    if (globalData)
      return globalData.sourceRootURL;

    // Unknown source.
    return '';
  },

  /**
   * @this {FileTransferController}
   * @param {DataTransfer} dataTransfer DataTransfer object from the event.
   * @return {boolean} Returns true when missing some file contents.
   */
  isMissingFileContents_: function(dataTransfer) {
    var data = dataTransfer.getData('fs/missingFileContents');
    if (!data) {
      // |dataTransfer| in protected mode.
      var globalData = this.getDragAndDropGlobalData_();
      if (globalData)
        data = globalData.missingFileContents;
    }
    return data === 'true';
  },

  /**
   * Obtains entries that need to share with me.
   * The method also observers child entries of the given entries.
   * @param {Array.<Entries>} entries Entries.
   * @return {Promise} Promise to be fulfilled with the entries that need to
   *     share.
   */
  getMultiProfileShareEntries_: function(entries) {
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
        var urls = util.entriesToURLs(entries);
        chrome.fileBrowserPrivate.getDriveEntryProperties(urls, callback);
      }).
      then(function(metadatas) {
        return entries.filter(function(entry, i) {
          var metadata = metadatas[i];
          return metadata && metadata.isHosted && !metadata.sharedWithMe;
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
  },

  /**
   * Queue up a file copy operation based on the current system clipboard.
   *
   * @this {FileTransferController}
   * @param {DataTransfer} dataTransfer System data transfer object.
   * @param {DirectoryEntry=} opt_destinationEntry Paste destination.
   * @param {string=} opt_effect Desired drop/paste effect. Could be
   *     'move'|'copy' (default is copy). Ignored if conflicts with
   *     |dataTransfer.effectAllowed|.
   * @return {string} Either "copy" or "move".
   */
  paste: function(dataTransfer, opt_destinationEntry, opt_effect) {
    var sourceURLs = dataTransfer.getData('fs/sources') ?
        dataTransfer.getData('fs/sources').split('\n') : [];
    // effectAllowed set in copy/paste handlers stay uninitialized. DnD handlers
    // work fine.
    var effectAllowed = dataTransfer.effectAllowed !== 'uninitialized' ?
        dataTransfer.effectAllowed : dataTransfer.getData('fs/effectallowed');
    var toMove = util.isDropEffectAllowed(effectAllowed, 'move') &&
        (!util.isDropEffectAllowed(effectAllowed, 'copy') ||
         opt_effect === 'move');
    var destinationEntry =
        opt_destinationEntry || this.currentDirectoryContentEntry;
    var entries;
    var failureUrls;

    util.URLsToEntries(sourceURLs).
    then(function(result) {
      entries = result.entries;
      failureUrls = result.failureUrls;
      // Check if cross share is needed or not.
      return this.getMultiProfileShareEntries_(entries);
    }.bind(this)).
    then(function(shareEntries) {
      if (shareEntries.length === 0)
        return;
      return this.multiProfileShareDialog_.show(shareEntries.length > 1).
          then(function(dialogResult) {
            if (dialogResult === 'cancel')
              return Promise.reject('ABORT');
            // Do cross share.
            // TODO(hirono): Make the loop cancellable.
            var requestDriveShare = function(index) {
              if (index >= shareEntries.length)
                return;
              return new Promise(function(fulfill) {
                chrome.fileBrowserPrivate.requestDriveShare(
                    shareEntries[index].toURL(),
                    dialogResult,
                    function() {
                      // TODO(hirono): Check chrome.runtime.lastError here.
                      fulfill();
                    });
              }).then(requestDriveShare.bind(null, index + 1));
            };
            return requestDriveShare(0);
          });
    }.bind(this)).
    then(function() {
      // Start the pasting operation.
      this.fileOperationManager_.paste(
          entries, destinationEntry, toMove);

      // Publish events for failureUrls.
      for (var i = 0; i < failureUrls.length; i++) {
        var fileName =
            decodeURIComponent(failureUrls[i].replace(/^.+\//, ''));
        var event = new Event('source-not-found');
        event.fileName = fileName;
        event.progressType =
            toMove ? ProgressItemType.MOVE : ProgressItemType.COPY;
        this.dispatchEvent(event);
      }
    }.bind(this)).
    catch(function(error) {
      if (error !== 'ABORT')
        console.error(error.stack ? error.stack : error);
    });
    return toMove ? 'move' : 'copy';
  },

  /**
   * Preloads an image thumbnail for the specified file entry.
   *
   * @this {FileTransferController}
   * @param {Entry} entry Entry to preload a thumbnail for.
   */
  preloadThumbnailImage_: function(entry) {
    var metadataPromise = new Promise(function(fulfill, reject) {
      this.metadataCache_.getOne(entry,
                                 'thumbnail|filesystem',
                                 function(metadata) {
        if (metadata)
          fulfill(metadata);
        else
          reject('Failed to fetch metadata.');
      });
    }.bind(this));

    var imagePromise = metadataPromise.then(function(metadata) {
      return new Promise(function(fulfill, reject) {
        var loader = new ThumbnailLoader(
            entry, ThumbnailLoader.LoaderType.Image, metadata);
        loader.loadDetachedImage(function(result) {
          if (result)
            fulfill(loader.getImage());
        });
      });
    });

    imagePromise.then(function(image) {
      // Store the image so that we can obtain the image synchronously.
      imagePromise.value = image;
    }, function(error) {
      console.error(error.stack || error);
    });

    this.preloadedThumbnailImagePromise_ = imagePromise;
  },

  /**
   * Renders a drag-and-drop thumbnail.
   *
   * @this {FileTransferController}
   * @return {HTMLElement} Element containing the thumbnail.
   */
  renderThumbnail_: function() {
    var length = this.selectedEntries_.length;

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
    var entry = this.selectedEntries_[0];
    var icon = this.document_.createElement('div');
    icon.className = 'detail-icon';
    icon.setAttribute('file-type-icon', FileType.getIcon(entry));
    contents.appendChild(icon);
    var label = this.document_.createElement('div');
    label.className = 'label';
    label.textContent = entry.name;
    contents.appendChild(label);
    return container;
  },

  /**
   * @this {FileTransferController}
   * @param {cr.ui.List} list Drop target list
   * @param {Event} event A dragstart event of DOM.
   */
  onDragStart_: function(list, event) {
    // Check if a drag selection should be initiated or not.
    if (list.shouldStartDragSelection(event)) {
      event.preventDefault();
      // If this drag operation is initiated by mouse, start selecting area.
      if (!this.touching_)
        this.dragSelector_.startDragSelection(list, event);
      return;
    }

    // Nothing selected.
    if (!this.selectedEntries_.length) {
      event.preventDefault();
      return;
    }

    var dt = event.dataTransfer;
    var canCopy = this.canCopyOrDrag_(dt);
    var canCut = this.canCutOrDrag_(dt);
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
      missingFileContents: dt.getData('fs/missingFileContents'),
    };
  },

  /**
   * @this {FileTransferController}
   * @param {cr.ui.List} list Drop target list.
   * @param {Event} event A dragend event of DOM.
   */
  onDragEnd_: function(list, event) {
    // TODO(fukino): This is workaround for crbug.com/373125.
    // This should be removed after the bug is fixed.
    this.touching_ = false;

    var container = this.document_.querySelector('#drag-container');
    container.textContent = '';
    this.clearDropTarget_();
    delete window[DRAG_AND_DROP_GLOBAL_DATA];
  },

  /**
   * @this {FileTransferController}
   * @param {boolean} onlyIntoDirectories True if the drag is only into
   *     directories.
   * @param {cr.ui.List} list Drop target list.
   * @param {Event} event A dragover event of DOM.
   */
  onDragOver_: function(onlyIntoDirectories, list, event) {
    event.preventDefault();
    var entry = this.destinationEntry_ ||
        (!onlyIntoDirectories && this.currentDirectoryContentEntry);
    event.dataTransfer.dropEffect = this.selectDropEffect_(event, entry);
    event.preventDefault();
  },

  /**
   * @this {FileTransferController}
   * @param {cr.ui.List} list Drop target list.
   * @param {Event} event A dragenter event of DOM.
   */
  onDragEnterFileList_: function(list, event) {
    event.preventDefault();  // Required to prevent the cursor flicker.
    this.lastEnteredTarget_ = event.target;
    var item = list.getListItemAncestor(event.target);
    item = item && list.isItem(item) ? item : null;
    if (item === this.dropTarget_)
      return;

    var entry = item && list.dataModel.item(item.listIndex);
    if (entry)
      this.setDropTarget_(item, event.dataTransfer, entry);
    else
      this.clearDropTarget_();
  },

  /**
   * @this {FileTransferController}
   * @param {DirectoryTree} tree Drop target tree.
   * @param {Event} event A dragenter event of DOM.
   */
  onDragEnterTree_: function(tree, event) {
    event.preventDefault();  // Required to prevent the cursor flicker.
    this.lastEnteredTarget_ = event.target;
    var item = event.target;
    while (item && !(item instanceof DirectoryItem)) {
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
  },

  /**
   * @this {FileTransferController}
   * @param {NavigationList} list Drop target list.
   * @param {Event} event A dragenter event of DOM.
   */
  onDragEnterVolumesList_: function(list, event) {
    event.preventDefault();  // Required to prevent the cursor flicker.

    this.lastEnteredTarget_ = event.target;
    var item = list.getListItemAncestor(event.target);
    item = item && list.isItem(item) ? item : null;
    if (item === this.dropTarget_)
      return;

    var modelItem = item && list.dataModel.item(item.listIndex);
    if (modelItem && modelItem.isShortcut) {
      this.setDropTarget_(item, event.dataTransfer, modelItem.entry);
      return;
    }
    if (modelItem && modelItem.isVolume && modelItem.volumeInfo.displayRoot) {
      this.setDropTarget_(
          item, event.dataTransfer, modelItem.volumeInfo.displayRoot);
      return;
    }

    this.clearDropTarget_();
  },

  /**
   * @this {FileTransferController}
   * @param {cr.ui.List} list Drop target list.
   * @param {Event} event A dragleave event of DOM.
   */
  onDragLeave_: function(list, event) {
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
  },

  /**
   * @this {FileTransferController}
   * @param {boolean} onlyIntoDirectories True if the drag is only into
   *     directories.
   * @param {Event} event A dragleave event of DOM.
   */
  onDrop_: function(onlyIntoDirectories, event) {
    if (onlyIntoDirectories && !this.dropTarget_)
      return;
    var destinationEntry = this.destinationEntry_ ||
                           this.currentDirectoryContentEntry;
    if (!this.canPasteOrDrop_(event.dataTransfer, destinationEntry))
      return;
    event.preventDefault();
    this.paste(event.dataTransfer, destinationEntry,
               this.selectDropEffect_(event, destinationEntry));
    this.clearDropTarget_();
  },

  /**
   * Sets the drop target.
   *
   * @this {FileTransferController}
   * @param {Element} domElement Target of the drop.
   * @param {DataTransfer} dataTransfer Data transfer object.
   * @param {DirectoryEntry} destinationEntry Destination entry.
   */
  setDropTarget_: function(domElement, dataTransfer, destinationEntry) {
    if (this.dropTarget_ === domElement)
      return;

    // Remove the old drop target.
    this.clearDropTarget_();

    // Set the new drop target.
    this.dropTarget_ = domElement;

    if (!domElement ||
        !destinationEntry.isDirectory ||
        !this.canPasteOrDrop_(dataTransfer, destinationEntry)) {
      return;
    }

    // Add accept class if the domElement can accept the drag.
    domElement.classList.add('accepts');
    this.destinationEntry_ = destinationEntry;

    // Start timer changing the directory.
    this.navigateTimer_ = setTimeout(function() {
      if (domElement instanceof DirectoryItem)
        // Do custom action.
        (/** @type {DirectoryItem} */ domElement).doDropTargetAction();
      this.directoryModel_.changeDirectoryEntry(destinationEntry);
    }.bind(this), 2000);
  },

  /**
   * Handles touch start.
   */
  onTouchStart_: function() {
    this.touching_ = true;
  },

  /**
   * Handles touch end.
   */
  onTouchEnd_: function(event) {
    // TODO(fukino): We have to check if event.touches.length be 0 to support
    // multi-touch operations, but event.touches has incorrect value by a bug
    // (crbug.com/373125).
    // After the bug is fixed, we should check event.touches.
    this.touching_ = false;
  },

  /**
   * Clears the drop target.
   * @this {FileTransferController}
   */
  clearDropTarget_: function() {
    if (this.dropTarget_ && this.dropTarget_.classList.contains('accepts'))
      this.dropTarget_.classList.remove('accepts');
    this.dropTarget_ = null;
    this.destinationEntry_ = null;
    if (this.navigateTimer_ !== undefined) {
      clearTimeout(this.navigateTimer_);
      this.navigateTimer_ = undefined;
    }
  },

  /**
   * @this {FileTransferController}
   * @return {boolean} Returns false if {@code <input type="text">} element is
   *     currently active. Otherwise, returns true.
   */
  isDocumentWideEvent_: function() {
    return this.document_.activeElement.nodeName.toLowerCase() !== 'input' ||
        this.document_.activeElement.type.toLowerCase() !== 'text';
  },

  /**
   * @this {FileTransferController}
   */
  onCopy_: function(event) {
    if (!this.isDocumentWideEvent_() ||
        !this.canCopyOrDrag_()) {
      return;
    }
    event.preventDefault();
    this.cutOrCopy_(event.clipboardData, 'copy');
    this.notify_('selection-copied');
  },

  /**
   * @this {FileTransferController}
   */
  onBeforeCopy_: function(event) {
    if (!this.isDocumentWideEvent_())
      return;

    // queryCommandEnabled returns true if event.defaultPrevented is true.
    if (this.canCopyOrDrag_())
      event.preventDefault();
  },

  /**
   * @this {FileTransferController}
   * @return {boolean} Returns true if all selected files are available to be
   *     copied.
   */
  isAllSelectedFilesAvailable_: function() {
    if (!this.currentDirectoryContentEntry)
      return false;
    var volumeInfo = this.volumeManager_.getVolumeInfo(
        this.currentDirectoryContentEntry);
    if (!volumeInfo)
      return false;
    var isDriveOffline = this.volumeManager_.getDriveConnectionState().type ===
        VolumeManagerCommon.DriveConnectionType.OFFLINE;
    if (this.isOnDrive && isDriveOffline && !this.allDriveFilesAvailable)
      return false;
    return true;
  },

  /**
   * @this {FileTransferController}
   * @return {boolean} Returns true if some files are selected and all the file
   *     on drive is available to be copied. Otherwise, returns false.
   */
  canCopyOrDrag_: function() {
    return this.isAllSelectedFilesAvailable_() &&
        this.selectedEntries_.length > 0;
  },

  /**
   * @this {FileTransferController}
   */
  onCut_: function(event) {
    if (!this.isDocumentWideEvent_() ||
        !this.canCutOrDrag_()) {
      return;
    }
    event.preventDefault();
    this.cutOrCopy_(event.clipboardData, 'move');
    this.notify_('selection-cut');
  },

  /**
   * @this {FileTransferController}
   */
  onBeforeCut_: function(event) {
    if (!this.isDocumentWideEvent_())
      return;
    // queryCommandEnabled returns true if event.defaultPrevented is true.
    if (this.canCutOrDrag_())
      event.preventDefault();
  },

  /**
   * @this {FileTransferController}
   * @return {boolean} Returns true if the current directory is not read only.
   */
  canCutOrDrag_: function() {
    return !this.readonly && this.selectedEntries_.length > 0;
  },

  /**
   * @this {FileTransferController}
   */
  onPaste_: function(event) {
    // If the event has destDirectory property, paste files into the directory.
    // This occurs when the command fires from menu item 'Paste into folder'.
    var destination = event.destDirectory || this.currentDirectoryContentEntry;

    // Need to update here since 'beforepaste' doesn't fire.
    if (!this.isDocumentWideEvent_() ||
        !this.canPasteOrDrop_(event.clipboardData, destination)) {
      return;
    }
    event.preventDefault();
    var effect = this.paste(event.clipboardData, destination);

    // On cut, we clear the clipboard after the file is pasted/moved so we don't
    // try to move/delete the original file again.
    if (effect === 'move') {
      this.simulateCommand_('cut', function(event) {
        event.preventDefault();
        event.clipboardData.setData('fs/clear', '');
      });
    }
  },

  /**
   * @this {FileTransferController}
   */
  onBeforePaste_: function(event) {
    if (!this.isDocumentWideEvent_())
      return;
    // queryCommandEnabled returns true if event.defaultPrevented is true.
    if (this.canPasteOrDrop_(event.clipboardData,
                             this.currentDirectoryContentEntry)) {
      event.preventDefault();
    }
  },

  /**
   * @this {FileTransferController}
   * @param {DataTransfer} dataTransfer Data transfer object.
   * @param {DirectoryEntry} destinationEntry Destination entry.
   * @return {boolean} Returns true if items stored in {@code dataTransfer} can
   *     be pasted to {@code destinationEntry}. Otherwise, returns false.
   */
  canPasteOrDrop_: function(dataTransfer, destinationEntry) {
    if (!destinationEntry)
      return false;
    var destinationLocationInfo =
        this.volumeManager_.getLocationInfo(destinationEntry);
    if (!destinationLocationInfo || destinationLocationInfo.isReadOnly)
      return false;
    if (!dataTransfer.types || dataTransfer.types.indexOf('fs/tag') === -1)
      return false;  // Unsupported type of content.

    // Copying between different sources requires all files to be available.
    if (this.getSourceRootURL_(dataTransfer) !==
        destinationLocationInfo.volumeInfo.fileSystem.root.toURL() &&
        this.isMissingFileContents_(dataTransfer))
      return false;

    return true;
  },

  /**
   * Execute paste command.
   *
   * @this {FileTransferController}
   * @return {boolean}  Returns true, the paste is success. Otherwise, returns
   *     false.
   */
  queryPasteCommandEnabled: function() {
    if (!this.isDocumentWideEvent_()) {
      return false;
    }

    // HACK(serya): return this.document_.queryCommandEnabled('paste')
    // should be used.
    var result;
    this.simulateCommand_('paste', function(event) {
      result = this.canPasteOrDrop_(event.clipboardData,
                                    this.currentDirectoryContentEntry);
    }.bind(this));
    return result;
  },

  /**
   * Allows to simulate commands to get access to clipboard.
   *
   * @this {FileTransferController}
   * @param {string} command 'copy', 'cut' or 'paste'.
   * @param {function} handler Event handler.
   */
  simulateCommand_: function(command, handler) {
    var iframe = this.document_.querySelector('#command-dispatcher');
    var doc = iframe.contentDocument;
    doc.addEventListener(command, handler);
    doc.execCommand(command);
    doc.removeEventListener(command, handler);
  },

  /**
   * @this {FileTransferController}
   */
  onSelectionChanged_: function(event) {
    var entries = this.selectedEntries_;
    var files = this.selectedFileObjects_ = [];
    this.preloadedThumbnailImagePromise_ = null;

    var fileEntries = [];
    for (var i = 0; i < entries.length; i++) {
      if (entries[i].isFile)
        fileEntries.push(entries[i]);
    }
    var containsDirectory = fileEntries.length !== entries.length;

    // File object must be prepeared in advance for clipboard operations
    // (copy, paste and drag). DataTransfer object closes for write after
    // returning control from that handlers so they may not have
    // asynchronous operations.
    if (!containsDirectory) {
      for (var i = 0; i < fileEntries.length; i++) {
        fileEntries[i].file(function(file) { files.push(file); });
      }
    }

    if (entries.length === 1) {
      // For single selection, the dragged element is created in advance,
      // otherwise an image may not be loaded at the time the 'dragstart' event
      // comes.
      this.preloadThumbnailImage_(entries[0]);
    }

    if (this.isOnDrive) {
      this.allDriveFilesAvailable = false;
      this.metadataCache_.get(entries, 'drive', function(props) {
        // We consider directories not available offline for the purposes of
        // file transfer since we cannot afford to recursive traversal.
        this.allDriveFilesAvailable =
            !containsDirectory &&
            props.filter(function(p) {
              return !p.availableOffline;
            }).length === 0;
        // |Copy| is the only menu item affected by allDriveFilesAvailable.
        // It could be open right now, update its UI.
        this.copyCommand_.disabled = !this.canCopyOrDrag_();
      }.bind(this));
    }
  },

  /**
   * Obains directory that is displaying now.
   * @this {FileTransferController}
   * @return {DirectoryEntry} Entry of directry that is displaying now.
   */
  get currentDirectoryContentEntry() {
    return this.directoryModel_.getCurrentDirEntry();
  },

  /**
   * @this {FileTransferController}
   * @return {boolean} True if the current directory is read only.
   */
  get readonly() {
    return this.directoryModel_.isReadOnly();
  },

  /**
   * @this {FileTransferController}
   * @return {boolean} True if the current directory is on Drive.
   */
  get isOnDrive() {
    var currentDir = this.directoryModel_.getCurrentDirEntry();
    if (!currentDir)
      return false;
    var locationInfo = this.volumeManager_.getLocationInfo(currentDir);
    if (!locationInfo)
      return false;
    return locationInfo.isDriveBased;
  },

  /**
   * @this {FileTransferController}
   */
  notify_: function(eventName) {
    var self = this;
    // Set timeout to avoid recursive events.
    setTimeout(function() {
      cr.dispatchSimpleEvent(self, eventName);
    }, 0);
  },

  /**
   * @this {FileTransferController}
   * @return {Array.<Entry>} Array of the selected entries.
   */
  get selectedEntries_() {
    var list = this.directoryModel_.getFileList();
    var selectedIndexes = this.directoryModel_.getFileListSelection().
        selectedIndexes;
    var entries = selectedIndexes.map(function(index) {
      return list.item(index);
    });

    // TODO(serya): Diagnostics for http://crbug/129642
    if (entries.indexOf(undefined) !== -1) {
      var index = entries.indexOf(undefined);
      entries = entries.filter(function(e) { return !!e; });
      console.error('Invalid selection found: list items: ', list.length,
                    'wrong indexe value: ', selectedIndexes[index],
                    'Stack trace: ', new Error().stack);
    }
    return entries;
  },

  /**
   * @param {Event} event Drag event.
   * @param {DirectoryEntry} destinationEntry Destination entry.
   * @this {FileTransferController}
   * @return {string}  Returns the appropriate drop query type ('none', 'move'
   *     or copy') to the current modifiers status and the destination.
   */
  selectDropEffect_: function(event, destinationEntry) {
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
  },
};
