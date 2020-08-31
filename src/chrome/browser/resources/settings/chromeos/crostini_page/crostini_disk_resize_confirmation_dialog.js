// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-crostini-disk-resize-confirmation-dialog' is a
 * component warning the user that resizing a sparse disk cannot be undone.
 * By clicking 'Reserve size', the user agrees to start the operation.
 */
Polymer({
  is: 'settings-crostini-disk-resize-confirmation-dialog',

  /** @override */
  attached() {
    this.$.dialog.showModal();
  },

  /** @private */
  onCancelTap_() {
    this.$.dialog.cancel();
  },

  /** @private */
  onReserveSizeTap_() {
    this.$.dialog.close();
  },
});
