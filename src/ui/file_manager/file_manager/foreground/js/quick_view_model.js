// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dispatchSimpleEvent} from 'chrome://resources/js/cr.m.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/js/cr/event_target.m.js';

/**
 * Quick view model that doesn't fit into properties of quick view element.
 */
export class QuickViewModel extends EventTarget {
  constructor() {
    super();

    /**
     * Current selected file entry.
     * @type {FileEntry}
     * @private
     */
    this.selectedEntry_ = null;
  }

  /**
   * Returns the selected file entry.
   * @return {FileEntry}
   */
  getSelectedEntry() {
    return this.selectedEntry_;
  }

  /**
   * Sets the selected file entry. Emits a synchronous selected-entry-changed
   * event to immediately call MetadataBoxController.updateView_().
   * @param {!FileEntry} entry
   */
  setSelectedEntry(entry) {
    this.selectedEntry_ = entry;
    dispatchSimpleEvent(this, 'selected-entry-changed');
  }
}
