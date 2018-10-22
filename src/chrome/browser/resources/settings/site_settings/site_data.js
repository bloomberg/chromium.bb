// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'site-data' handles showing the local storage summary list for all sites.
 */

/**
 * @typedef {{
 *   site: string,
 *   id: string,
 *   localData: string,
 * }}
 */
let CookieDataSummaryItem;

/**
 * @typedef {{
 *   id: string,
 *   start: number,
 *   count: number,
 * }}
 */
let CookieRemovePacket;

Polymer({
  is: 'site-data',

  behaviors: [
    I18nBehavior,
    ListPropertyUpdateBehavior,
    settings.GlobalScrollTargetBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * The current filter applied to the cookie data list.
     */
    filter: {
      observer: 'updateSiteList_',
      notify: true,
      type: String,
    },

    /** @type {!Map<string, (string|Function)>} */
    focusConfig: {
      type: Object,
      observer: 'focusConfigChanged_',
    },

    isLoading_: Boolean,

    /** @type {!Array<!CookieDataSummaryItem>} */
    sites: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * settings.GlobalScrollTargetBehavior
     * @override
     */
    subpageRoute: {
      type: Object,
      value: settings.routes.SITE_SETTINGS_SITE_DATA,
    },

    /** @private */
    lastFocused_: Object,
  },

  /** @private {settings.LocalDataBrowserProxy} */
  browserProxy_: null,

  /**
   * When navigating to site data details sub-page, |lastSelected_| holds the
   * site name as well as the index of the selected site. This is used when
   * navigating back to site data in order to focus on the correct site.
   * @private {!{item: CookieDataSummaryItem, index: number}|null}
   */
  lastSelected_: null,

  /** @override */
  ready: function() {
    this.browserProxy_ = settings.LocalDataBrowserProxyImpl.getInstance();
    this.addWebUIListener(
        'on-tree-item-removed', this.updateSiteList_.bind(this));
  },

  /**
   * Reload cookies when the site data page is visited.
   *
   * settings.RouteObserverBehavior
   * @param {!settings.Route} currentRoute
   * @protected
   */
  currentRouteChanged: function(currentRoute) {
    settings.GlobalScrollTargetBehaviorImpl.currentRouteChanged.call(
        this, currentRoute);
    if (currentRoute == settings.routes.SITE_SETTINGS_SITE_DATA) {
      this.isLoading_ = true;
      // Needed to fix iron-list rendering issue. The list will not render
      // correctly until a scroll occurs.
      // See https://crbug.com/853906.
      const ironList = /** @type {!IronListElement} */ (this.$$('iron-list'));
      ironList.scrollToIndex(0);
      this.browserProxy_.reloadCookies().then(this.updateSiteList_.bind(this));
    }
  },

  /**
   * Returns the icon to use for a given site.
   * @param {string} url The url of the site to fetch the icon for.
   * @return {string} Value for background-image style.
   * @private
   */
  favicon_: function(url) {
    // If the url doesn't have a scheme, inject HTTP as the scheme. Otherwise,
    // the URL isn't valid and no icon will be returned.
    const urlWithScheme = url.includes('://') ? url : 'http://' + url;
    return cr.icon.getFavicon(urlWithScheme);
  },

  /**
   * @param {!Map<string, (string|Function)>} newConfig
   * @param {?Map<string, (string|Function)>} oldConfig
   * @private
   */
  focusConfigChanged_: function(newConfig, oldConfig) {
    // focusConfig is set only once on the parent, so this observer should only
    // fire once.
    assert(!oldConfig);

    // Populate the |focusConfig| map of the parent <settings-animated-pages>
    // element, with additional entries that correspond to subpage trigger
    // elements residing in this element's Shadow DOM.
    if (settings.routes.SITE_SETTINGS_DATA_DETAILS) {
      const onNavigatedTo = () => this.async(() => {
        if (this.lastSelected_ == null || this.sites.length == 0)
          return;

        const lastSelectedSite = this.lastSelected_.item.site;
        const lastSelectedIndex = this.lastSelected_.index;
        this.lastSelected_ = null;

        const indexFromId =
            this.sites.findIndex(site => site.site == lastSelectedSite);

        // If the site is no longer in |sites|, use the index as a fallback.
        // Since the sites are sorted, an alternative could be to select the
        // site that comes next in sort order.
        const indexFallback = lastSelectedIndex < this.sites.length ?
            lastSelectedIndex :
            this.sites.length - 1;
        const index = indexFromId > -1 ? indexFromId : indexFallback;
        this.focusOnSiteSelectButton_(index);
      });
      this.focusConfig.set(
          settings.routes.SITE_SETTINGS_DATA_DETAILS.path, onNavigatedTo);
    }
  },

  /**
   * @param {number} index
   * @private
   */
  focusOnSiteSelectButton_: function(index) {
    const ironList =
        /** @type {!IronListElement} */ (this.$$('iron-list'));
    ironList.focusItem(index);
    const siteToSelect = this.sites[index].site.replace(/[.]/g, '\\.');
    const button =
        this.$$(`#siteItem_${siteToSelect}`).$$('.subpage-arrow button');
    cr.ui.focusWithoutInk(assert(button));
  },

  /**
   * Gather all the site data.
   * @private
   */
  updateSiteList_: function() {
    this.isLoading_ = true;
    this.browserProxy_.getDisplayList(this.filter).then(listInfo => {
      this.updateList(
          'sites', item => `${item.site}_${item.localData}`, listInfo.items);
      this.isLoading_ = false;
      this.fire('site-data-list-complete');
    });
  },

  /**
   * Returns the string to use for the Remove label.
   * @param {string} filter The current filter string.
   * @return {string}
   * @private
   */
  computeRemoveLabel_: function(filter) {
    if (filter.length == 0)
      return loadTimeData.getString('siteSettingsCookieRemoveAll');
    return loadTimeData.getString('siteSettingsCookieRemoveAllShown');
  },

  /** @private */
  onCloseDialog_: function() {
    this.$.confirmDeleteDialog.close();
  },

  /** @private */
  onConfirmDeleteDialogClosed_: function() {
    cr.ui.focusWithoutInk(assert(this.$.removeShowingSites));
  },

  /**
   * Shows a dialog to confirm the deletion of multiple sites.
   * @param {!Event} e
   * @private
   */
  onRemoveShowingSitesTap_: function(e) {
    e.preventDefault();
    this.$.confirmDeleteDialog.showModal();
  },

  /**
   * Called when deletion for all showing sites has been confirmed.
   * @private
   */
  onConfirmDelete_: function() {
    this.$.confirmDeleteDialog.close();
    if (this.filter.length == 0) {
      this.browserProxy_.removeAll().then(() => {
        this.sites = [];
      });
    } else {
      this.browserProxy_.removeShownItems();
      // We just deleted all items found by the filter, let's reset the filter.
      this.fire('clear-subpage-search');
    }
  },

  /**
   * @param {!{model: !{item: CookieDataSummaryItem, index: number}}} event
   * @private
   */
  onSiteClick_: function(event) {
    // If any delete button is selected, the focus will be in a bad state when
    // returning to this page. To avoid this, the site select button is given
    // focus. See https://crbug.com/872197.
    this.focusOnSiteSelectButton_(event.model.index);
    settings.navigateTo(
        settings.routes.SITE_SETTINGS_DATA_DETAILS,
        new URLSearchParams('site=' + event.model.item.site));
    this.lastSelected_ = event.model;
  },
});
