// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {DialogType} dialogType
 * @param {!FileManagerUI} ui
 * @param {!MetadataCache} metadataCache
 * @param {!FileSelectionHandler} selectionHandler
 * @param {!MetadataUpdateController} metadataUpdateController
 * @param {function():!FileTasks} createTask
 * @constructor
 * @struct
 */
function TaskController(
    dialogType, ui, metadataCache, selectionHandler, metadataUpdateController,
    createTask) {
  /**
   * @type {DialogType}
   * @const
   * @private
   */
  this.dialogType_ = dialogType;

  /**
   * @type {!FileManagerUI}
   * @const
   * @private
   */
  this.ui_ = ui;

  /**
   * @type {!MetadataCache}
   * @const
   * @private
   */
  this.metadataCache_ = metadataCache;

  /**
   * @type {!FileSelectionHandler}
   * @const
   * @private
   */
  this.selectionHandler_ = selectionHandler;

  /**
   * @type {!MetadataUpdateController}
   * @const
   * @private
   */
  this.metadataUpdateController_ = metadataUpdateController;

  /**
   * @type {function():!FileTasks}
   * @const
   * @private
   * TODO(hirono): Remove this after removing dependency for FileManager from
   * FileTasks.
   */
  this.createTask_ = createTask;

  /**
   * @type {!cr.ui.Command}
   * @const
   * @private
   */
  this.openWithCommand_ =
      assertInstanceof(document.querySelector('#open-with'), cr.ui.Command);

  ui.taskMenuButton.addEventListener(
      'select', this.onTaskItemClicked_.bind(this));
  ui.fileContextMenu.defaultActionMenuItem.addEventListener(
      'activate', this.onActionMenuItemActivated_.bind(this));
  this.selectionHandler_.addEventListener(
      FileSelectionHandler.EventType.CHANGE,
      this.onSelectionChanged_.bind(this));
  this.selectionHandler_.addEventListener(
      FileSelectionHandler.EventType.CHANGE_THROTTLED,
      this.onSelectionChangeThrottled_.bind(this));
}

/**
 * Cached the temporary disabled action item. Used inside
 * FileSelectionHandler.createTemporaryDisabledActionItem_().
 * @type {Object}
 * @private
 */
TaskController.cachedDisabledActionItem_ = null;

/**
 * Create the temporary disabled action item.
 * @return {Object} Created disabled item.
 * @private
 */
TaskController.createTemporaryDisabledActionItem_ = function() {
  if (!TaskController.cachedDisabledActionItem_) {
    TaskController.cachedDisabledActionItem_ = {
      title: str('ACTION_OPEN'),
      disabled: true,
      taskId: null
    };
  }

  return TaskController.cachedDisabledActionItem_;
};

/**
 * Do action depending on the selection and the dialog type.
 */
TaskController.prototype.dispatchSelectionAction = function() {
  if (this.dialogType_ == DialogType.FULL_PAGE) {
    var selection = this.selectionHandler_.selection;
    var tasks = selection.tasks;
    var mimeTypes = selection.mimeTypes;
    if (tasks)
      tasks.executeDefault();
    return true;
  }
  if (!this.ui_.dialogFooter.okButton.disabled) {
    this.ui_.dialogFooter.okButton.click();
    return true;
  }
  return false;
};

/**
 * Task combobox handler.
 *
 * @param {Object} event Event containing task which was clicked.
 * @private
 */
TaskController.prototype.onTaskItemClicked_ = function(event) {
  var selection = this.selectionHandler_.selection;
  if (!selection.tasks)
    return;

  if (event.item.task) {
    // Task field doesn't exist on change-default dropdown item.
    selection.tasks.execute(event.item.task.taskId);
  } else {
    var extensions = [];

    for (var i = 0; i < selection.entries.length; i++) {
      var match = /\.(\w+)$/g.exec(selection.entries[i].toURL());
      if (match) {
        var ext = match[1].toUpperCase();
        if (extensions.indexOf(ext) == -1) {
          extensions.push(ext);
        }
      }
    }

    var format = '';

    if (extensions.length == 1) {
      format = extensions[0];
    }

    // Change default was clicked. We should open "change default" dialog.
    selection.tasks.showTaskPicker(
        this.ui_.defaultTaskPicker,
        loadTimeData.getString('CHANGE_DEFAULT_MENU_ITEM'),
        strf('CHANGE_DEFAULT_CAPTION', format),
        this.changeDefaultTask_.bind(this, selection),
        true);
  }
};

/**
 * Sets the given task as default, when this task is applicable.
 *
 * @param {Object} task Task to set as default.
 * @private
 */
TaskController.prototype.changeDefaultTask_ = function(selection, task) {
  // TODO(mtomasz): Move conversion from entry to url to custom bindings.
  // crbug.com/345527.
  chrome.fileManagerPrivate.setDefaultTask(
      task.taskId,
      util.entriesToURLs(selection.entries),
      selection.mimeTypes);
  this.metadataUpdateController_.refreshCurrentDirectoryMetadata();

  // Update task menu button unless the task button was updated other selection.
  if (this.selectionHandler_.selection === selection) {
    selection.tasks = this.createTask_();
    selection.tasks.init(selection.entries, selection.mimeTypes);
    selection.tasks.display(this.ui_.taskMenuButton);
  }
  this.selectionHandler_.onFileSelectionChanged();
};

/**
 * Handles activate event of action menu item.
 *
 * @private
 */
TaskController.prototype.onActionMenuItemActivated_ = function() {
  var tasks = this.selectionHandler_.selection.tasks;
  if (tasks)
    tasks.execute(this.ui_.fileContextMenu.defaultActionMenuItem.taskId);
};

/**
 * Opens the suggest file dialog.
 *
 * @param {Entry} entry Entry of the file.
 * @param {function()} onSuccess Success callback.
 * @param {function()} onCancelled User-cancelled callback.
 * @param {function()} onFailure Failure callback.
 * @private
 */
TaskController.prototype.openSuggestAppsDialog =
    function(entry, onSuccess, onCancelled, onFailure) {
  if (!url) {
    onFailure();
    return;
  }

  this.metadataCache_.getOne(entry, 'external', function(prop) {
    if (!prop || !prop.contentMimeType) {
      onFailure();
      return;
    }

    var basename = entry.name;
    var splitted = util.splitExtension(basename);
    var filename = splitted[0];
    var extension = splitted[1];
    var mime = prop.contentMimeType;

    // Returns with failure if the file has neither extension nor mime.
    if (!extension || !mime) {
      onFailure();
      return;
    }

    var onDialogClosed = function(result) {
      switch (result) {
        case SuggestAppsDialog.Result.INSTALL_SUCCESSFUL:
          onSuccess();
          break;
        case SuggestAppsDialog.Result.FAILED:
          onFailure();
          break;
        default:
          onCancelled();
      }
    };

    if (FileTasks.EXECUTABLE_EXTENSIONS.indexOf(extension) !== -1) {
      this.ui_.suggestAppsDialog.showByFilename(filename, onDialogClosed);
    } else {
      this.ui_.suggestAppsDialog.showByExtensionAndMime(
          extension, mime, onDialogClosed);
    }
  }.bind(this));
};

/**
 * Handles change of selection and clears context menu.
 * @private
 */
TaskController.prototype.onSelectionChanged_ = function() {
  var selection = this.selectionHandler_.selection;
  // Caller of update context menu action items.
  // FileSelectionHandler.EventType.CHANGE
  if (this.dialogType_ === DialogType.FULL_PAGE &&
      selection.directoryCount === 0 && selection.fileCount > 0) {
    // Show disabled items for position calculation of the menu. They will be
    // overridden in this.updateFileSelectionAsync().
    this.updateContextMenuActionItems_(
        [TaskController.createTemporaryDisabledActionItem_()]);
  } else {
    // Update context menu.
    this.updateContextMenuActionItems_([]);
  }
};

/**
 * Handles change of selection asynchronously and updates context menu.
 * @private
 */
TaskController.prototype.onSelectionChangeThrottled_ = function() {
  // FileSelectionHandler.EventType.CHANGE_THROTTLED
  // Update the file tasks.
  var selection = this.selectionHandler_.selection;
  if (this.dialogType_ === DialogType.FULL_PAGE &&
      selection.directoryCount === 0 && selection.fileCount > 0) {
    selection.completeInit().then(function() {
      if (this.selectionHandler_.selection !== selection)
        return;
      selection.tasks.display(this.ui_.taskMenuButton);
      this.updateContextMenuActionItems_(
          assert(selection.tasks.getTaskItems()));
    }.bind(this));
  } else {
    this.ui_.taskMenuButton.hidden = true;
  }
};

/**
 * Updates action menu item to match passed task items.
 *
 * @param {!Array.<!Object>} items List of items.
 * @private
 */
TaskController.prototype.updateContextMenuActionItems_ = function(items) {
  // When only one task is available, show it as default item.
  if (items.length === 1) {
    var actionItem = items[0];

    if (actionItem.iconType) {
      this.ui_.fileContextMenu.defaultActionMenuItem.style.backgroundImage = '';
      this.ui_.fileContextMenu.defaultActionMenuItem.setAttribute(
          'file-type-icon', actionItem.iconType);
    } else if (actionItem.iconUrl) {
      this.ui_.fileContextMenu.defaultActionMenuItem.style.backgroundImage =
          'url(' + actionItem.iconUrl + ')';
    } else {
      this.ui_.fileContextMenu.defaultActionMenuItem.style.backgroundImage = '';
    }

    this.ui_.fileContextMenu.defaultActionMenuItem.label =
        actionItem.taskId === FileTasks.ZIP_UNPACKER_TASK_ID ?
        str('ACTION_OPEN') : actionItem.title;
    this.ui_.fileContextMenu.defaultActionMenuItem.disabled =
        !!actionItem.disabled;
    this.ui_.fileContextMenu.defaultActionMenuItem.taskId = actionItem.taskId;
  }

  this.ui_.fileContextMenu.defaultActionMenuItem.hidden = items.length !== 1;

  // When multiple tasks are available, show them in open with.
  this.openWithCommand_.canExecuteChange();
  this.openWithCommand_.setHidden(items.length < 2);
  this.openWithCommand_.disabled = items.length < 2;

  // Hide default action separator when there does not exist available task.
  this.ui_.fileContextMenu.defaultActionSeparator.hidden = items.length === 0;
};

/**
 * @param {FileEntry} entry
 */
TaskController.prototype.doEntryAction = function(entry) {
  if (this.dialogType_ == DialogType.FULL_PAGE) {
    this.metadataCache_.get([entry], 'external', function(props) {
      var tasks = this.createTask_();
      tasks.init([entry], [props[0].contentMimeType || '']);
      tasks.executeDefault();
    }.bind(this));
  } else {
    var selection = this.selectionHandler_.selection;
    if (selection.entries.length === 1 &&
        util.isSameEntry(selection.entries[0], entry)) {
      this.ui_.dialogFooter.okButton.click();
    }
  }
};
