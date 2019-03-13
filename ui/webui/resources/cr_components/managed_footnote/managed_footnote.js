// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for indicating that this user is managed by
 * their organization. This component uses the |isManaged| boolean in
 * loadTimeData, and the |managedByOrg| i18n string.
 *
 * If |isManaged| is false, this component is hidden. If |isManaged| is true, it
 * becomes visible.
 */

(function() {
Polymer({
  is: 'managed-footnote',

  behaviors: [I18nBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * Whether the browser is managed by their organization through enterprise
     * policies.
     * @private
     */
    isManaged_: {
      reflectToAttribute: true,
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('isManaged');
      },
    },
  },

  /** @override */
  ready: function() {
    this.addWebUIListener('is-managed-changed', managed => {
      loadTimeData.overrideValues({isManaged: managed});
      this.isManaged_ = managed;
    });
  },
});

chrome.send('observeManagedUI');
})();
