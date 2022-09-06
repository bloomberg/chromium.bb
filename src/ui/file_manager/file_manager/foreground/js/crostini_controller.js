// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';

import {FakeEntryImpl} from '../../common/js/files_app_entry_types.js';
import {str, strf} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {Crostini} from '../../externs/background/crostini.js';
import {FilesToast} from '../elements/files_toast.js';

import {constants} from './constants.js';
import {DirectoryModel} from './directory_model.js';
import {CommandHandler} from './file_manager_commands.js';
import {NavigationModelFakeItem, NavigationModelItemType} from './navigation_list_model.js';
import {DirectoryTree} from './ui/directory_tree.js';

/**
 * CrostiniController handles the foreground UI relating to crostini.
 */
export class CrostiniController {
  /**
   * @param {!Crostini} crostini Crostini background object.
   * @param {!DirectoryModel} directoryModel DirectoryModel.
   * @param {!DirectoryTree} directoryTree DirectoryTree.
   */
  constructor(crostini, directoryModel, directoryTree) {
    /** @private @const */
    this.crostini_ = crostini;

    /** @private @const */
    this.directoryModel_ = directoryModel;

    /** @private @const */
    this.directoryTree_ = directoryTree;

    /** @private */
    this.entrySharedWithCrostini_ = false;

    /** @private */
    this.entrySharedWithPluginVm_ = false;
  }

  /**
   * Refresh the Linux files item at startup and when crostini enabled changes.
   */
  async redraw() {
    // Setup Linux files fake root.
    this.directoryTree_.dataModel.linuxFilesItem =
        this.crostini_.isEnabled(constants.DEFAULT_CROSTINI_VM) ?
        new NavigationModelFakeItem(
            str('LINUX_FILES_ROOT_LABEL'), NavigationModelItemType.CROSTINI,
            new FakeEntryImpl(
                str('LINUX_FILES_ROOT_LABEL'),
                VolumeManagerCommon.RootType.CROSTINI)) :
        null;
    // Redraw the tree to ensure 'Linux files' is added/removed.
    this.directoryTree_.redraw(false);
  }

  /**
   * Load the list of shared paths and show a toast if this is the first time
   * that FilesApp is loaded since login.
   *
   * @param {boolean} maybeShowToast if true, show toast if this is the first
   *     time FilesApp is opened since login.
   * @param {!FilesToast} filesToast
   */
  async loadSharedPaths(maybeShowToast, filesToast) {
    let showToast = maybeShowToast;
    const getSharedPaths = async (vmName) => {
      if (!this.crostini_.isEnabled(vmName)) {
        return 0;
      }

      return new Promise(resolve => {
        chrome.fileManagerPrivate.getCrostiniSharedPaths(
            maybeShowToast, vmName, (entries, firstForSession) => {
              showToast = showToast && firstForSession;
              for (const entry of entries) {
                this.crostini_.registerSharedPath(vmName, assert(entry));
              }
              resolve(entries.length);
            });
      });
    };

    const toast = (count, msgSingle, msgPlural, action, subPage, umaItem) => {
      if (!showToast || count == 0) {
        return;
      }
      filesToast.show(count == 1 ? str(msgSingle) : strf(msgPlural, count), {
        text: str(action),
        callback: () => {
          chrome.fileManagerPrivate.openSettingsSubpage(subPage);
          CommandHandler.recordMenuItemSelected(umaItem);
        }
      });
    };

    const [crostiniShareCount, pluginVmShareCount] = await Promise.all([
      getSharedPaths(constants.DEFAULT_CROSTINI_VM),
      getSharedPaths(constants.PLUGIN_VM)
    ]);

    toast(
        crostiniShareCount, 'FOLDER_SHARED_WITH_CROSTINI',
        'FOLDER_SHARED_WITH_CROSTINI_PLURAL', 'MANAGE_TOAST_BUTTON_LABEL',
        'crostini/sharedPaths',
        CommandHandler.MenuCommandsForUMA.MANAGE_LINUX_SHARING_TOAST_STARTUP);
    // TODO(crbug.com/949356): UX to provide guidance for what to do
    // when we have shared paths with both Linux and Plugin VM.
    toast(
        pluginVmShareCount, 'FOLDER_SHARED_WITH_PLUGIN_VM',
        'FOLDER_SHARED_WITH_PLUGIN_VM_PLURAL', 'MANAGE_TOAST_BUTTON_LABEL',
        'app-management/pluginVm/sharedPaths',
        CommandHandler.MenuCommandsForUMA
            .MANAGE_PLUGIN_VM_SHARING_TOAST_STARTUP);
  }
}
