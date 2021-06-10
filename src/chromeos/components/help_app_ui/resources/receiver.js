// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * A script for the app inside the iframe. Implements a delegate.
 */

/** A pipe through which we can send messages to the parent frame. */
const parentMessagePipe = new MessagePipe('chrome://help-app', window.parent);

/**
 * A delegate which exposes privileged WebUI functionality to the help
 * app.
 * @type {!helpApp.ClientApiDelegate}
 */
const DELEGATE = {
  async openFeedbackDialog() {
    const response =
        await parentMessagePipe.sendMessage(Message.OPEN_FEEDBACK_DIALOG);
    return /** @type {?string} */ (response['errorMessage']);
  },
  async showParentalControls() {
    await parentMessagePipe.sendMessage(Message.SHOW_PARENTAL_CONTROLS);
  },
  /**
   * @override
   * @param {!Array<!helpApp.SearchableItem>} data
   */
  async addOrUpdateSearchIndex(data) {
    await parentMessagePipe.sendMessage(
        Message.ADD_OR_UPDATE_SEARCH_INDEX, data);
  },
  async clearSearchIndex() {
    await parentMessagePipe.sendMessage(Message.CLEAR_SEARCH_INDEX);
  },
  /**
   * @override
   * @param {string} query
   * @return {!Promise<!helpApp.FindResponse>}
   */
  findInSearchIndex(query) {
    return /** @type {!Promise<!helpApp.FindResponse>} */ (
        parentMessagePipe.sendMessage(Message.FIND_IN_SEARCH_INDEX, {query}));
  },
  closeBackgroundPage() {
    parentMessagePipe.sendMessage(Message.CLOSE_BACKGROUND_PAGE);
  },
  /**
   * @override
   * @param {!Array<!helpApp.LauncherSearchableItem>} data
   */
  async updateLauncherSearchIndex(data) {
    await parentMessagePipe.sendMessage(
        Message.UPDATE_LAUNCHER_SEARCH_INDEX, data);
  },
  async maybeShowDiscoverNotification() {
    await parentMessagePipe.sendMessage(
        Message.MAYBE_SHOW_DISCOVER_NOTIFICATION);
  }
};

window.customLaunchData = {
  delegate: DELEGATE,
};
