// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {dom, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// #import {invokePolymerMethod} from '../../display_manager.m.js';
// clang-format on

/**
 * @fileoverview
 * 'OobeDialogHostBehavior' is a behavior for oobe-dialog containers to
 * match oobe-dialog bahavior.
 */

/** @polymerBehavior */
/* #export */ var OobeDialogHostBehavior = {
  properties: {},

  /**
   * Triggers onBeforeShow for descendants.
   * @suppress {missingProperties} cr.ui.login.invokePolymerMethod
   */
  propagateOnBeforeShow() {
    const dialogs = this.shadowRoot.querySelectorAll(
        'oobe-dialog,oobe-adaptive-dialog,oobe-content-dialog,' +
        'gaia-dialog,oobe-loading-dialog');
    for (const dialog of dialogs) {
      cr.ui.login.invokePolymerMethod(dialog, 'onBeforeShow');
    }
  },

  /**
   * Trigger onBeforeShow for all children.
   */
  onBeforeShow() {
    this.propagateOnBeforeShow();
  },

  /**
   * Triggers updateLocalizedContent() for elements matched by |selector|.
   * @param {string} selector CSS selector (optional).
   */
  propagateUpdateLocalizedContent(selector) {
    var screens = Polymer.dom(this.root).querySelectorAll(selector);
    for (var i = 0; i < screens.length; ++i) {
      /** @type {{updateLocalizedContent: function()}}}*/ (screens[i])
          .updateLocalizedContent();
    }
  },

};

/**
 * TODO(alemate): Replace with an interface. b/24294625
 * @typedef {{
 *   onBeforeShow: function()
 * }}
 */
OobeDialogHostBehavior.Proto;
