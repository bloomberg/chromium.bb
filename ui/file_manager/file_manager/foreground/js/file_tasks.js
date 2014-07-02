// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * This object encapsulates everything related to tasks execution.
 *
 * TODO(hirono): Pass each component instead of the entire FileManager.
 * @param {FileManager} fileManager FileManager instance.
 * @param {Object=} opt_params File manager load parameters.
 * @constructor
 */
function FileTasks(fileManager, opt_params) {
  this.fileManager_ = fileManager;
  this.params_ = opt_params;
  this.tasks_ = null;
  this.defaultTask_ = null;
  this.entries_ = null;

  /**
   * List of invocations to be called once tasks are available.
   *
   * @private
   * @type {Array.<Object>}
   */
  this.pendingInvocations_ = [];
}

/**
 * Location of the Chrome Web Store.
 *
 * @const
 * @type {string}
 */
FileTasks.CHROME_WEB_STORE_URL = 'https://chrome.google.com/webstore';

/**
 * Base URL of apps list in the Chrome Web Store. This constant is used in
 * FileTasks.createWebStoreLink().
 *
 * @const
 * @type {string}
 */
FileTasks.WEB_STORE_HANDLER_BASE_URL =
    'https://chrome.google.com/webstore/category/collection/file_handlers';


/**
 * The app ID of the video player app.
 * @const
 * @type {string}
 */
FileTasks.VIDEO_PLAYER_ID = 'jcgeabjmjgoblfofpppfkcoakmfobdko';

/**
 * Returns URL of the Chrome Web Store which show apps supporting the given
 * file-extension and mime-type.
 *
 * @param {string} extension Extension of the file (with the first dot).
 * @param {string} mimeType Mime type of the file.
 * @return {string} URL
 */
FileTasks.createWebStoreLink = function(extension, mimeType) {
  if (!extension)
    return FileTasks.CHROME_WEB_STORE_URL;

  if (extension[0] === '.')
    extension = extension.substr(1);
  else
    console.warn('Please pass an extension with a dot to createWebStoreLink.');

  var url = FileTasks.WEB_STORE_HANDLER_BASE_URL;
  url += '?_fe=' + extension.toLowerCase().replace(/[^\w]/g, '');

  // If a mime is given, add it into the URL.
  if (mimeType)
    url += '&_fmt=' + mimeType.replace(/[^-\w\/]/g, '');
  return url;
};

/**
 * Complete the initialization.
 *
 * @param {Array.<Entry>} entries List of file entries.
 */
FileTasks.prototype.init = function(entries, opt_mimeTypes) {
  this.entries_ = entries;
  this.mimeTypes_ = opt_mimeTypes || [];

  // TODO(mtomasz): Move conversion from entry to url to custom bindings.
  var urls = util.entriesToURLs(entries);
  if (urls.length > 0)
    chrome.fileBrowserPrivate.getFileTasks(urls, this.onTasks_.bind(this));
};

/**
 * Returns amount of tasks.
 *
 * @return {number} amount of tasks.
 */
FileTasks.prototype.size = function() {
  return (this.tasks_ && this.tasks_.length) || 0;
};

/**
 * Callback when tasks found.
 *
 * @param {Array.<Object>} tasks The tasks.
 * @private
 */
FileTasks.prototype.onTasks_ = function(tasks) {
  this.processTasks_(tasks);
  for (var index = 0; index < this.pendingInvocations_.length; index++) {
    var name = this.pendingInvocations_[index][0];
    var args = this.pendingInvocations_[index][1];
    this[name].apply(this, args);
  }
  this.pendingInvocations_ = [];
};

/**
 * The list of known extensions to record UMA.
 * Note: Because the data is recorded by the index, so new item shouldn't be
 * inserted.
 *
 * @const
 * @type {Array.<string>}
 * @private
 */
FileTasks.UMA_INDEX_KNOWN_EXTENSIONS_ = Object.freeze([
  'other', '.3ga', '.3gp', '.aac', '.alac', '.asf', '.avi', '.bmp', '.csv',
  '.doc', '.docx', '.flac', '.gif', '.jpeg', '.jpg', '.log', '.m3u', '.m3u8',
  '.m4a', '.m4v', '.mid', '.mkv', '.mov', '.mp3', '.mp4', '.mpg', '.odf',
  '.odp', '.ods', '.odt', '.oga', '.ogg', '.ogv', '.pdf', '.png', '.ppt',
  '.pptx', '.ra', '.ram', '.rar', '.rm', '.rtf', '.wav', '.webm', '.webp',
  '.wma', '.wmv', '.xls', '.xlsx',
]);

/**
 * The list of executable file extensions.
 *
 * @const
 * @type {Array.<string>}
 */
FileTasks.EXECUTABLE_EXTENSIONS = Object.freeze([
  '.exe', '.lnk', '.deb', '.dmg', '.jar', '.msi',
]);

/**
 * The list of extensions to skip the suggest app dialog.
 * @const
 * @type {Array.<string>}
 * @private
 */
FileTasks.EXTENSIONS_TO_SKIP_SUGGEST_APPS_ = Object.freeze([
  '.crdownload', '.dsc', '.inf', '.crx',
]);

/**
 * Records trial of opening file grouped by extensions.
 *
 * @param {Array.<Entry>} entries The entries to be opened.
 * @private
 */
FileTasks.recordViewingFileTypeUMA_ = function(entries) {
  for (var i = 0; i < entries.length; i++) {
    var entry = entries[i];
    var extension = FileType.getExtension(entry).toLowerCase();
    if (FileTasks.UMA_INDEX_KNOWN_EXTENSIONS_.indexOf(extension) < 0) {
      extension = 'other';
    }
    metrics.recordEnum(
        'ViewingFileType', extension, FileTasks.UMA_INDEX_KNOWN_EXTENSIONS_);
  }
};

/**
 * Returns true if the taskId is for an internal task.
 *
 * @param {string} taskId Task identifier.
 * @return {boolean} True if the task ID is for an internal task.
 * @private
 */
FileTasks.isInternalTask_ = function(taskId) {
  var taskParts = taskId.split('|');
  var appId = taskParts[0];
  var taskType = taskParts[1];
  var actionId = taskParts[2];
  // The action IDs here should match ones used in executeInternalTask_().
  return (appId === chrome.runtime.id &&
          taskType === 'file' &&
          (actionId === 'play' ||
           actionId === 'mount-archive' ||
           actionId === 'gallery' ||
           actionId === 'gallery-video'));
};

/**
 * Processes internal tasks.
 *
 * @param {Array.<Object>} tasks The tasks.
 * @private
 */
FileTasks.prototype.processTasks_ = function(tasks) {
  this.tasks_ = [];
  var id = chrome.runtime.id;
  var isOnDrive = false;
  var fm = this.fileManager_;
  for (var index = 0; index < this.entries_.length; ++index) {
    var locationInfo = fm.volumeManager.getLocationInfo(this.entries_[index]);
    if (locationInfo && locationInfo.isDriveBased) {
      isOnDrive = true;
      break;
    }
  }

  for (var i = 0; i < tasks.length; i++) {
    var task = tasks[i];
    var taskParts = task.taskId.split('|');

    // Skip internal Files.app's handlers.
    if (taskParts[0] === id && (taskParts[2] === 'auto-open' ||
        taskParts[2] === 'select' || taskParts[2] === 'open')) {
      continue;
    }

    // Tweak images, titles of internal tasks.
    if (taskParts[0] === id && taskParts[1] === 'file') {
      if (taskParts[2] === 'play') {
        // TODO(serya): This hack needed until task.iconUrl is working
        //             (see GetFileTasksFileBrowserFunction::RunImpl).
        task.iconType = 'audio';
        task.title = loadTimeData.getString('ACTION_LISTEN');
      } else if (taskParts[2] === 'mount-archive') {
        task.iconType = 'archive';
        task.title = loadTimeData.getString('MOUNT_ARCHIVE');
      } else if (taskParts[2] === 'gallery' ||
                 taskParts[2] === 'gallery-video') {
        task.iconType = 'image';
        task.title = loadTimeData.getString('ACTION_OPEN');
      } else if (taskParts[2] === 'open-hosted-generic') {
        if (this.entries_.length > 1)
          task.iconType = 'generic';
        else // Use specific icon.
          task.iconType = FileType.getIcon(this.entries_[0]);
        task.title = loadTimeData.getString('ACTION_OPEN');
      } else if (taskParts[2] === 'open-hosted-gdoc') {
        task.iconType = 'gdoc';
        task.title = loadTimeData.getString('ACTION_OPEN_GDOC');
      } else if (taskParts[2] === 'open-hosted-gsheet') {
        task.iconType = 'gsheet';
        task.title = loadTimeData.getString('ACTION_OPEN_GSHEET');
      } else if (taskParts[2] === 'open-hosted-gslides') {
        task.iconType = 'gslides';
        task.title = loadTimeData.getString('ACTION_OPEN_GSLIDES');
      } else if (taskParts[2] === 'view-swf') {
        // Do not render this task if disabled.
        if (!loadTimeData.getBoolean('SWF_VIEW_ENABLED'))
          continue;
        task.iconType = 'generic';
        task.title = loadTimeData.getString('ACTION_VIEW');
      } else if (taskParts[2] === 'view-pdf') {
        // Do not render this task if disabled.
        if (!loadTimeData.getBoolean('PDF_VIEW_ENABLED'))
          continue;
        task.iconType = 'pdf';
        task.title = loadTimeData.getString('ACTION_VIEW');
      } else if (taskParts[2] === 'view-in-browser') {
        task.iconType = 'generic';
        task.title = loadTimeData.getString('ACTION_VIEW');
      }
    }

    if (!task.iconType && taskParts[1] === 'web-intent') {
      task.iconType = 'generic';
    }

    this.tasks_.push(task);
    if (this.defaultTask_ === null && task.isDefault) {
      this.defaultTask_ = task;
    }
  }
  if (!this.defaultTask_ && this.tasks_.length > 0) {
    // If we haven't picked a default task yet, then just pick the first one.
    // This is not the preferred way we want to pick this, but better this than
    // no default at all if the C++ code didn't set one.
    this.defaultTask_ = this.tasks_[0];
  }
};

/**
 * Executes default task.
 *
 * @param {function(boolean, Array.<string>)=} opt_callback Called when the
 *     default task is executed, or the error is occurred.
 * @private
 */
FileTasks.prototype.executeDefault_ = function(opt_callback) {
  FileTasks.recordViewingFileTypeUMA_(this.entries_);
  this.executeDefaultInternal_(this.entries_, opt_callback);
};

/**
 * Executes default task.
 *
 * @param {Array.<Entry>} entries Entries to execute.
 * @param {function(boolean, Array.<Entry>)=} opt_callback Called when the
 *     default task is executed, or the error is occurred.
 * @private
 */
FileTasks.prototype.executeDefaultInternal_ = function(entries, opt_callback) {
  var callback = opt_callback || function(arg1, arg2) {};

  if (this.defaultTask_ !== null) {
    this.executeInternal_(this.defaultTask_.taskId, entries);
    callback(true, entries);
    return;
  }

  // We don't have tasks, so try to show a file in a browser tab.
  // We only do that for single selection to avoid confusion.
  if (entries.length !== 1 || !entries[0])
    return;

  var filename = entries[0].name;
  var extension = util.splitExtension(filename)[1];
  var mimeType = this.mimeTypes_[0];

  var showAlert = function() {
    var textMessageId;
    var titleMessageId;
    switch (extension) {
      case '.exe':
        textMessageId = 'NO_ACTION_FOR_EXECUTABLE';
        break;
      case '.crx':
        textMessageId = 'NO_ACTION_FOR_CRX';
        titleMessageId = 'NO_ACTION_FOR_CRX_TITLE';
        break;
      default:
        textMessageId = 'NO_ACTION_FOR_FILE';
    }

    var webStoreUrl = FileTasks.createWebStoreLink(extension, mimeType);
    var text = strf(textMessageId, webStoreUrl, str('NO_ACTION_FOR_FILE_URL'));
    var title = titleMessageId ? str(titleMessageId) : filename;
    this.fileManager_.alert.showHtml(title, text, function() {});
    callback(false, entries);
  }.bind(this);

  var onViewFilesFailure = function() {
    var fm = this.fileManager_;
    if (!fm.isOnDrive() ||
        !entries[0] ||
        FileTasks.EXTENSIONS_TO_SKIP_SUGGEST_APPS_.indexOf(extension) !== -1) {
      showAlert();
      return;
    }

    fm.openSuggestAppsDialog(
        entries[0],
        function() {
          var newTasks = new FileTasks(fm);
          newTasks.init(entries, this.mimeTypes_);
          newTasks.executeDefault();
          callback(true, entries);
        }.bind(this),
        // Cancelled callback.
        function() {
          callback(false, entries);
        },
        showAlert);
  }.bind(this);

  var onViewFiles = function(result) {
    switch (result) {
      case 'opened':
        callback(success, entries);
        break;
      case 'message_sent':
        util.isTeleported(window).then(function(teleported) {
          if (teleported) {
            util.showOpenInOtherDesktopAlert(
                this.fileManager_.ui.alertDialog, entries);
          }
        }.bind(this));
        callback(success, entries);
        break;
      case 'empty':
        callback(success, entries);
        break;
      case 'failed':
        onViewFilesFailure();
        break;
    }
  }.bind(this);

  this.checkAvailability_(function() {
    // TODO(mtomasz): Pass entries instead.
    var urls = util.entriesToURLs(entries);
    var taskId = chrome.runtime.id + '|file|view-in-browser';
    chrome.fileBrowserPrivate.executeTask(taskId, urls, onViewFiles);
  }.bind(this));
};

/**
 * Executes a single task.
 *
 * @param {string} taskId Task identifier.
 * @param {Array.<Entry>=} opt_entries Entries to xecute on instead of
 *     this.entries_|.
 * @private
 */
FileTasks.prototype.execute_ = function(taskId, opt_entries) {
  var entries = opt_entries || this.entries_;
  FileTasks.recordViewingFileTypeUMA_(entries);
  this.executeInternal_(taskId, entries);
};

/**
 * The core implementation to execute a single task.
 *
 * @param {string} taskId Task identifier.
 * @param {Array.<Entry>} entries Entries to execute.
 * @private
 */
FileTasks.prototype.executeInternal_ = function(taskId, entries) {
  this.checkAvailability_(function() {
    if (FileTasks.isInternalTask_(taskId)) {
      var taskParts = taskId.split('|');
      this.executeInternalTask_(taskParts[2], entries);
    } else {
      // TODO(mtomasz): Pass entries instead.
      var urls = util.entriesToURLs(entries);
      chrome.fileBrowserPrivate.executeTask(taskId, urls, function(result) {
        if (result !== 'message_sent')
          return;
        util.isTeleported(window).then(function(teleported) {
          if (teleported) {
            util.showOpenInOtherDesktopAlert(
                this.fileManager_.ui.alertDialog, entries);
          }
        }.bind(this));
      }.bind(this));
    }
  }.bind(this));
};

/**
 * Checks whether the remote files are available right now.
 *
 * @param {function} callback The callback.
 * @private
 */
FileTasks.prototype.checkAvailability_ = function(callback) {
  var areAll = function(props, name) {
    var isOne = function(e) {
      // If got no properties, we safely assume that item is unavailable.
      return e && e[name];
    };
    return props.filter(isOne).length === props.length;
  };

  var fm = this.fileManager_;
  var entries = this.entries_;

  var isDriveOffline = fm.volumeManager.getDriveConnectionState().type ===
      VolumeManagerCommon.DriveConnectionType.OFFLINE;

  if (fm.isOnDrive() && isDriveOffline) {
    fm.metadataCache_.get(entries, 'drive', function(props) {
      if (areAll(props, 'availableOffline')) {
        callback();
        return;
      }

      fm.alert.showHtml(
          loadTimeData.getString('OFFLINE_HEADER'),
          props[0].hosted ?
            loadTimeData.getStringF(
                entries.length === 1 ?
                    'HOSTED_OFFLINE_MESSAGE' :
                    'HOSTED_OFFLINE_MESSAGE_PLURAL') :
            loadTimeData.getStringF(
                entries.length === 1 ?
                    'OFFLINE_MESSAGE' :
                    'OFFLINE_MESSAGE_PLURAL',
                loadTimeData.getString('OFFLINE_COLUMN_LABEL')));
    });
    return;
  }

  var isOnMetered = fm.volumeManager.getDriveConnectionState().type ===
      VolumeManagerCommon.DriveConnectionType.METERED;

  if (fm.isOnDrive() && isOnMetered) {
    fm.metadataCache_.get(entries, 'drive', function(driveProps) {
      if (areAll(driveProps, 'availableWhenMetered')) {
        callback();
        return;
      }

      fm.metadataCache_.get(entries, 'filesystem', function(fileProps) {
        var sizeToDownload = 0;
        for (var i = 0; i !== entries.length; i++) {
          if (!driveProps[i].availableWhenMetered)
            sizeToDownload += fileProps[i].size;
        }
        fm.confirm.show(
            loadTimeData.getStringF(
                entries.length === 1 ?
                    'CONFIRM_MOBILE_DATA_USE' :
                    'CONFIRM_MOBILE_DATA_USE_PLURAL',
                util.bytesToString(sizeToDownload)),
            callback);
      });
    });
    return;
  }

  callback();
};

/**
 * Executes an internal task.
 *
 * @param {string} id The short task id.
 * @param {Array.<Entry>} entries The entries to execute on.
 * @private
 */
FileTasks.prototype.executeInternalTask_ = function(id, entries) {
  var fm = this.fileManager_;

  if (id === 'play') {
    var selectedEntry = entries[0];
    if (entries.length === 1) {
      // If just a single audio file is selected pass along every audio file
      // in the directory.
      entries = fm.getAllEntriesInCurrentDirectory().filter(FileType.isAudio);
    }
    // TODO(mtomasz): Pass entries instead.
    var urls = util.entriesToURLs(entries);
    var position = urls.indexOf(selectedEntry.toURL());
    chrome.fileBrowserPrivate.getProfiles(function(profiles,
                                                   currentId,
                                                   displayedId) {
      fm.backgroundPage.launchAudioPlayer({items: urls, position: position},
                                          displayedId);
    });
    return;
  }

  if (id === 'mount-archive') {
    this.mountArchivesInternal_(entries);
    return;
  }

  if (id === 'gallery' || id === 'gallery-video') {
    this.openGalleryInternal_(entries);
    return;
  }

  console.error('Unexpected action ID: ' + id);
};

/**
 * Mounts archives.
 *
 * @param {Array.<Entry>} entries Mount file entries list.
 */
FileTasks.prototype.mountArchives = function(entries) {
  FileTasks.recordViewingFileTypeUMA_(entries);
  this.mountArchivesInternal_(entries);
};

/**
 * The core implementation of mounts archives.
 *
 * @param {Array.<Entry>} entries Mount file entries list.
 * @private
 */
FileTasks.prototype.mountArchivesInternal_ = function(entries) {
  var fm = this.fileManager_;

  var tracker = fm.directoryModel.createDirectoryChangeTracker();
  tracker.start();

  // TODO(mtomasz): Pass Entries instead of URLs.
  var urls = util.entriesToURLs(entries);
  fm.resolveSelectResults_(urls, function(resolvedURLs) {
    for (var index = 0; index < resolvedURLs.length; ++index) {
      // TODO(mtomasz): Pass Entry instead of URL.
      fm.volumeManager.mountArchive(resolvedURLs[index],
        function(volumeInfo) {
          if (tracker.hasChanged) {
            tracker.stop();
            return;
          }
          volumeInfo.resolveDisplayRoot(function(displayRoot) {
            if (tracker.hasChanged) {
              tracker.stop();
              return;
            }
            fm.directoryModel.changeDirectoryEntry(displayRoot);
          }, function() {
            console.warn('Failed to resolve the display root after mounting.');
            tracker.stop();
          });
        }, function(url, error) {
          tracker.stop();
          var path = util.extractFilePath(url);
          var namePos = path.lastIndexOf('/');
          fm.alert.show(strf('ARCHIVE_MOUNT_FAILED',
                             path.substr(namePos + 1), error));
        }.bind(null, resolvedURLs[index]));
      }
  });
};

/**
 * Open the Gallery.
 *
 * @param {Array.<Entry>} entries List of selected entries.
 */
FileTasks.prototype.openGallery = function(entries) {
  FileTasks.recordViewingFileTypeUMA_(entries);
  this.openGalleryInternal_(entries);
};

/**
 * The core implementation to open the Gallery.
 *
 * @param {Array.<Entry>} entries List of selected entries.
 * @private
 */
FileTasks.prototype.openGalleryInternal_ = function(entries) {
  var fm = this.fileManager_;

  var allEntries =
      fm.getAllEntriesInCurrentDirectory().filter(FileType.isImageOrVideo);

  var galleryFrame = fm.document_.createElement('iframe');
  galleryFrame.className = 'overlay-pane';
  galleryFrame.scrolling = 'no';
  galleryFrame.setAttribute('webkitallowfullscreen', true);

  if (this.params_ && this.params_.gallery) {
    // Remove the Gallery state from the location, we do not need it any more.
    // TODO(mtomasz): Consider keeping the selection path.
    util.updateAppState(
        null, /* keep current directory */
        '', /* remove current selection */
        '' /* remove search. */);
  }

  var savedAppState = JSON.parse(JSON.stringify(window.appState));
  var savedTitle = document.title;

  // Push a temporary state which will be replaced every time the selection
  // changes in the Gallery and popped when the Gallery is closed.
  util.updateAppState();

  var onBack = function(selectedEntries) {
    fm.directoryModel.selectEntries(selectedEntries);
    fm.closeFilePopup();  // Will call Gallery.unload.
    window.appState = savedAppState;
    util.saveAppState();
    document.title = savedTitle;
  };

  var onAppRegionChanged = function(visible) {
    fm.onFilePopupAppRegionChanged(visible);
  };

  galleryFrame.onload = function() {
    galleryFrame.contentWindow.ImageUtil.metrics = metrics;

    // TODO(haruki): isOnReadonlyDirectory() only checks the permission for the
    // root. We should check more granular permission to know whether the file
    // is writable or not.
    var readonly = fm.isOnReadonlyDirectory();
    var currentDir = fm.getCurrentDirectoryEntry();
    var downloadsVolume = fm.volumeManager.getCurrentProfileVolumeInfo(
            VolumeManagerCommon.RootType.DOWNLOADS);
    var downloadsDir = downloadsVolume && downloadsVolume.fileSystem.root;

    // TODO(mtomasz): Pass Entry instead of localized name. Conversion to a
    //     display string should be done in gallery.js.
    var readonlyDirName = null;
    if (readonly && currentDir)
      readonlyDirName = util.getEntryLabel(fm.volumeManager, currentDir);

    var context = {
      // We show the root label in readonly warning (e.g. archive name).
      readonlyDirName: readonlyDirName,
      curDirEntry: currentDir,
      saveDirEntry: readonly ? downloadsDir : null,
      searchResults: fm.directoryModel.isSearching(),
      metadataCache: fm.metadataCache_,
      pageState: this.params_,
      appWindow: chrome.app.window.current(),
      onBack: onBack,
      onClose: fm.onClose.bind(fm),
      onMaximize: fm.onMaximize.bind(fm),
      onMinimize: fm.onMinimize.bind(fm),
      onAppRegionChanged: onAppRegionChanged,
      loadTimeData: fm.backgroundPage.background.stringData
    };
    galleryFrame.contentWindow.Gallery.open(
        context, fm.volumeManager, allEntries, entries);
  }.bind(this);

  galleryFrame.src = 'gallery.html';
  fm.openFilePopup(galleryFrame, fm.updateTitle_.bind(fm));
};

/**
 * Displays the list of tasks in a task picker combobutton.
 *
 * @param {cr.ui.ComboButton} combobutton The task picker element.
 * @private
 */
FileTasks.prototype.display_ = function(combobutton) {
  if (this.tasks_.length === 0) {
    combobutton.hidden = true;
    return;
  }

  combobutton.clear();
  combobutton.hidden = false;
  combobutton.defaultItem = this.createCombobuttonItem_(this.defaultTask_);

  var items = this.createItems_();

  if (items.length > 1) {
    var defaultIdx = 0;

    for (var j = 0; j < items.length; j++) {
      combobutton.addDropDownItem(items[j]);
      if (items[j].task.taskId === this.defaultTask_.taskId)
        defaultIdx = j;
    }

    combobutton.addSeparator();
    var changeDefaultMenuItem = combobutton.addDropDownItem({
        label: loadTimeData.getString('CHANGE_DEFAULT_MENU_ITEM')
    });
    changeDefaultMenuItem.classList.add('change-default');
  }
};

/**
 * Creates sorted array of available task descriptions such as title and icon.
 *
 * @return {Array} created array can be used to feed combobox, menus and so on.
 * @private
 */
FileTasks.prototype.createItems_ = function() {
  var items = [];
  var title = this.defaultTask_.title + ' ' +
              loadTimeData.getString('DEFAULT_ACTION_LABEL');
  items.push(this.createCombobuttonItem_(this.defaultTask_, title, true));

  for (var index = 0; index < this.tasks_.length; index++) {
    var task = this.tasks_[index];
    if (task !== this.defaultTask_)
      items.push(this.createCombobuttonItem_(task));
  }

  items.sort(function(a, b) {
    return a.label.localeCompare(b.label);
  });

  return items;
};

/**
 * Updates context menu with default item.
 * @private
 */

FileTasks.prototype.updateMenuItem_ = function() {
  this.fileManager_.updateContextMenuActionItems(this.defaultTask_,
      this.tasks_.length > 1);
};

/**
 * Creates combobutton item based on task.
 *
 * @param {Object} task Task to convert.
 * @param {string=} opt_title Title.
 * @param {boolean=} opt_bold Make a menu item bold.
 * @return {Object} Item appendable to combobutton drop-down list.
 * @private
 */
FileTasks.prototype.createCombobuttonItem_ = function(task, opt_title,
                                                      opt_bold) {
  return {
    label: opt_title || task.title,
    iconUrl: task.iconUrl,
    iconType: task.iconType,
    task: task,
    bold: opt_bold || false
  };
};

/**
 * Shows modal action picker dialog with currently available list of tasks.
 *
 * @param {DefaultActionDialog} actionDialog Action dialog to show and update.
 * @param {string} title Title to use.
 * @param {string} message Message to use.
 * @param {function(Object)} onSuccess Callback to pass selected task.
 */
FileTasks.prototype.showTaskPicker = function(actionDialog, title, message,
                                              onSuccess) {
  var items = this.createItems_();

  var defaultIdx = 0;
  for (var j = 0; j < items.length; j++) {
    if (items[j].task.taskId === this.defaultTask_.taskId)
      defaultIdx = j;
  }

  actionDialog.show(
      title,
      message,
      items, defaultIdx,
      function(item) {
        onSuccess(item.task);
      });
};

/**
 * Decorates a FileTasks method, so it will be actually executed after the tasks
 * are available.
 * This decorator expects an implementation called |method + '_'|.
 *
 * @param {string} method The method name.
 */
FileTasks.decorate = function(method) {
  var privateMethod = method + '_';
  FileTasks.prototype[method] = function() {
    if (this.tasks_) {
      this[privateMethod].apply(this, arguments);
    } else {
      this.pendingInvocations_.push([privateMethod, arguments]);
    }
    return this;
  };
};

FileTasks.decorate('display');
FileTasks.decorate('updateMenuItem');
FileTasks.decorate('execute');
FileTasks.decorate('executeDefault');
