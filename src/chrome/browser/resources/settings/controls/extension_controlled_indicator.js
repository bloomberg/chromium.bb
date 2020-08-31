// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'extension-controlled-indicator',

  behaviors: [I18nBehavior],

  properties: {
    extensionCanBeDisabled: Boolean,
    extensionId: String,
    extensionName: String,
  },

  /**
   * @param {string} extensionId
   * @param {string} extensionName
   * @return {string}
   * @private
   */
  getLabel_(extensionId, extensionName) {
    if (this.extensionId == undefined || this.extensionName == undefined) {
      return '';
    }

    const manageUrl = 'chrome://extensions/?id=' + this.extensionId;
    return this.i18nAdvanced('controlledByExtension', {
      substitutions:
          ['<a href="' + manageUrl + '" target="_blank">' + this.extensionName +
           '</a>'],
    });
  },

  /** @private */
  onDisableTap_() {
    assert(this.extensionCanBeDisabled);
    settings.ExtensionControlBrowserProxyImpl.getInstance().disableExtension(
        assert(this.extensionId));
    this.fire('extension-disable');
  },
});
