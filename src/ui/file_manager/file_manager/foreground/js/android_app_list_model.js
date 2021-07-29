// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';

/**
 * Model for managing a list of Android apps.
 */
export class AndroidAppListModel extends EventTarget {
  /**
   * @param {boolean} showAndroidPickerApps Whether to show picker apps in file
   *     selector.
   * @param {boolean} includeAllFiles Corresponds to LaunchParam.includeAllFiles
   * @param {!Array<!Object>} typeList Corresponds to LaunchParam.typeList
   */
  constructor(showAndroidPickerApps, includeAllFiles, typeList) {
    super();

    /** @private {!Array<!chrome.fileManagerPrivate.AndroidApp>} */
    this.apps_ = [];

    if (!showAndroidPickerApps) {
      return;
    }

    let extensions = [];
    if (!includeAllFiles) {
      for (let i = 0; i < typeList.length; i++) {
        extensions = extensions.concat(typeList[i].extensions);
      }
    }

    chrome.fileManagerPrivate.getAndroidPickerApps(extensions, apps => {
      this.apps_ = apps;
      this.dispatchEvent(new Event('permuted'));
    });
  }

  /**
   * @return {number} Number of picker apps.
   */
  length() {
    return this.apps_.length;
  }

  /**
   * @param {number} index Index of the picker app to be retrieved.
   * @return {chrome.fileManagerPrivate.AndroidApp} The value of the |index|-th
   *     picker app.
   */
  item(index) {
    return this.apps_[index];
  }
}
