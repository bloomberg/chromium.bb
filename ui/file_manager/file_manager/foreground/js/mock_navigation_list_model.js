// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Mock class for NavigationListModel.
 * Current implementation of mock class cannot handle shortcut list.
 */
class MockNavigationListModel extends cr.EventTarget {
  /**
   * @param {VolumeManager} volumeManager A volume manager.
   */
  constructor(volumeManager) {
    super();
    this.volumeManager_ = volumeManager;
  }

  /**
   * Returns the item at the given index.
   * @param {number} index The index of the entry to get.
   * @return {NavigationModelItem} The item at the given index.
   */
  item(index) {
    const volumeInfo = this.volumeManager_.volumeInfoList.item(index);
    return new NavigationModelVolumeItem(volumeInfo.label, volumeInfo);
  }

  /**
   * Returns the number of items in the model.
   * @return {number} The length of the model.
   * @private
   */
  length_() {
    return this.volumeManager_.volumeInfoList.length;
  }

  get length() {
    return this.length_();
  }
}
