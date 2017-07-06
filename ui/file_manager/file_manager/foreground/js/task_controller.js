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
  this.canExecuteDefaultTask_ = false;

  /**
   * @private {boolean}
   */
  this.canExecuteMoreActions_ = false;

  /**
   * @private {!cr.ui.Command}
   * @const
   */
  this.defaultTaskCommand_ = assertInstanceof(
      document.querySelector('#default-task'), cr.ui.Command);

  /**
   * More actions command that uses #open-with as selector due to the open-with
   * command used previously for the same task.
   * @private {!cr.ui.Command}
   * @const
   */
  this.moreActionsCommand_ =
      assertInstanceof(document.querySelector('#open-with'), cr.ui.Command);

  /**
   * @private {Promise<!FileTasks>}
   */
  this.tasks_ = null;

  ui.taskMenuButton.addEventListener(
      'select', this.onTaskItemClicked_.bind(this));
  this.selectionHandler_.addEventListener(
      FileSelectionHandler.EventType.CHANGE,
      this.onSelectionChanged_.bind(this));
  this.selectionHandler_.addEventListener(
      FileSelectionHandler.EventType.CHANGE_THROTTLED,
      this.updateTasks_.bind(this));
  chrome.fileManagerPrivate.onAppsUpdated.addListener(
      this.updateTasks_.bind(this));
}

/**
 * Cached the temporary disabled task item. Used inside
 * FileSelectionHandler.createTemporaryDisabledTaskItem_().
 * @type {Object}
 * @private
 */
TaskController.cachedDisabledTaskItem_ = null;

/**
 * Create the temporary disabled task item.
 * @return {Object} Created disabled item.
 * @private
 */
TaskController.createTemporaryDisabledTaskItem_ = function() {
  if (!TaskController.cachedDisabledTaskItem_) {
    TaskController.cachedDisabledTaskItem_ = {
      title: str('TASK_OPEN'),
      disabled: true,
      taskId: null
    };
  }

  return TaskController.cachedDisabledTaskItem_;
};

/**
 * Task combobox handler.
 *
 * @param {Object} event Event containing task which was clicked.
 * @private
 */
TaskController.prototype.onTaskItemClicked_ = function(event) {
  this.getFileTasks()
    .then(function(tasks) {
      switch (event.item.type) {
        case FileTasks.TaskMenuButtonItemType.ShowMenu:
          this.ui_.taskMenuButton.showMenu(false);
          break;
        case FileTasks.TaskMenuButtonItemType.RunTask:
          tasks.execute(event.item.task.taskId);
          break;
        case FileTasks.TaskMenuButtonItemType.ChangeDefaultTask:
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
          tasks.showTaskPicker(
              this.ui_.defaultTaskPicker,
              loadTimeData.getString('CHANGE_DEFAULT_MENU_ITEM'),
              strf('CHANGE_DEFAULT_CAPTION', format),
              this.changeDefaultTask_.bind(this, selection),
              true);
          break;
        default:
          assertNotReached('Unknown task.');
      }
    }.bind(this))
    .catch(function(error) {
      if (error)
        console.error(error.stack || error);
    });
};

/**
 * Sets the given task as default, when this task is applicable.
 *
 * @param {!FileSelection} selection File selection.
 * @param {Object} task Task to set as default.
 * @private
 */
TaskController.prototype.changeDefaultTask_ = function(selection, task) {
  var entries = selection.entries;

  Promise.all(entries.map((entry) => this.getMimeType_(entry))).then(function(
      mimeTypes) {
    chrome.fileManagerPrivate.setDefaultTask(
        task.taskId,
        entries,
        mimeTypes,
        util.checkAPIError);
    this.metadataUpdateController_.refreshCurrentDirectoryMetadata();

    // Update task menu button unless the task button was updated other
    // selection.
    if (this.selectionHandler_.selection === selection) {
      this.tasks_ = null;
      this.getFileTasks()
          .then(function(tasks) {
            tasks.display(this.ui_.taskMenuButton);
          }.bind(this))
          .catch(function(error) {
            if (error)
              console.error(error.stack || error);
          });
    }
    this.selectionHandler_.onFileSelectionChanged();
  }.bind(this));
};

/**
 * Executes default task.
 */
TaskController.prototype.executeDefaultTask = function() {
  this.getFileTasks()
      .then(function(tasks) {
        tasks.execute(this.ui_.fileContextMenu.defaultTaskMenuItem.taskId);
      }.bind(this))
      .catch(function(error) {
        if (error)
          console.error(error.stack || error);
      });
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
  this.tasks_ = null;
  var selection = this.selectionHandler_.selection;
  // Caller of update context menu task items.
  // FileSelectionHandler.EventType.CHANGE
  if (this.dialogType_ === DialogType.FULL_PAGE &&
      (selection.directoryCount > 0 || selection.fileCount > 0)) {
    // Show disabled items for position calculation of the menu. They will be
    // overridden in this.updateFileSelectionAsync().
    this.updateContextMenuTaskItems_(
        [TaskController.createTemporaryDisabledTaskItem_()]);
  } else {
    // Update context menu.
    this.updateContextMenuTaskItems_([]);
  }
};

/**
 * Updates available tasks opened from context menu or the open button.
 * @private
 */
TaskController.prototype.updateTasks_ = function() {
  var selection = this.selectionHandler_.selection;
  if (this.dialogType_ === DialogType.FULL_PAGE &&
      (selection.directoryCount > 0 || selection.fileCount > 0)) {
    this.getFileTasks()
        .then(function(tasks) {
          tasks.display(this.ui_.taskMenuButton);
          this.updateContextMenuTaskItems_(tasks.getTaskItems());
        }.bind(this))
        .catch(function(error) {
          if (error)
            console.error(error.stack || error);
        });
  } else {
    this.ui_.taskMenuButton.hidden = true;
  }
}

/**
 * @return {!Promise<!FileTasks>}
 * @public
 */
TaskController.prototype.getFileTasks = function() {
  if (this.tasks_)
    return this.tasks_;

  var selection = this.selectionHandler_.selection;
  return selection.computeAdditional(this.metadataModel_).then(
      function() {
        if (this.selectionHandler_.selection !== selection)
          return Promise.reject();
        return FileTasks.create(
            this.volumeManager_, this.metadataModel_, this.directoryModel_,
            this.ui_, selection.entries, assert(selection.mimeTypes)).
            then(function(tasks) {
              if (this.selectionHandler_.selection !== selection)
                return Promise.reject();
              return tasks;
            }.bind(this));
      }.bind(this));
};

/**
 * Returns whether default task command can be executed or not.
 * @return {boolean} True if default task command is executable.
 */
TaskController.prototype.canExecuteDefaultTask = function() {
  return this.canExecuteDefaultTask_;
};

/**
 * Returns whether open with command can be executed or not.
 * @return {boolean} True if open with command is executable.
 */
TaskController.prototype.canExecuteMoreActions = function() {
  return this.canExecuteMoreActions_;
};

/**
 * Updates tasks menu item to match passed task items.
 *
 * @param {!Array<!Object>} items List of items.
 * @private
 */
TaskController.prototype.updateContextMenuTaskItems_ = function(items) {
  // Always show a default item in case at least one task is available, even
  // if there is no corresponding default task (i.e. the available task is
  // a generic handler).
  if (items.length >= 1) {
    var defaultTask = FileTasks.getDefaultTask(
        items, items[0] /* task to use in case of no default */);

    if (defaultTask.iconType) {
      this.ui_.fileContextMenu.defaultTaskMenuItem.style.backgroundImage = '';
      this.ui_.fileContextMenu.defaultTaskMenuItem.setAttribute(
          'file-type-icon', defaultTask.iconType);
    } else if (defaultTask.iconUrl) {
      this.ui_.fileContextMenu.defaultTaskMenuItem.style.backgroundImage =
          'url(' + defaultTask.iconUrl + ')';
    } else {
      this.ui_.fileContextMenu.defaultTaskMenuItem.style.backgroundImage = '';
    }

    this.ui_.fileContextMenu.defaultTaskMenuItem.label =
        defaultTask.taskId === FileTasks.ZIP_UNPACKER_TASK_ID ?
        str('TASK_OPEN') : defaultTask.title;
    this.ui_.fileContextMenu.defaultTaskMenuItem.disabled =
        !!defaultTask.disabled;
    this.ui_.fileContextMenu.defaultTaskMenuItem.taskId = defaultTask.taskId;
  }

  this.canExecuteDefaultTask_ = items.length >= 1;
  this.defaultTaskCommand_.canExecuteChange(this.ui_.listContainer.element);

  this.canExecuteMoreActions_ = items.length > 1;
  this.moreActionsCommand_.canExecuteChange(this.ui_.listContainer.element);

  this.ui_.fileContextMenu.tasksSeparator.hidden = items.length === 0;
};

/**
 * @param {FileEntry} entry
 */
TaskController.prototype.executeEntryTask = function(entry) {
  this.metadataModel_.get([entry], ['contentMimeType']).then(
      function(props) {
        FileTasks.create(
            this.volumeManager_, this.metadataModel_, this.directoryModel_,
            this.ui_, [entry], [props[0].contentMimeType || null])
            .then(function(tasks) {
              tasks.executeDefault();
            });
      }.bind(this));
};
