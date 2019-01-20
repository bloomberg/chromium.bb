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
/**
 * URL of the help article for the clickable link.
 * @type {string}
 */
// TODO(nicolaso): Use a p-link instead, once it's available. b/117655761
const HELP_ARTICLE_URL = 'https://support.google.com/chromebook/answer/1331549';

Polymer({
  is: 'managed-footnote',

  behaviors: [I18nBehavior],

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

    /**
     * Localized message to display in the footnote. May contain an <a>
     * element.
     * @private
     */
    message_: String,
  },

  /** @override */
  ready: function() {
    this.message_ = this.i18nAdvanced('managedByOrg', {
      substitutions: [HELP_ARTICLE_URL],
      tags: ['a'],
      attrs: {
        target: (node, v) => v === '_blank',
        href: (node, v) => v === HELP_ARTICLE_URL,
      },
    });

    cr.addWebUIListener('is-managed-changed', managed => {
      loadTimeData.overrideValues({isManaged: managed});
      this.isManaged_ = managed;
    });
  },
});

chrome.send('observeManagedUI');

})();
