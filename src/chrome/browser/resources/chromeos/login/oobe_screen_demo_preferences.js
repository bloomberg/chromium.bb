// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Demo mode preferences screen implementation.
 */

login.createScreen('DemoPreferencesScreen', 'demo-preferences', function() {
  var CONTEXT_KEY_LOCALE = 'locale';
  var CONTEXT_KEY_INPUT_METHOD = 'input-method';

  var demoPreferencesModule = null;

  return {

    /** @override */
    decorate: function() {
      demoPreferencesModule = $('demo-preferences-content');
      demoPreferencesModule.screen = this;

      this.context.addObserver(
          CONTEXT_KEY_INPUT_METHOD, function(inputMethodId) {
            $('demo-preferences-content').setSelectedKeyboard(inputMethodId);
          });
      this.updateLocalizedContent();
    },

    /** Returns a control which should receive an initial focus. */
    get defaultControl() {
      return demoPreferencesModule;
    },

    /** Called after resources are updated. */
    updateLocalizedContent: function() {
      demoPreferencesModule.updateLocalizedContent();
    },

    /**
     * Called when language was selected.
     * @param {string} languageId Id of the selected language.
     */
    onLanguageSelected_: function(languageId) {
      this.context.set(CONTEXT_KEY_LOCALE, languageId);
      this.commitContextChanges();
    },

    /**
     * Called when keyboard was selected.
     * @param {string} inputMethodId Id of the selected input method.
     */
    onKeyboardSelected_: function(inputMethodId) {
      this.context.set(CONTEXT_KEY_INPUT_METHOD, inputMethodId);
      this.commitContextChanges();
    },
  };
});
