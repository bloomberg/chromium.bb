// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Commands to pass from the ChromeVox background page context
 * to the ChromeVox Panel.
 */

goog.provide('PanelCommand');
goog.provide('PanelCommandType');

/**
 * Create one command to pass to the ChromeVox Panel.
 */
PanelCommand = class {
  /**
   * @param {PanelCommandType} type The type of command.
   * @param {string|{groups:Array}=} opt_data
   *     Optional data associated with the command.
   */
  constructor(type, opt_data) {
    this.type = type;
    this.data = opt_data;
  }

  /**
   * Send this command to the ChromeVox Panel window.
   */
  send() {
    const views = chrome.extension.getViews();
    for (let i = 0; i < views.length; i++) {
      if (views[i].location.href.indexOf('panel/panel.html') > 0) {
        views[i].postMessage(JSON.stringify(this), window.location.origin);
      }
    }
  }
};


/**
 * Possible panel commands.
 * @enum {string}
 */
PanelCommandType = {
  CLEAR_SPEECH: 'clear_speech',
  ADD_NORMAL_SPEECH: 'add_normal_speech',
  ADD_ANNOTATION_SPEECH: 'add_annotation_speech',
  UPDATE_BRAILLE: 'update_braille',
  OPEN_ANNOTATIONS_UI: 'open_annotations_ui',
  OPEN_MENUS: 'open_menus',
  OPEN_MENUS_MOST_RECENT: 'open_menus_most_recent',
  SEARCH: 'search',
  TUTORIAL: 'tutorial',
  UPDATE_NOTES: 'update_notes',
};
