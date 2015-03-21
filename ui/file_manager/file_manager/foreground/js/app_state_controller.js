// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {DialogType} dialogType
 * @constructor
 * @struct
 */
function AppStateController(dialogType) {
  /**
   * @const {string}
   * @private
   */
  this.viewOptionStorageKey_ = 'file-manager-' + dialogType;

  /** @private {DirectoryModel} */
  this.directoryModel_ = null;

  /**
   * @type {FileManagerUI}
   * @private
   */
  this.ui_ = null;

  /**
   * @type {*}
   * @private
   */
  this.viewOptions_ = null;
};

/**
 * @return {Promise}
 */
AppStateController.prototype.loadInitialViewOptions = function() {
  // Load initial view option.
  return new Promise(function(fulfill) {
    chrome.storage.local.get(this.viewOptionStorageKey_, fulfill);
  }.bind(this)).then(function(values) {
    this.viewOptions_ = {};
    var value = values[this.viewOptionStorageKey_];
    if (!value)
      return;

    // Load the global default options.
    try {
      this.viewOptions_ = JSON.parse(value);
    } catch (ignore) {}

    // Override with window-specific options.
    if (window.appState && window.appState.viewOptions) {
      for (var key in window.appState.viewOptions) {
        if (window.appState.viewOptions.hasOwnProperty(key))
          this.viewOptions_[key] = window.appState.viewOptions[key];
      }
    }
  }.bind(this));
};

/**
 * @param {!FileManagerUI} ui
 * @param {!DirectoryModel} directoryModel
 */
AppStateController.prototype.initialize = function(ui, directoryModel) {
  assert(this.viewOptions_);

  this.ui_ = ui;
  this.directoryModel_ = directoryModel;

  // Register event listeners.
  ui.listContainer.table.addEventListener(
      'column-resize-end', this.saveViewOptions.bind(this));
  directoryModel.getFileList().addEventListener(
      'permuted', this.saveViewOptions.bind(this));
  directoryModel.addEventListener(
      'directory-changed', this.onDirectoryChanged_.bind(this));

  // Restore preferences.
  this.ui_.setCurrentListType(
      this.viewOptions_.listType || ListContainer.ListType.DETAIL);
  this.directoryModel_.getFileList().sort(
      this.viewOptions_.sortField || 'modificationTime',
      this.viewOptions_.sortDirection || 'desc');
  if (this.viewOptions_.columnConfig) {
    this.ui_.listContainer.table.columnModel.restoreColumnConfig(
        this.viewOptions_.columnConfig);
  }
};

/**
 * Saves current view option.
 */
AppStateController.prototype.saveViewOptions = function() {
  var sortStatus = this.directoryModel_.getFileList().sortStatus;
  var prefs = {
    sortField: sortStatus.field,
    sortDirection: sortStatus.direction,
    columnConfig: {},
    listType: this.ui_.listContainer.currentListType
  };
  var cm = this.ui_.listContainer.table.columnModel;
  prefs.columnConfig = cm.exportColumnConfig();
  // Save the global default.
  var items = {};
  items[this.viewOptionStorageKey_] = JSON.stringify(prefs);
  chrome.storage.local.set(items);

  // Save the window-specific preference.
  if (window.appState) {
    window.appState.viewOptions = prefs;
    util.saveAppState();
  }
};

/**
 * @private
 */
AppStateController.prototype.onDirectoryChanged_ = function() {
  // TODO(mtomasz): Consider remembering the selection.
  util.updateAppState(
      this.directoryModel_.getCurrentDirEntry() ?
          this.directoryModel_.getCurrentDirEntry().toURL() : '',
      '' /* selectionURL */,
      '' /* opt_param */);
};
