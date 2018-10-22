// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Demo mode setup screen implementation.
 */

login.createScreen('DemoSetupScreen', 'demo-setup', function() {
  return {
    EXTERNAL_API: ['onSetupFinished'],

    /**
     * Demo setup module.
     * @private
     */
    demoSetupModule_: null,


    /** @override */
    decorate: function() {
      this.demoSetupModule_ = $('demo-setup-content');
    },

    /** Returns a control which should receive an initial focus. */
    get defaultControl() {
      return this.demoSetupModule_;
    },

    /** Called after resources are updated. */
    updateLocalizedContent: function() {
      this.demoSetupModule_.updateLocalizedContent();
    },

    /** @override */
    onBeforeShow: function() {
      this.demoSetupModule_.reset();
    },

    /**
     * Called when demo mode setup finished.
     * @param {boolean} isSuccess Whether demo setup finished successfully.
     * @param {string} message Error message to be displayed to the user,
     *  populated if setup finished with an error.
     */
    onSetupFinished: function(isSuccess, message) {
      this.demoSetupModule_.onSetupFinished(isSuccess, message);
    },
  };
});
