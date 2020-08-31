// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../settings_shared_css.m.js';

import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Route, RouteObserverBehavior, Router} from '../router.m.js';

import {CookieDataForDisplay, CookieDetails, getCookieData} from './cookie_info.js';
import {CookieList, LocalDataBrowserProxy, LocalDataBrowserProxyImpl} from './local_data_browser_proxy.js';


const categoryLabels = {
  app_cache: loadTimeData.getString('cookieAppCache'),
  cache_storage: loadTimeData.getString('cookieCacheStorage'),
  database: loadTimeData.getString('cookieDatabaseStorage'),
  file_system: loadTimeData.getString('cookieFileSystem'),
  flash_lso: loadTimeData.getString('cookieFlashLso'),
  indexed_db: loadTimeData.getString('cookieDatabaseStorage'),
  local_storage: loadTimeData.getString('cookieLocalStorage'),
  service_worker: loadTimeData.getString('cookieServiceWorker'),
  shared_worker: loadTimeData.getString('cookieSharedWorker'),
  media_license: loadTimeData.getString('cookieMediaLicense'),
};

/**
 * 'site-data-details-subpage' Display cookie contents.
 */
Polymer({
  is: 'site-data-details-subpage',

  _template: html`{__html_template__}`,

  behaviors: [RouteObserverBehavior, WebUIListenerBehavior],

  properties: {
    /**
     * The cookie entries for the given site.
     * @type {!Array<!CookieDetails>}
     * @private
     */
    entries_: Array,

    /** Set the page title on the settings-subpage parent. */
    pageTitle: {
      type: String,
      notify: true,
    },

    /** @private */
    site_: String,

    /** @private */
    siteId_: String,
  },

  /**
   * The browser proxy used to retrieve and change cookies.
   * @private {?LocalDataBrowserProxy}
   */
  browserProxy_: null,

  /** @override */
  ready() {
    this.browserProxy_ = LocalDataBrowserProxyImpl.getInstance();

    this.addWebUIListener(
        'on-tree-item-removed', this.getCookieDetails_.bind(this));
  },

  /**
   * RouteObserverBehavior
   * @param {!Route} route
   * @protected
   */
  currentRouteChanged(route) {
    if (Router.getInstance().getCurrentRoute() !=
        routes.SITE_SETTINGS_DATA_DETAILS) {
      return;
    }
    const site = Router.getInstance().getQueryParameters().get('site');
    if (!site) {
      return;
    }
    this.site_ = site;
    this.pageTitle = loadTimeData.getStringF('siteSettingsCookieSubpage', site);
    this.getCookieDetails_();
  },

  /** @private */
  getCookieDetails_() {
    if (!this.site_) {
      return;
    }
    this.browserProxy_.getCookieDetails(this.site_)
        .then(
            this.onCookiesLoaded_.bind(this),
            this.onCookiesLoadFailed_.bind(this));
  },

  /**
   * @return {!Array<!CookieDataForDisplay>}
   * @private
   */
  getCookieNodes_(node) {
    return getCookieData(node);
  },

  /**
   * @param {!CookieList} cookies
   * @private
   */
  onCookiesLoaded_(cookies) {
    this.siteId_ = cookies.id;
    this.entries_ = cookies.children;
    // Set up flag for expanding cookie details.
    this.entries_.forEach(function(e) {
      e.expanded_ = false;
    });
  },

  /**
   * The site was not found. E.g. The site data may have been deleted or the
   * site URL parameter may be mistyped.
   * @private
   */
  onCookiesLoadFailed_() {
    this.siteId_ = '';
    this.entries_ = [];
  },

  /**
   * A handler for when the user opts to remove a single cookie.
   * @param {!CookieDetails} item
   * @return {string}
   * @private
   */
  getEntryDescription_(item) {
    // Frequently there are multiple cookies per site. To avoid showing a list
    // of '1 cookie', '1 cookie', ... etc, it is better to show the title of the
    // cookie to differentiate them.
    if (item.type == 'cookie') {
      return item.title;
    }
    if (item.type == 'quota') {
      return item.totalUsage;
    }
    return categoryLabels[item.type];
  },

  /**
   * A handler for when the user opts to remove a single cookie.
   * @param {!Event} event
   * @private
   */
  onRemove_(event) {
    this.browserProxy_.removeCookie(
        /** @type {!CookieDetails} */ (event.currentTarget.dataset).idPath);
  },

  /**
   * A handler for when the user opts to remove all cookies.
   */
  removeAll() {
    this.browserProxy_.removeCookie(this.siteId_);
  },
});
