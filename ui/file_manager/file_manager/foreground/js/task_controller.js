// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {DialogType} dialogType
 * @param {!VolumeManagerWrapper} volumeManager
 * @param {!FileManagerUI} ui
 * @param {!MetadataModel} metadataModel
 * @param {!DirectoryModel} directoryModel
 * @param {!FileSelectionHandler} selectionHandler
 * @param {!MetadataUpdateController} metadataUpdateController
 * @constructor
 * @struct
 */
function TaskController(
    dialogType, volumeManager, ui, metadataModel, directoryModel,
    selectionHandler, metadataUpdateController) {
  /**
   * @private {DialogType}
   * @const
   */
  this.dialogType_ = dialogType;

  /**
   * @private {!VolumeManagerWrapper}
   * @const
   */
  this.volumeManager_ = volumeManager;

  /**
   * @private {!FileManagerUI}
   * @const
   */
  this.ui_ = ui;

  /**
   * @private {!MetadataModel}
   * @const
   */
  this.metadataModel_ = metadataModel;

  /**
   * @private {!DirectoryModel}
   * @const
   */
  this.directoryModel_ = directoryModel;

  /**
   * @private {!FileSelectionHandler}
   * @const
   */
  this.selectionHandler_ = selectionHandler;

  /**
   * @type {!MetadataUpdateController}
   * @const
   * @private
   */
  this.metadataUpdateController_ = metadataUpdateController;

  /**
   * @private {boolean}
   */
  this.canExecuteDefaultAction_ = false;

  /**
   * @private {boolean}
   */
  this.canExecuteOpenWith_ = false;

  /**
   * @private {!cr.ui.Command}
   * @const
   */
  this.defaultActionCommand_ = assertInstanceof(
      document.querySelector('#default-action'), cr.ui.Command);

  /**
   * @private {!cr.ui.Command}
   * @const
   */
  this.openWithCommand_ =
      assertInstanceof(document.querySelector('#open-with'), cr.ui.Command);

  /**
   * @public {!FileTasks}
   */
  this.tasks = new FileTasks(
      this.volumeManager_, this.metadataModel_, this.directoryModel_, this.ui_);

  ui.taskMenuButton.addEventListener(
      'select', this.onTaskItemClicked_.bind(this));
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
    if (this.tasks)
      this.tasks.executeDefault();
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
  if (!this.tasks)
    return;

  switch (event.item.action) {
    case FileTasks.TaskMenuButtonActions.ShowMenu:
      this.ui_.taskMenuButton.showMenu(false);
      break;
    case FileTasks.TaskMenuButtonActions.RunTask:
      this.tasks.execute(event.item.task.taskId);
      break;
    case FileTasks.TaskMenuButtonActions.ChangeDefaultAction:
      var selection = this.selectionHandler_.selection;
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
      this.tasks.showTaskPicker(
          this.ui_.defaultTaskPicker,
          loadTimeData.getString('CHANGE_DEFAULT_MENU_ITEM'),
          strf('CHANGE_DEFAULT_CAPTION', format),
          this.changeDefaultTask_.bind(this, selection),
          true);
      break;
    default:
      assertNotReached('Unknown action.');
  }
};

/**
 * Sets the given task as default, when this task is applicable.
 *
 * @param {!FileSelection} selection File selection.
 * @param {Object} task Task to set as default.
 * @private
 */
TaskController.prototype.changeDefaultTask_ = function(selection, task) {
  chrome.fileManagerPrivate.setDefaultTask(
      task.taskId,
      selection.entries,
      assert(selection.mimeTypes),
      util.checkAPIError);
  this.metadataUpdateController_.refreshCurrentDirectoryMetadata();

  // Update task menu button unless the task button was updated other selection.
  if (this.selectionHandler_.selection === selection) {
    this.tasks = new FileTasks(
        this.volumeManager_, this.metadataModel_, this.directoryModel_,
        this.ui_);
    this.tasks.init(selection.entries, selection.mimeTypes);
    this.tasks.display(this.ui_.taskMenuButton);
  }
  this.selectionHandler_.onFileSelectionChanged();
};

/**
 * Executes default action.
 */
TaskController.prototype.executeDefaultAction = function() {
  if (this.tasks)
    this.tasks.execute(this.ui_.fileContextMenu.defaultActionMenuItem.taskId);
};

/**
 * Get MIME type for an entry. This method first tries to obtain the MIME type
 * from metadata. If it fails, this falls back to obtain the MIME type from its
 * content or name.
 *
 * @param {!Entry} entry An entry to obtain its mime type.
 * @return {!Promise}
 * @private
 */
TaskController.prototype.getMimeType_ = function(entry) {
  return this.metadataModel_.get([entry], ['contentMimeType']).then(
      function(properties) {
        if (properties[0].contentMimeType)
          return properties[0].contentMimeType;
        return new Promise(function(fulfill, reject) {
          chrome.fileManagerPrivate.getMimeType(
              entry, function(mimeType) {
                if (!chrome.runtime.lastError)
                  fulfill(mimeType);
                else
                  reject(chrome.runtime.lastError);
              });
        });
      });
};

/**
 * Handles change of selection and clears context menu.
 * @private
 */
TaskController.prototype.onSelectionChanged_ = function() {
  var selection = this.selectionHandler_.selection;
  this.tasks = new FileTasks(
      this.volumeManager_, this.metadataModel_, this.directoryModel_,
      this.ui_);
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
    selection.computeAdditional(this.metadataModel_).then(function() {
      if (this.selectionHandler_.selection !== selection)
        return;
      this.tasks.init(selection.entries, assert(selection.mimeTypes)).then(
          function() {
            if (this.selectionHandler_.selection !== selection)
              return;
            this.tasks.display(this.ui_.taskMenuButton);
            this.updateContextMenuActionItems_(
                assert(this.tasks.getTaskItems()));
          }.bind(this));
    }.bind(this));
  } else {
    this.ui_.taskMenuButton.hidden = true;
  }
};

/**
 * Returns whether default action command can be executed or not.
 * @return {boolean} True if default action command is executable.
 */
TaskController.prototype.canExecuteDefaultAction = function() {
  return this.canExecuteDefaultAction_;
};

/**
 * Returns whether open with command can be executed or not.
 * @return {boolean} True if open with command is executable.
 */
TaskController.prototype.canExecuteOpenWith = function() {
  return this.canExecuteOpenWith_;
};

/**
 * Updates action menu item to match passed task items.
 *
 * @param {!Array<!Object>} items List of items.
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

  this.canExecuteDefaultAction_ = items.length === 1;
  this.defaultActionCommand_.canExecuteChange(this.ui_.listContainer.element);

  this.canExecuteOpenWith_ = items.length > 1;
  this.openWithCommand_.canExecuteChange(this.ui_.listContainer.element);

  this.ui_.fileContextMenu.actionItemsSeparator.hidden = items.length === 0;
};

/**
 * @param {FileEntry} entry
 */
TaskController.prototype.doEntryAction = function(entry) {
  if (this.dialogType_ == DialogType.FULL_PAGE) {
    this.metadataModel_.get([entry], ['contentMimeType']).then(
        function(props) {
          var tasks = new FileTasks(
              this.volumeManager_, this.metadataModel_, this.directoryModel_,
              this.ui_);
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
