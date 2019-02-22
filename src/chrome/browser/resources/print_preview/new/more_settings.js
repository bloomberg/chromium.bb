// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'print-preview-more-settings',

  behaviors: [I18nBehavior],

  properties: {
    settingsExpandedByUser: {
      type: Boolean,
      notify: true,
    },

    disabled: {
      type: Boolean,
      reflectToAttribute: true,
    },
  },

  /** @private {!print_preview.PrintSettingsUiMetricsContext} */
  metrics_: new print_preview.PrintSettingsUiMetricsContext(),

  /**
   * Toggles the expand button within the element being listened to.
   * @param {!Event} e
   * @private
   */
  toggleExpandButton_: function(e) {
    // The expand button handles toggling itself.
    const expandButtonTag = 'CR-EXPAND-BUTTON';
    if (e.target.tagName == expandButtonTag)
      return;

    if (!e.currentTarget.hasAttribute('actionable'))
      return;

    /** @type {!CrExpandButtonElement} */
    const expandButton = e.currentTarget.querySelector(expandButtonTag);
    assert(expandButton);
    expandButton.expanded = !expandButton.expanded;
    this.metrics_.record(
        this.settingsExpandedByUser ?
            print_preview.Metrics.PrintSettingsUiBucket.MORE_SETTINGS_CLICKED :
            print_preview.Metrics.PrintSettingsUiBucket.LESS_SETTINGS_CLICKED);
  },
});
