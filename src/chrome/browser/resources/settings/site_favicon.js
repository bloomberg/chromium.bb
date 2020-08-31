// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'site-favicon' is the section to display the favicon given the
 * site URL.
 */

import {getFavicon, getFaviconForPageURL} from 'chrome://resources/js/icon.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'site-favicon',

  _template: html`{__html_template__}`,

  properties: {
    faviconUrl: String,
    url: String,
  },

  /** @private */
  getBackgroundImage_() {
    let backgroundImage = getFavicon('');
    if (this.faviconUrl) {
      const url = this.ensureUrlHasScheme_(this.faviconUrl);
      backgroundImage = getFavicon(url);
    } else if (this.url) {
      let url = this.removePatternWildcard_(this.url);
      url = this.ensureUrlHasScheme_(url);
      backgroundImage = getFaviconForPageURL(url || '', false);
    }
    return backgroundImage;
  },

  /**
   * Removes the wildcard prefix from a pattern string.
   * @param {string} pattern The pattern to remove the wildcard from.
   * @return {string} The resulting pattern.
   * @private
   */
  removePatternWildcard_(pattern) {
    if (!pattern || pattern.length === 0) {
      return pattern;
    }

    if (pattern.startsWith('http://[*.]')) {
      return pattern.replace('http://[*.]', 'http://');
    } else if (pattern.startsWith('https://[*.]')) {
      return pattern.replace('https://[*.]', 'https://');
    } else if (pattern.startsWith('[*.]')) {
      return pattern.substring(4, pattern.length);
    }
    return pattern;
  },

  /**
   * Ensures the URL has a scheme (assumes http if omitted).
   * @param {string} url The URL with or without a scheme.
   * @return {string} The URL with a scheme, or an empty string.
   * @private
   */
  ensureUrlHasScheme_(url) {
    if (!url || url.length === 0) {
      return url;
    }
    return url.includes('://') ? url : 'http://' + url;
  },
});
