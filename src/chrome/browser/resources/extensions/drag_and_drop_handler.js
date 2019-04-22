// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('extensions', function() {
  'use strict';

  /** @implements cr.ui.DragWrapperDelegate */
  class DragAndDropHandler {
    /**
     * @param {boolean} dragEnabled
     * @param {!EventTarget} target
     */
    constructor(dragEnabled, target) {
      this.dragEnabled = dragEnabled;

      /** @private {!EventTarget} */
      this.eventTarget_ = target;
    }

    /** @override */
    shouldAcceptDrag(e) {
      // External Extension installation can be disabled globally, e.g. while a
      // different overlay is already showing.
      if (!this.dragEnabled) {
        return false;
      }

      // We can't access filenames during the 'dragenter' event, so we have to
      // wait until 'drop' to decide whether to do something with the file or
      // not.
      // See: http://www.w3.org/TR/2011/WD-html5-20110113/dnd.html#concept-dnd-p
      return !!e.dataTransfer.types &&
          e.dataTransfer.types.indexOf('Files') > -1;
    }

    /** @override */
    doDragEnter() {
      chrome.developerPrivate.notifyDragInstallInProgress();
      this.eventTarget_.dispatchEvent(
          new CustomEvent('extension-drag-started'));
    }

    /** @override */
    doDragLeave() {
      this.fireDragEnded_();
    }

    /** @override */
    doDragOver(e) {
      e.preventDefault();
    }

    /** @override */
    doDrop(e) {
      this.fireDragEnded_();
      if (e.dataTransfer.files.length != 1) {
        return;
      }

      let handled = false;

      // Files lack a check if they're a directory, but we can find out through
      // its item entry.
      const item = e.dataTransfer.items[0];
      if (item.kind === 'file' && item.webkitGetAsEntry().isDirectory) {
        handled = true;
        this.handleDirectoryDrop_();
      } else if (/\.(crx|user\.js|zip)$/i.test(e.dataTransfer.files[0].name)) {
        // Only process files that look like extensions. Other files should
        // navigate the browser normally.
        handled = true;
        this.handleFileDrop_();
      }

      if (handled) {
        e.preventDefault();
      }
    }

    /**
     * Handles a dropped file.
     * @private
     */
    handleFileDrop_() {
      chrome.developerPrivate.installDroppedFile();
    }

    /**
     * Handles a dropped directory.
     * @private
     */
    handleDirectoryDrop_() {
      // TODO(devlin): Update this to use extensions.Service when it's not
      // shared between the MD and non-MD pages.
      chrome.developerPrivate.loadUnpacked(
          {failQuietly: true, populateError: true, useDraggedPath: true},
          (loadError) => {
            if (loadError) {
              this.eventTarget_.dispatchEvent(new CustomEvent(
                  'drag-and-drop-load-error', {detail: loadError}));
            }
          });
    }

    /** @private */
    fireDragEnded_() {
      this.eventTarget_.dispatchEvent(new CustomEvent('extension-drag-ended'));
    }
  }

  return {
    DragAndDropHandler: DragAndDropHandler,
  };
});
