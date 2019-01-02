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
   * @return {string} 'plus-icon' if settings are collapsed, 'minus-icon' if
   *     they are expanded.
   * @private
   */
  getIconClass_: function() {
    return this.settingsExpandedByUser ? 'minus-icon' : 'plus-icon';
  },

  /**
   * @return {string} The text to display on the label.
   * @private
   */
  getLabelText_: function() {
    return this.i18n(
        this.settingsExpandedByUser ? 'lessOptionsLabel' : 'moreOptionsLabel');
  },

  /**
   * @return {string} 'true' if the settings are expanded, 'false' otherwise
   * @private
   */
  getAriaExpanded_: function() {
    return this.settingsExpandedByUser ? 'true' : 'false';
  },

  /** @private */
  onMoreSettingsClick_: function() {
    this.settingsExpandedByUser = !this.settingsExpandedByUser;
    this.metrics_.record(
        this.settingsExpandedByUser ?
            print_preview.Metrics.PrintSettingsUiBucket.MORE_SETTINGS_CLICKED :
            print_preview.Metrics.PrintSettingsUiBucket.LESS_SETTINGS_CLICKED);
  },
});
