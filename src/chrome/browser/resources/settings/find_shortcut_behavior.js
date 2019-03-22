// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Listens for a find keyboard shortcut (i.e. Ctrl/Cmd+f)
 * and keeps track of an stack of potential listeners. Only the listener at the
 * top of the stack will be notified that a find shortcut has been invoked.
 */

cr.define('settings', function() {
  /**
   * Stack of listeners. Only the top listener will handle the shortcut.
   * @type {!Array<!HTMLElement>}
   */
  const listeners = [];

  /**
   * Tracks if any modal context is open in settings. This assumes only one
   * modal can be open at a time. The modals that are being tracked include
   * cr-dialog and cr-drawer.
   * @type {boolean}
   */
  let modalContextOpen = false;

  const shortcut =
      new cr.ui.KeyboardShortcutList(cr.isMac ? 'meta|f' : 'ctrl|f');

  window.addEventListener('keydown', e => {
    if (e.defaultPrevented || listeners.length == 0)
      return;

    if (shortcut.matchesEvent(e)) {
      const listener = /** @type {!{handleFindShortcut: function(boolean)}} */ (
          listeners[listeners.length - 1]);
      if (listener.handleFindShortcut(modalContextOpen))
        e.preventDefault();
    }
  });

  window.addEventListener('cr-dialog-open', () => {
    modalContextOpen = true;
  });

  window.addEventListener('cr-drawer-opened', () => {
    modalContextOpen = true;
  });

  window.addEventListener('close', e => {
    if (['CR-DIALOG', 'CR-DRAWER'].includes(e.composedPath()[0].nodeName))
      modalContextOpen = false;
  });

  /**
   * Used to determine how to handle find shortcut invocations.
   * @polymerBehavior
   */
  const FindShortcutBehavior = {
    /**
     * If handled, return true.
     * @param {boolean} modalContextOpen
     * @return {boolean}
     * @protected
     */
    handleFindShortcut(modalContextOpen) {
      assertNotReached();
    },

    becomeActiveFindShortcutListener() {
      assert(listeners.length == 0 || listeners[listeners.length - 1] != this);
      listeners.push(this);
    },

    removeSelfAsFindShortcutListener() {
      assert(listeners.pop() == this);
    },
  };

  return {
    FindShortcutBehavior,
  };
});
