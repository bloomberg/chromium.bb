// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'all-sites' is the polymer element for showing the list of all sites under
 * Site Settings.
 */
Polymer({
  is: 'all-sites',

  behaviors: [
    SiteSettingsBehavior,
    WebUIListenerBehavior,
    settings.RouteObserverBehavior,
    settings.GlobalScrollTargetBehavior,
  ],

  properties: {
    /**
     * Map containing sites to display in the widget, grouped into their
     * eTLD+1 names.
     * @type {!Map<string, !SiteGroup>}
     */
    siteGroupMap: {
      type: Object,
      value: function() {
        return new Map();
      },
    },

    /**
     * Needed by GlobalScrollTargetBehavior.
     * @override
     */
    subpageRoute: {
      type: Object,
      value: settings.routes.SITE_SETTINGS_ALL,
      readOnly: true,
    },

    /**
     * The search query entered into the All Sites search textbox. Used to
     * filter the All Sites list.
     * @private
     */
    searchQuery_: {
      type: String,
      value: '',
    },

    /**
     * All possible sort methods.
     * @type {!{name: string, mostVisited: string, storage: string}}
     * @private
     */
    sortMethods_: {
      type: Object,
      value: {
        name: 'name',
        mostVisited: 'most-visited',
        storage: 'data-stored',
      },
      readOnly: true,
    },

    /**
     * Stores the last selected item in the All Sites list.
     * @type {?{item: !SiteGroup, index: number}}
     * @private
     */
    selectedItem_: Object,

    /**
     * Used to determine focus between settings pages.
     * @type {!Map<string, (string|Function)>}
     */
    focusConfig: {
      type: Object,
      observer: 'focusConfigChanged_',
    },
  },

  /** @private {?settings.LocalDataBrowserProxy} */
  localDataBrowserProxy_: null,

  /** @override */
  created: function() {
    this.localDataBrowserProxy_ =
        settings.LocalDataBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready: function() {
    this.addWebUIListener(
        'onLocalStorageListFetched', this.onLocalStorageListFetched.bind(this));
    this.addWebUIListener(
        'contentSettingSitePermissionChanged', this.populateList_.bind(this));
    this.addEventListener(
        'site-entry-selected',
        (/** @type {!{detail: !{item: !SiteGroup, index: number}}} */ e) => {
          this.selectedItem_ = e.detail;
        });
    this.addEventListener('site-entry-storage-updated', () => {
      this.debounce('site-entry-storage-updated', () => {
        if (this.sortMethods_ &&
            this.$.sortMethod.value == this.sortMethods_.storage) {
          this.onSortMethodChanged_();
        }
      }, 500);
    });
    this.populateList_();
  },

  /** @override */
  attached: function() {
    // Set scrollOffset so the iron-list scrolling accounts for the space the
    // title takes.
    Polymer.RenderStatus.afterNextRender(this, () => {
      this.$.allSitesList.scrollOffset = this.$.allSitesList.offsetTop;
    });
  },

  /**
   * Retrieves a list of all known sites with site details.
   * @private
   */
  populateList_: function() {
    /** @type {!Array<settings.ContentSettingsTypes>} */
    const contentTypes = this.getCategoryList();
    // Make sure to include cookies, because All Sites handles data storage +
    // cookies as well as regular settings.ContentSettingsTypes.
    if (!contentTypes.includes(settings.ContentSettingsTypes.COOKIES))
      contentTypes.push(settings.ContentSettingsTypes.COOKIES);

    this.browserProxy.getAllSites(contentTypes).then((response) => {
      response.forEach(siteGroup => {
        this.siteGroupMap.set(siteGroup.etldPlus1, siteGroup);
      });
      this.forceListUpdate_();
    });
  },

  /**
   * Integrate sites using local storage into the existing sites map, as there
   * may be overlap between the existing sites.
   * @param {!Array<!SiteGroup>} list The list of sites using local storage.
   */
  onLocalStorageListFetched: function(list) {
    list.forEach(storageSiteGroup => {
      if (this.siteGroupMap.has(storageSiteGroup.etldPlus1)) {
        const siteGroup = this.siteGroupMap.get(storageSiteGroup.etldPlus1);
        const storageOriginInfoMap = new Map();
        storageSiteGroup.origins.forEach(
            originInfo =>
                storageOriginInfoMap.set(originInfo.origin, originInfo));

        // If there is an overlapping origin, update the original
        // |originInfo|.
        siteGroup.origins.forEach(originInfo => {
          if (!storageOriginInfoMap.has(originInfo.origin))
            return;
          Object.apply(originInfo, storageOriginInfoMap.get(originInfo.origin));
          storageOriginInfoMap.delete(originInfo.origin);
        });
        // Otherwise, add it to the list.
        storageOriginInfoMap.forEach(
            originInfo => siteGroup.origins.push(originInfo));
      } else {
        this.siteGroupMap.set(storageSiteGroup.etldPlus1, storageSiteGroup);
      }
    });
    this.forceListUpdate_();
  },

  /**
   * Filters the all sites list with the given search query text.
   * @param {!Map<string, !SiteGroup>} siteGroupMap The map of sites to filter.
   * @param {string} searchQuery The filter text.
   * @return {!Array<!SiteGroup>}
   * @private
   */
  filterPopulatedList_: function(siteGroupMap, searchQuery) {
    const result = [];
    for (const [etldPlus1, siteGroup] of siteGroupMap) {
      if (siteGroup.origins.find(
              originInfo => originInfo.origin.includes(searchQuery))) {
        result.push(siteGroup);
      }
    }
    return this.sortSiteGroupList_(result);
  },

  /**
   * Sorts the given SiteGroup list with the currently selected sort method.
   * @param {!Array<!SiteGroup>} siteGroupList The list of sites to sort.
   * @return {!Array<!SiteGroup>}
   * @private
   */
  sortSiteGroupList_: function(siteGroupList) {
    const sortMethod = this.$.sortMethod.value;
    if (!this.sortMethods_)
      return siteGroupList;

    if (sortMethod == this.sortMethods_.mostVisited) {
      siteGroupList.sort(this.mostVisitedComparator_);
    } else if (sortMethod == this.sortMethods_.storage) {
      // Storage is loaded asynchronously, so make sure it's updated for every
      // item in the list to ensure the sorting is correct.
      const etldPlus1List = siteGroupList.reduce((list, siteGroup) => {
        if (siteGroup.origins.length > 1 && siteGroup.etldPlus1.length > 0)
          list.push(siteGroup.etldPlus1);
        return list;
      }, []);

      this.localDataBrowserProxy_.getNumCookiesList(etldPlus1List)
          .then(numCookiesList => {
            assert(etldPlus1List.length == numCookiesList.length);
            numCookiesList.forEach(cookiesPerEtldPlus1 => {
              this.siteGroupMap.get(cookiesPerEtldPlus1.etldPlus1).numCookies =
                  cookiesPerEtldPlus1.numCookies;
            });

            // |siteGroupList| by this point should have already been provided
            // to the iron list, so just sort in-place here and make sure to
            // re-render the item order.
            siteGroupList.sort(this.storageComparator_);
            this.$.allSitesList.fire('iron-resize');
          });
    } else if (sortMethod == this.sortMethods_.name) {
      siteGroupList.sort(this.nameComparator_);
    }
    return siteGroupList;
  },

  /**
   * Comparator used to sort SiteGroups by the amount of engagement the user has
   * with the origins listed inside it. Note only the maximum engagement is used
   * for each SiteGroup (as opposed to the sum) in order to prevent domains with
   * higher numbers of origins from always floating to the top of the list.
   * @param {!SiteGroup} siteGroup1
   * @param {!SiteGroup} siteGroup2
   * @private
   */
  mostVisitedComparator_: function(siteGroup1, siteGroup2) {
    const getMaxEngagement = (max, originInfo) => {
      return (max > originInfo.engagement) ? max : originInfo.engagement;
    };
    const score1 = siteGroup1.origins.reduce(getMaxEngagement, 0);
    const score2 = siteGroup2.origins.reduce(getMaxEngagement, 0);
    return score2 - score1;
  },

  /**
   * Comparator used to sort SiteGroups by the amount of storage they use. Note
   * this sorts in descending order.
   * TODO(https://crbug.com/835712): Account for website storage in sorting by
   * storage used.
   * @param {!SiteGroup} siteGroup1
   * @param {!SiteGroup} siteGroup2
   * @private
   */
  storageComparator_: function(siteGroup1, siteGroup2) {
    const getOverallUsage = siteGroup => {
      let usage = 0;
      siteGroup.origins.forEach(originInfo => {
        usage += originInfo.usage;
      });
      return usage;
    };

    const siteGroup1Size = getOverallUsage(siteGroup1);
    const siteGroup2Size = getOverallUsage(siteGroup2);
    // Use the number of cookies as a tie breaker.
    return siteGroup2Size - siteGroup1Size ||
        siteGroup2.numCookies - siteGroup1.numCookies;
  },

  /**
   * Comparator used to sort SiteGroups by their eTLD+1 name (domain).
   * @param {!SiteGroup} siteGroup1
   * @param {!SiteGroup} siteGroup2
   * @private
   */
  nameComparator_: function(siteGroup1, siteGroup2) {
    return siteGroup1.etldPlus1.localeCompare(siteGroup2.etldPlus1);
  },

  /**
   * Called when the input text in the search textbox is updated.
   * @private
   */
  onSearchChanged_: function() {
    const searchElement = /** @type {SettingsSubpageSearchElement} */ (
        this.$$('settings-subpage-search'));
    this.searchQuery_ = searchElement.getSearchInput().value.toLowerCase();
  },

  /**
   * Called when the user chooses a different sort method to the default.
   * @private
   */
  onSortMethodChanged_: function() {
    this.$.allSitesList.items =
        this.sortSiteGroupList_(this.$.allSitesList.items);
    // Force the iron-list to rerender its items, as the order has changed.
    this.$.allSitesList.fire('iron-resize');
  },

  /**
   * Forces the all sites list to update its list of items, taking into account
   * the search query and the sort method, then re-renders it.
   * @private
   */
  forceListUpdate_: function() {
    this.$.allSitesList.items =
        this.filterPopulatedList_(this.siteGroupMap, this.searchQuery_);
    this.$.allSitesList.fire('iron-resize');
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

    if (!settings.routes.SITE_SETTINGS_ALL)
      return;

    const onNavigatedTo = () => {
      this.async(() => {
        if (this.selectedItem_ == null || this.siteGroupMap.size == 0)
          return;

        // Focus the site-entry to ensure the iron-list renders it, otherwise
        // the query selector will not be able to find it. Note the index is
        // used here instead of the item, in case the item was already removed.
        const index = Math.max(
            0, Math.min(this.selectedItem_.index, this.siteGroupMap.size));
        this.$.allSitesList.focusItem(index);
        this.selectedItem_ = null;
      });
    };

    this.focusConfig.set(
        settings.routes.SITE_SETTINGS_SITE_DETAILS.path, onNavigatedTo);
  },
});
