// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Quick view model that doesn't fit into properties of quick view element.
 */
class QuickViewModel extends cr.EventTarget {
  constructor() {
    super();

    /**
     * Current selected file entry.
     * @type {FileEntry}
     * @private
     */
    this.selectedEntry_ = null;
  }

  /** @return {FileEntry} */
  getSelectedEntry() {
    return this.selectedEntry_;
  }

  /**
   * @param {!FileEntry} entry
   */
  setSelectedEntry(entry) {
    this.selectedEntry_ = entry;
    cr.dispatchSimpleEvent(this, 'selected-entry-changed');
  }
}
