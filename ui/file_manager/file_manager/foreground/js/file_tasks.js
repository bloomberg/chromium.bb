// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This object encapsulates everything related to tasks execution.
 *
 * @param {!VolumeManagerWrapper} volumeManager
 * @param {!MetadataModel} metadataModel
 * @param {!DirectoryModel} directoryModel
 * @param {!FileManagerUI} ui
 * @constructor
 */
function FileTasks(volumeManager, metadataModel, directoryModel, ui) {
  /**
   * @private {!VolumeManagerWrapper}
   */
  this.volumeManager_ = volumeManager;

  /**
   * @private {!MetadataModel}
   */
  this.metadataModel_ = metadataModel;

  /**
   * @private {!DirectoryModel}
   */
  this.directoryModel_ = directoryModel;

  /**
   * @private {!FileManagerUI}
   */
  this.ui_ = ui;

  /**
   * @private {Array<!Object>}
   */
  this.tasks_ = null;

  /**
   * @private {Object}
   */
  this.defaultTask_ = null;

  /**
   * @public {Array<!Entry>}
   */
  this.entries = null;

  /**
   * @public {Array<string>}
   */
  this.mimeTypes = null;

  /**
   * List of invocations to be called once tasks are available.
   *
   * @private {Array<Object>}
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
 * The task id of the zip unpacker app.
 * @const
 * @type {string}
 */
FileTasks.ZIP_UNPACKER_TASK_ID = 'oedeeodfidgoollimchfdnbmhcpnklnd|app|zip';

/**
 * Available actions in task menu button.
 * @enum {string}
 */
FileTasks.TaskMenuButtonActions = {
  ShowMenu: 'ShowMenu',
  RunTask: 'RunTask',
  ChangeDefaultAction: 'ChangeDefaultAction'
};

/**
 * Returns URL of the Chrome Web Store which show apps supporting the given
 * file-extension and mime-type.
 *
 * @param {?string} extension Extension of the file (with the first dot).
 * @param {?string} mimeType Mime type of the file.
 * @return {string} URL
 */
FileTasks.createWebStoreLink = function(extension, mimeType) {
  if (!extension || FileTasks.EXECUTABLE_EXTENSIONS.indexOf(extension) !== -1)
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
 * Opens the suggest file dialog.
 *
 * @param {Entry} entry Entry of the file.
 * @param {?string} mimeType Mime type of the file.
 * @param {function()} onSuccess Success callback.
 * @param {function()} onCancelled User-cancelled callback.
 * @param {function()} onFailure Failure callback.
 */
FileTasks.prototype.openSuggestAppsDialog =
    function(entry, mimeType, onSuccess, onCancelled, onFailure) {
  if (!entry) {
    onFailure();
    return;
  }

  var basename = entry.name;
  var splitted = util.splitExtension(basename);
  var extension = splitted[1];

  // Returns with failure if the file has neither extension nor MIME type.
  if (!extension || !mimeType) {
    onFailure();
    return;
  }

  var onDialogClosed = function(result, itemId) {
    switch (result) {
      case SuggestAppsDialog.Result.SUCCESS:
        onSuccess();
        break;
      case SuggestAppsDialog.Result.FAILED:
        onFailure();
        break;
      default:
        onCancelled();
    }
  };

  this.ui_.suggestAppsDialog.showByExtensionAndMime(
      extension, mimeType, onDialogClosed);
};

/**
 * Complete the initialization.
 *
 * @param {!Array<!Entry>} entries List of file entries.
 * @param {!Array<?string>} mimeTypes Mime-type specified for each entries.
 * @return {!Promise} Promise to be fulfilled when the initialization completes.
 */
FileTasks.prototype.init = function(entries, mimeTypes) {
  this.entries_ = entries;
  this.mimeTypes_ = mimeTypes;

  if (entries.length > 0) {
    return new Promise(function(fulfill) {
      chrome.fileManagerPrivate.getFileTasks(entries, function(taskItems) {
        this.onTasks_(taskItems);
        fulfill();
      }.bind(this));
    }.bind(this));
  } else {
    return Promise.resolve();
  }
};

/**
 * Obtains the task items.
 * @return {Array<!Object>}
 */
FileTasks.prototype.getTaskItems = function() {
  return this.tasks_;
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
 * @param {Array<Object>} tasks The tasks.
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
 * @type {Array<string>}
 * @private
 */
FileTasks.UMA_INDEX_KNOWN_EXTENSIONS_ = Object.freeze([
  'other', '.3ga', '.3gp', '.aac', '.alac', '.asf', '.avi', '.bmp', '.csv',
  '.doc', '.docx', '.flac', '.gif', '.jpeg', '.jpg', '.log', '.m3u', '.m3u8',
  '.m4a', '.m4v', '.mid', '.mkv', '.mov', '.mp3', '.mp4', '.mpg', '.odf',
  '.odp', '.ods', '.odt', '.oga', '.ogg', '.ogv', '.pdf', '.png', '.ppt',
  '.pptx', '.ra', '.ram', '.rar', '.rm', '.rtf', '.wav', '.webm', '.webp',
  '.wma', '.wmv', '.xls', '.xlsx', '.crdownload', '.crx', '.dmg', '.exe',
  '.html', 'htm', '.jar', '.ps', '.torrent', '.txt', '.zip',
]);

/**
 * The list of executable file extensions.
 *
 * @const
 * @type {Array<string>}
 */
FileTasks.EXECUTABLE_EXTENSIONS = Object.freeze([
  '.exe', '.lnk', '.deb', '.dmg', '.jar', '.msi',
]);

/**
 * The list of extensions to skip the suggest app dialog.
 * @const
 * @type {Array<string>}
 * @private
 */
FileTasks.EXTENSIONS_TO_SKIP_SUGGEST_APPS_ = Object.freeze([
  '.crdownload', '.dsc', '.inf', '.crx',
]);

/**
 * Records trial of opening file grouped by extensions.
 *
 * @param {Array<!Entry>} entries The entries to be opened.
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
           actionId === 'mount-archive'));
};

/**
 * Processes internal tasks.
 *
 * @param {Array<Object>} tasks The tasks.
 * @private
 */
FileTasks.prototype.processTasks_ = function(tasks) {
  this.tasks_ = [];
  var id = chrome.runtime.id;
  for (var i = 0; i < tasks.length; i++) {
    var task = tasks[i];
    var taskParts = task.taskId.split('|');

    // Skip internal Files.app's handlers.
    if (taskParts[0] === id &&
        (taskParts[2] === 'select' || taskParts[2] === 'open')) {
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
    // If we haven't picked a default task yet, then just pick the first one
    // which is not generic file handler.
    for (var i = 0; i < this.tasks_.length; i++) {
      var task = this.tasks_[i];
      if (!task.isGenericFileHandler) {
        this.defaultTask_ = task;
        break;
      }
    }
  }
};

/**
 * Executes default task.
 *
 * @param {function(boolean, Array<!Entry>)=} opt_callback Called when the
 *     default task is executed, or the error is occurred.
 * @private
 */
FileTasks.prototype.executeDefault_ = function(opt_callback) {
  assert(this.entries_);  // Set by init().
  FileTasks.recordViewingFileTypeUMA_(this.entries_);
  this.executeDefaultInternal_(opt_callback);
};

/**
 * Executes default task.
 *
 * @param {function(boolean, Array<!Entry>)=} opt_callback Called when the
 *     default task is executed, or the error is occurred.
 * @private
 */
FileTasks.prototype.executeDefaultInternal_ = function(opt_callback) {
  var callback = opt_callback || function(arg1, arg2) {};
  var entries = assert(this.entries_);

  if (this.defaultTask_ !== null) {
    this.executeInternal_(this.defaultTask_.taskId);
    callback(true, entries);
    return;
  }

  // We don't have tasks, so try to show a file in a browser tab.
  // We only do that for single selection to avoid confusion.
  if (entries.length !== 1)
    return;

  var filename = entries[0].name;
  var extension = util.splitExtension(filename)[1] || null;
  var mimeType = this.mimeTypes_[0] || null;

  var showAlert = function() {
    var textMessageId;
    var titleMessageId;
    switch (extension) {
      case '.exe':
      case '.msi':
        textMessageId = 'NO_ACTION_FOR_EXECUTABLE';
        break;
      case '.dmg':
        textMessageId = 'NO_ACTION_FOR_DMG';
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
    this.ui_.alertDialog.showHtml(title, text, null, null, null);
    callback(false, entries);
  }.bind(this);

  var onViewFilesFailure = function() {
    if (FileTasks.EXTENSIONS_TO_SKIP_SUGGEST_APPS_.indexOf(extension) !== -1 ||
        FileTasks.EXECUTABLE_EXTENSIONS.indexOf(extension) !== -1) {
      showAlert();
      return;
    }

    this.openSuggestAppsDialog(
        entries[0],
        mimeType,
        function() {
          var newTasks = new FileTasks(
              this.volumeManager_, this.metadataModel_, this.directoryModel_,
              this.ui_);
          newTasks.init(assert(entries), assert(this.mimeTypes_));
          newTasks.executeDefault();
          callback(true, entries);
        }.bind(this),
        // Cancelled callback.
        function() {
          callback(false, entries);
        }.bind(this),
        showAlert);
  }.bind(this);

  var onViewFiles = function(result) {
    switch (result) {
      case 'opened':
        callback(true, entries);
        break;
      case 'message_sent':
        util.isTeleported(window).then(function(teleported) {
          if (teleported) {
            util.showOpenInOtherDesktopAlert(this.ui_.alertDialog, entries);
          }
        }.bind(this));
        callback(true, entries);
        break;
      case 'empty':
        callback(true, entries);
        break;
      case 'failed':
        onViewFilesFailure();
        break;
    }
  }.bind(this);

  this.checkAvailability_(function() {
    var taskId = chrome.runtime.id + '|file|view-in-browser';
    chrome.fileManagerPrivate.executeTask(taskId, entries, onViewFiles);
  }.bind(this));
};

/**
 * Executes a single task.
 *
 * @param {string} taskId Task identifier.
 * @private
 */
FileTasks.prototype.execute_ = function(taskId) {
  var entries = assert(this.entries_);  // Entries set by init().
  FileTasks.recordViewingFileTypeUMA_(this.entries_);
  this.executeInternal_(taskId);
};

/**
 * The core implementation to execute a single task.
 *
 * @param {string} taskId Task identifier.
 * @private
 */
FileTasks.prototype.executeInternal_ = function(taskId) {
  var entries = assert(this.entries_);
  this.checkAvailability_(function() {
    if (FileTasks.isInternalTask_(taskId)) {
      var taskParts = taskId.split('|');
      this.executeInternalTask_(taskParts[2]);
    } else {
      chrome.fileManagerPrivate.executeTask(taskId,
          assert(entries),
          function(result) {
            if (result !== 'message_sent')
              return;
            util.isTeleported(window).then(function(teleported) {
              if (teleported) {
                util.showOpenInOtherDesktopAlert(
                    this.ui_.alertDialog, entries);
              }
            }.bind(this));
      }.bind(this));
    }
  }.bind(this));
};

/**
 * Checks whether the remote files are available right now.
 *
 * Must not call before initialization.
 * @param {function()} callback The callback.
 * @private
 */
FileTasks.prototype.checkAvailability_ = function(callback) {
  var areAll = function(props, name) {
    var isOne = function(e) {
      // If got no properties, we safely assume that item is available.
      return !e || e[name];
    };
    return props.filter(isOne).length === props.length;
  };

  var entries = assert(this.entries_);

  var isDriveOffline = this.volumeManager_.getDriveConnectionState().type ===
      VolumeManagerCommon.DriveConnectionType.OFFLINE;

  if (isDriveOffline) {
    this.metadataModel_.get(entries, ['availableOffline', 'hosted']).then(
        function(props) {
          if (areAll(props, 'availableOffline')) {
            callback();
            return;
          }

          this.ui_.alertDialog.showHtml(
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
                      loadTimeData.getString('OFFLINE_COLUMN_LABEL')),
              null, null, null);
    }.bind(this));
    return;
  }

  var isOnMetered = this.volumeManager_.getDriveConnectionState().type ===
      VolumeManagerCommon.DriveConnectionType.METERED;

  if (isOnMetered) {
    this.metadataModel_.get(entries, ['availableWhenMetered', 'size']).then(
        function(props) {
          if (areAll(props, 'availableWhenMetered')) {
            callback();
            return;
          }

          var sizeToDownload = 0;
          for (var i = 0; i !== entries.length; i++) {
            if (!props[i].availableWhenMetered)
              sizeToDownload += props[i].size;
          }
          this.ui_.confirmDialog.show(
              loadTimeData.getStringF(
                  entries.length === 1 ?
                      'CONFIRM_MOBILE_DATA_USE' :
                      'CONFIRM_MOBILE_DATA_USE_PLURAL',
                  util.bytesToString(sizeToDownload)),
              callback, null, null);
        }.bind(this));
    return;
  }

  callback();
};

/**
 * Executes an internal task.
 *
 * @param {string} id The short task id.
 * @private
 */
FileTasks.prototype.executeInternalTask_ = function(id) {
  if (id === 'mount-archive') {
    this.mountArchivesInternal_();
    return;
  }

  console.error('Unexpected action ID: ' + id);
};

/**
 * The core implementation of mounts archives.
 * @private
 */
FileTasks.prototype.mountArchivesInternal_ = function() {
  var tracker = this.directoryModel_.createDirectoryChangeTracker();
  tracker.start();

  // TODO(mtomasz): Move conversion from entry to url to custom bindings.
  // crbug.com/345527.
  var urls = util.entriesToURLs(this.entries_);
  for (var index = 0; index < urls.length; ++index) {
    // TODO(mtomasz): Pass Entry instead of URL.
    this.volumeManager_.mountArchive(
        urls[index],
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
            this.directoryModel_.changeDirectoryEntry(displayRoot);
          }, function() {
            console.warn('Failed to resolve the display root after mounting.');
            tracker.stop();
          });
        }, function(url, error) {
          tracker.stop();
          var path = util.extractFilePath(url);
          var namePos = path.lastIndexOf('/');
          this.ui_.alertDialog.show(
              strf('ARCHIVE_MOUNT_FAILED', path.substr(namePos + 1), error),
              null,
              null);
        }.bind(this, urls[index]));
  }
};

/**
 * Displays the list of tasks in a task picker combobutton.
 *
 * @param {cr.ui.ComboButton} combobutton The task picker element.
 * @private
 */
FileTasks.prototype.display_ = function(combobutton) {
  // If there does not exist available task, hide combobutton.
  if (this.tasks_.length === 0) {
    combobutton.hidden = true;
    return;
  }

  combobutton.clear();
  combobutton.hidden = false;

  // If there exist defaultTask show it on the combobutton.
  if (this.defaultTask_) {
    combobutton.defaultItem = this.createCombobuttonItem_(this.defaultTask_,
        str('ACTION_OPEN'));
  } else {
    combobutton.defaultItem = {
      action: FileTasks.TaskMenuButtonActions.ShowMenu,
      label: str('OPEN_WITH_BUTTON_LABEL')
    };
  }

  // If there exist 2 or more available tasks, show them in context menu
  // (including defaultTask). If only one generic task is available, we also
  // show it in the context menu.
  var items = this.createItems_();

  if (items.length > 1 || (items.length === 1 && this.defaultTask_ === null)) {
    for (var j = 0; j < items.length; j++) {
      combobutton.addDropDownItem(items[j]);
    }

    // If there exist non generic task (i.e. defaultTask is set), we show an
    // item to change default action.
    if (this.defaultTask_) {
      combobutton.addSeparator();
      var changeDefaultMenuItem = combobutton.addDropDownItem({
        action: FileTasks.TaskMenuButtonActions.ChangeDefaultAction,
        label: loadTimeData.getString('CHANGE_DEFAULT_MENU_ITEM')
      });
      changeDefaultMenuItem.classList.add('change-default');
    }
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

  // Create items.
  for (var index = 0; index < this.tasks_.length; index++) {
    var task = this.tasks_[index];
    if (task === this.defaultTask_) {
      var title = task.title + ' ' +
                  loadTimeData.getString('DEFAULT_ACTION_LABEL');
      items.push(this.createCombobuttonItem_(task, title, true, true));
    } else {
      items.push(this.createCombobuttonItem_(task));
    }
  }

  // Sort items (Sort order: isDefault, isGenericFileHandler, label).
  items.sort(function(a, b) {
    // Sort by isDefaultTask.
    var isDefault = (b.isDefault ? 1 : 0) - (a.isDefault ? 1 : 0);
    if (isDefault !== 0)
      return isDefault;

    // Sort by isGenericFileHandler.
    var isGenericFileHandler =
        (a.isGenericFileHandler ? 1 : 0) - (b.isGenericFileHandler ? 1 : 0);
    if (isGenericFileHandler !== 0)
      return isGenericFileHandler;

    // Sort by label.
    return a.label.localeCompare(b.label);
  });

  return items;
};

/**
 * Creates combobutton item based on task.
 *
 * @param {Object} task Task to convert.
 * @param {string=} opt_title Title.
 * @param {boolean=} opt_bold Make a menu item bold.
 * @param {boolean=} opt_isDefault Mark the item as default item.
 * @return {Object} Item appendable to combobutton drop-down list.
 * @private
 */
FileTasks.prototype.createCombobuttonItem_ = function(task, opt_title,
                                                      opt_bold,
                                                      opt_isDefault) {
  return {
    action: FileTasks.TaskMenuButtonActions.RunTask,
    label: opt_title || task.title,
    iconUrl: task.iconUrl,
    iconType: task.iconType,
    task: task,
    bold: opt_bold || false,
    isDefault: opt_isDefault || false,
    isGenericFileHandler: task.isGenericFileHandler
  };
};

/**
 * Shows modal action picker dialog with currently available list of tasks.
 *
 * @param {cr.filebrowser.DefaultActionDialog} actionDialog Action dialog to
 *     show and update.
 * @param {string} title Title to use.
 * @param {string} message Message to use.
 * @param {function(Object)} onSuccess Callback to pass selected task.
 * @param {boolean=} opt_hideGenericFileHandler Whether to hide generic file
 *     handler or not.
 */
FileTasks.prototype.showTaskPicker = function(actionDialog, title, message,
                                              onSuccess,
                                              opt_hideGenericFileHandler) {
  var hideGenericFileHandler = opt_hideGenericFileHandler || false;
  var items = this.createItems_();

  if (hideGenericFileHandler)
    items = items.filter(function(item) { return !item.isGenericFileHandler; });

  var defaultIdx = 0;
  for (var j = 0; j < items.length; j++) {
    if (this.defaultTask_ && items[j].task.taskId === this.defaultTask_.taskId)
      defaultIdx = j;
  }

  actionDialog.showDefaultActionDialog(
      title,
      message,
      items, defaultIdx,
      function(item) {
        onSuccess(item.task);
      });
};

/**
 * @param {cr.ui.ComboButton} combobutton The task picker element.
 */
FileTasks.prototype.display = function(combobutton) {
  if (this.tasks_)
    this.display_(combobutton);
  else
    this.pendingInvocations_.push(['display_', [combobutton]]);
};

/**
 * Executes a single task.
 *
 * @param {string} taskId Task identifier.
 */
FileTasks.prototype.execute = function(taskId) {
  if (this.tasks_)
    this.execute_(taskId);
  else
    this.pendingInvocations_.push(['execute_', [taskId]]);
};

/**
 * Executes default task.
 *
 * @param {function(boolean, Array<!Entry>)=} opt_callback Called when the
 *     default task is executed, or the error is occurred.
 */
FileTasks.prototype.executeDefault = function(opt_callback) {
  if (this.tasks_)
    this.executeDefault_(opt_callback);
  else
    this.pendingInvocations_.push(['executeDefault_', [opt_callback]]);
};
