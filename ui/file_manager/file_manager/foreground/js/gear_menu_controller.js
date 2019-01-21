// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {!cr.ui.MultiMenuButton} gearButton
 * @param {!FilesToggleRipple} toggleRipple
 * @param {!GearMenu} gearMenu
 * @param {!ProvidersMenu} providersMenu
 * @param {!DirectoryModel} directoryModel
 * @param {!CommandHandler} commandHandler
 * @param {!ProvidersModel} providersModel
 * @constructor
 * @struct
 */
function GearMenuController(
    gearButton, toggleRipple, gearMenu, providersMenu, directoryModel,
    commandHandler, providersModel) {
  /**
   * @type {!FilesToggleRipple}
   * @const
   * @private
   */
  this.toggleRipple_ = toggleRipple;

  /**
   * @type {!GearMenu}
   * @const
   * @private
   */
  this.gearMenu_ = gearMenu;

  /**
   * @type {!ProvidersMenu}
   * @const
   * @private
   */
  this.providersMenu_ = providersMenu;

  /**
   * @type {!DirectoryModel}
   * @const
   * @private
   */
  this.directoryModel_ = directoryModel;

  /**
   * @type {!CommandHandler}
   * @const
   * @private
   */
  this.commandHandler_ = commandHandler;

  /**
   * @type {!ProvidersModel}
   * @const
   * @private
   */
  this.providersModel_ = providersModel;

  gearButton.addEventListener('menushow', this.onShowGearMenu_.bind(this));
  gearButton.addEventListener('menuhide', this.onHideGearMenu_.bind(this));
  directoryModel.addEventListener(
      'directory-changed', this.onDirectoryChanged_.bind(this));
  chrome.fileManagerPrivate.onPreferencesChanged.addListener(
      this.onPreferencesChanged_.bind(this));
  this.onPreferencesChanged_();
}

/**
 * @private
 */
GearMenuController.prototype.onShowGearMenu_ = function() {
  this.toggleRipple_.activated = true;
  this.refreshRemainingSpace_(false);  /* Without loading caption. */

  // Update view of drive-related settings.
  this.commandHandler_.updateAvailability();

  this.updateNewServiceItem();
};

/**
 * Update "New service" menu item to either directly show the Webstore dialog
 * when there isn't any service/FSP extension installed, or display the
 * providers menu with the currently installed extensions and also install new
 * service.
 *
 * @private
 */
GearMenuController.prototype.updateNewServiceItem = function() {
  this.providersModel_.getMountableProviders().then(providers => {
    // Go straight to webstore to install the first provider.
    let desiredMenu = '#install-new-extension';
    let label = str('INSTALL_NEW_EXTENSION_LABEL');

    const shouldDisplayProvidersMenu = providers.length > 0;
    if (shouldDisplayProvidersMenu) {
      // Open the providers menu with an installed provider and an install new
      // provider option.
      desiredMenu = '#new-service';
      label = str('ADD_NEW_SERVICES_BUTTON_LABEL');
      // Trigger an update of the providers submenu.
      this.providersMenu_.updateSubMenu();
    }

    this.gearMenu_.setNewServiceCommand(desiredMenu, label);
  });
};

/**
 * @private
 */
GearMenuController.prototype.onHideGearMenu_ = function() {
  this.toggleRipple_.activated = false;
};

/**
 * @param {Event} event
 * @private
 */
GearMenuController.prototype.onDirectoryChanged_ = function(event) {
  event = /** @type {DirectoryChangeEvent} */ (event);
  if (event.volumeChanged) {
    this.refreshRemainingSpace_(true);
  }  // Show loading caption.
};

/**
 * Refreshes space info of the current volume.
 * @param {boolean} showLoadingCaption Whether show loading caption or not.
 * @private
 */
GearMenuController.prototype.refreshRemainingSpace_ = function(
    showLoadingCaption) {
  var currentDirectory = this.directoryModel_.getCurrentDirEntry();
  if (!currentDirectory || util.isRecentRoot(currentDirectory)) {
    this.gearMenu_.setSpaceInfo(null, false);
    return;
  }

  var currentVolumeInfo = this.directoryModel_.getCurrentVolumeInfo();
  if (!currentVolumeInfo) {
    return;
  }

  // TODO(mtomasz): Add support for remaining space indication for provided
  // file systems.
  if (currentVolumeInfo.volumeType == VolumeManagerCommon.VolumeType.PROVIDED ||
      currentVolumeInfo.volumeType ==
          VolumeManagerCommon.VolumeType.MEDIA_VIEW ||
      currentVolumeInfo.volumeType == VolumeManagerCommon.VolumeType.ARCHIVE) {
    this.gearMenu_.setSpaceInfo(null, false);
    return;
  }

  this.gearMenu_.setSpaceInfo(new Promise(function(fulfill) {
    chrome.fileManagerPrivate.getSizeStats(currentVolumeInfo.volumeId, fulfill);
  }), true);
};

/**
 * Handles preferences change and updates menu.
 * @private
 */
GearMenuController.prototype.onPreferencesChanged_ = function() {
  chrome.fileManagerPrivate.getPreferences(function(prefs) {
    if (chrome.runtime.lastError) {
      return;
    }

    if (prefs.cellularDisabled) {
      this.gearMenu_.syncButton.setAttribute('checked', '');
    } else {
      this.gearMenu_.syncButton.removeAttribute('checked');
    }
  }.bind(this));
};
