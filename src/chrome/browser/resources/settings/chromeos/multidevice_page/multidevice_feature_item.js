// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Item for an individual multidevice feature. These features appear in the
 * multidevice subpage to allow the user to individually toggle them as long as
 * the phone is enabled as a multidevice host. The feature items contain basic
 * information relevant to the individual feature, such as a route to the
 * feature's autonomous page if there is one.
 */
Polymer({
  is: 'settings-multidevice-feature-item',

  behaviors: [MultiDeviceFeatureBehavior, settings.RouteOriginBehavior],

  properties: {
    /** @type {!settings.MultiDeviceFeature} */
    feature: Number,

    /**
     * If it is truthy, the item should be actionable and clicking on it should
     * navigate to the provided route. Otherwise, the item is simply not
     * actionable.
     * @type {!settings.Route|undefined}
     */
    subpageRoute: Object,

    /**
     * A tooltip to show over a help icon. If unset, no help icon is shown.
     */
    iconTooltip: String,

    /**
     * A Chrome icon asset to use as a help icon. The icon is not shown if
     * iconTooltip is unset. Defaults to cr:info-outline.
     */
    icon: {
      type: String,
      value: 'cr:info-outline',
    },

    /**
     * URLSearchParams for subpage route. No param is provided if it is
     * undefined.
     * @type {URLSearchParams|undefined}
     */
    subpageRouteUrlSearchParams: Object,

    /** Whether if the feature is a sub-feature */
    isSubFeature: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /** Whether feature icon is present next to text in row */
    isFeatureIconHidden: {
      type: Boolean,
      value: false,
    }
  },

  /** settings.RouteOriginBehavior override */
  route_: settings.routes.MULTIDEVICE_FEATURES,

  ready() {
    this.addFocusConfig(this.subpageRoute, '#subpageButton');
  },

  /** @override */
  focus() {
    const slot = this.$$('slot[name="feature-controller"]');
    const elems = slot.assignedElements({flatten: true});
    assert(elems.length > 0);
    // Elems contains any elements that override the feature controller. If none
    // exist, contains the default toggle elem.
    elems[0].focus();
  },

  /**
   * @return {boolean}
   * @private
   */
  isRowClickable_() {
    return this.hasSubpageClickHandler_() ||
        this.isFeatureStateEditable(this.feature);
  },

  /**
   * @return {boolean}
   * @private
   */
  hasSubpageClickHandler_() {
    return !!this.subpageRoute && this.isFeatureAllowedByPolicy(this.feature);
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowSeparator_() {
    return this.hasSubpageClickHandler_() || !!this.iconTooltip;
  },

  /** @private */
  handleItemClick_(event) {
    // We do not navigate away if the click was on a link.
    if (event.path[0].tagName === 'A') {
      event.stopPropagation();
      return;
    }

    if (!this.hasSubpageClickHandler_()) {
      if (this.isFeatureStateEditable(this.feature)) {
        // Toggle the editable feature if the feature is editable and does not
        // link to a subpage.
        const toggleButton =
            /** @type{SettingsMultideviceFeatureToggleElement} */
            (this.shadowRoot.querySelector(
                'settings-multidevice-feature-toggle'));
        toggleButton.toggleFeature();
      }
      return;
    }

    // Remove the search term when navigating to avoid potentially having any
    // visible search term reappear at a later time. See
    // https://crbug.com/989119.
    settings.Router.getInstance().navigateTo(
        /** @type {!settings.Route} */ (this.subpageRoute),
        this.subpageRouteUrlSearchParams, true /* opt_removeSearch */);
  },


  /**
   * The class name used for given multidevice feature item text container
   * Checks if icon is present next to text to determine if class 'middle'
   * applies
   * @param {boolean} isFeatureIconHidden
   * @return {string}
   * @private
   */
  getItemTextContainerClassName_(isFeatureIconHidden) {
    return isFeatureIconHidden ? 'start' : 'middle';
  },
});
