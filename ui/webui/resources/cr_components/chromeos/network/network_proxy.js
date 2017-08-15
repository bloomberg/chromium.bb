// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying and editing network proxy
 * values.
 */
Polymer({
  is: 'network-proxy',

  behaviors: [
    CrPolicyNetworkBehavior,
    I18nBehavior,
  ],

  properties: {
    /**
     * The network properties dictionary containing the proxy properties to
     * display and modify.
     * @type {!CrOnc.NetworkProperties|undefined}
     */
    networkProperties: {
      type: Object,
      observer: 'networkPropertiesChanged_',
    },

    /** Whether or not the proxy values can be edited. */
    editable: {
      type: Boolean,
      value: false,
    },

    /** Whether shared proxies are allowed. */
    useSharedProxies: {
      type: Boolean,
      value: false,
      observer: 'updateProxy_',
    },

    /**
     * UI visible / edited proxy configuration.
     * @private {!CrOnc.ProxySettings}
     */
    proxy_: {
      type: Object,
      value: function() {
        return this.createDefaultProxySettings_();
      },
    },

    /**
     * The Web Proxy Auto Discovery URL extracted from networkProperties.
     * @private
     */
    WPAD_: {
      type: String,
      value: '',
    },

    /**
     * Whether or not to use the same manual proxy for all protocols.
     * @private
     */
    useSameProxy_: {
      type: Boolean,
      value: false,
      observer: 'useSameProxyChanged_',
    },

    /**
     * Array of proxy configuration types.
     * @private {!Array<string>}
     * @const
     */
    proxyTypes_: {
      type: Array,
      value: [
        CrOnc.ProxySettingsType.DIRECT,
        CrOnc.ProxySettingsType.PAC,
        CrOnc.ProxySettingsType.WPAD,
        CrOnc.ProxySettingsType.MANUAL,
      ],
      readOnly: true
    },

    /**
     * Object providing proxy type values for data binding.
     * @private {!Object}
     * @const
     */
    ProxySettingsType_: {
      type: Object,
      value: {
        DIRECT: CrOnc.ProxySettingsType.DIRECT,
        PAC: CrOnc.ProxySettingsType.PAC,
        MANUAL: CrOnc.ProxySettingsType.MANUAL,
        WPAD: CrOnc.ProxySettingsType.WPAD,
      },
      readOnly: true,
    },
  },

  /**
   * Saved Manual properties so that switching to another type does not loose
   * any set properties while the UI is open.
   * @private {!CrOnc.ManualProxySettings|undefined}
   */
  savedManual_: undefined,

  /**
   * Saved ExcludeDomains properties so that switching to a non-Manual type does
   * not loose any set exclusions while the UI is open.
   * @private {!Array<string>|undefined}
   */
  savedExcludeDomains_: undefined,

  /**
   * Set to true while modifying proxy values so that an update does not
   * override the edited values.
   * @private {boolean}
   */
  proxyModified_: false,

  /** @override */
  attached: function() {
    this.reset();
  },

  /**
   * Called any time the page is refreshed or navigated to so that the proxy
   * is updated correctly.
   */
  reset: function() {
    this.proxyModified_ = false;
    this.proxy_ = this.createDefaultProxySettings_();
    this.updateProxy_();
  },

  /** @private */
  networkPropertiesChanged_: function() {
    if (this.proxyModified_)
      return;  // Ignore update.
    this.updateProxy_();
  },

  /** @private */
  updateProxy_: function() {
    if (!this.networkProperties)
      return;

    /** @type {!CrOnc.ProxySettings} */
    var proxy = this.createDefaultProxySettings_();

    // For shared networks with unmanaged proxy settings, ignore any saved
    // proxy settings (use the default values).
    if (this.isShared_()) {
      var property = this.getProxySettingsTypeProperty_();
      if (!this.isControlled(property) && !this.useSharedProxies) {
        this.setProxyAsync_(proxy);
        return;  // Proxy settings will be ignored.
      }
    }

    /** @type {!chrome.networkingPrivate.ManagedProxySettings|undefined} */
    var proxySettings = this.networkProperties.ProxySettings;
    if (proxySettings) {
      proxy.Type = /** @type {!CrOnc.ProxySettingsType} */ (
          CrOnc.getActiveValue(proxySettings.Type));
      if (proxySettings.Manual) {
        proxy.Manual.HTTPProxy = /** @type {!CrOnc.ProxyLocation|undefined} */ (
                                     CrOnc.getSimpleActiveProperties(
                                         proxySettings.Manual.HTTPProxy)) ||
            {Host: '', Port: 80};
        proxy.Manual.SecureHTTPProxy =
            /** @type {!CrOnc.ProxyLocation|undefined} */ (
                CrOnc.getSimpleActiveProperties(
                    proxySettings.Manual.SecureHTTPProxy)) ||
            {Host: '', Port: 80};
        proxy.Manual.FTPProxy =
            /** @type {!CrOnc.ProxyLocation|undefined} */ (
                CrOnc.getSimpleActiveProperties(
                    proxySettings.Manual.FTPProxy)) ||
            {Host: '', Port: 80};
        proxy.Manual.SOCKS =
            /** @type {!CrOnc.ProxyLocation|undefined} */ (
                CrOnc.getSimpleActiveProperties(proxySettings.Manual.SOCKS)) ||
            {Host: '', Port: 80};
        var jsonHttp = proxy.Manual.HTTPProxy;
        this.useSameProxy_ =
            (CrOnc.proxyMatches(jsonHttp, proxy.Manual.SecureHTTPProxy) &&
             CrOnc.proxyMatches(jsonHttp, proxy.Manual.FTPProxy) &&
             CrOnc.proxyMatches(jsonHttp, proxy.Manual.SOCKS)) ||
            (!proxy.Manual.SecureHTTPProxy.Host &&
             !proxy.Manual.FTPProxy.Host && !proxy.Manual.SOCKS.Host);
      }
      if (proxySettings.ExcludeDomains) {
        proxy.ExcludeDomains = /** @type {!Array<string>|undefined} */ (
            CrOnc.getActiveValue(proxySettings.ExcludeDomains));
      }
      proxy.PAC = /** @type {string|undefined} */ (
          CrOnc.getActiveValue(proxySettings.PAC));
    }
    // Use saved ExcludeDomains and Manual if not defined.
    proxy.ExcludeDomains = proxy.ExcludeDomains || this.savedExcludeDomains_;
    proxy.Manual = proxy.Manual || this.savedManual_;

    // Set the Web Proxy Auto Discovery URL.
    var ipv4 =
        CrOnc.getIPConfigForType(this.networkProperties, CrOnc.IPType.IPV4);
    this.WPAD_ = (ipv4 && ipv4.WebProxyAutoDiscoveryUrl) || '';

    this.setProxyAsync_(proxy);
  },

  /**
   * @param {!CrOnc.ProxySettings} proxy
   * @private
   */
  setProxyAsync_: function(proxy) {
    // Set this.proxy_ after dom-repeat has been stamped.
    this.async(() => {
      this.proxy_ = proxy;
      this.proxyModified_ = false;
    });
  },

  /** @private */
  useSameProxyChanged_: function() {
    this.proxyModified_ = true;
  },

  /**
   * @return {CrOnc.ProxySettings} An empty/default proxy settings object.
   * @private
   */
  createDefaultProxySettings_: function() {
    return {
      Type: CrOnc.ProxySettingsType.DIRECT,
      ExcludeDomains: [],
      Manual: {
        HTTPProxy: {Host: '', Port: 80},
        SecureHTTPProxy: {Host: '', Port: 80},
        FTPProxy: {Host: '', Port: 80},
        SOCKS: {Host: '', Port: 1080}
      },
      PAC: ''
    };
  },

  /**
   * Called when the proxy changes in the UI.
   * @private
   */
  sendProxyChange_: function() {
    var proxy =
        /** @type {!CrOnc.ProxySettings} */ (Object.assign({}, this.proxy_));
    if (proxy.Type == CrOnc.ProxySettingsType.MANUAL) {
      var manual = proxy.Manual;
      var defaultProxy = manual.HTTPProxy || {Host: '', Port: 80};
      if (this.useSameProxy_) {
        proxy.Manual.SecureHTTPProxy = /** @type {!CrOnc.ProxyLocation} */ (
            Object.assign({}, defaultProxy));
        proxy.Manual.FTPProxy = /** @type {!CrOnc.ProxyLocation} */ (
            Object.assign({}, defaultProxy));
        proxy.Manual.SOCKS = /** @type {!CrOnc.ProxyLocation} */ (
            Object.assign({}, defaultProxy));
      } else {
        // Remove properties with empty hosts to unset them.
        if (manual.HTTPProxy && !manual.HTTPProxy.Host)
          delete manual.HTTPProxy;
        if (manual.SecureHTTPProxy && !manual.SecureHTTPProxy.Host)
          delete manual.SecureHTTPProxy;
        if (manual.FTPProxy && !manual.FTPProxy.Host)
          delete manual.FTPProxy;
        if (manual.SOCKS && !manual.SOCKS.Host)
          delete manual.SOCKS;
      }
      this.savedManual_ = Object.assign({}, manual);
      this.savedExcludeDomains_ = proxy.ExcludeDomains;
    } else if (proxy.Type == CrOnc.ProxySettingsType.PAC) {
      if (!proxy.PAC)
        return;
    }
    this.fire('proxy-change', {field: 'ProxySettings', value: proxy});
    this.proxyModified_ = false;
  },

  /**
   * Event triggered when the selected proxy type changes.
   * @param {!Event} event
   * @private
   */
  onTypeChange_: function(event) {
    var target = /** @type {!HTMLSelectElement} */ (event.target);
    var type = /** @type {chrome.networkingPrivate.ProxySettingsType} */ (
        target.value);
    this.set('proxy_.Type', type);
    if (type == CrOnc.ProxySettingsType.MANUAL)
      this.proxyModified_ = true;
    else
      this.sendProxyChange_();
  },

  /** @private */
  onPACChange_: function() {
    this.sendProxyChange_();
  },

  /** @private */
  onProxyInputChange_: function() {
    this.proxyModified_ = true;
  },

  /**
   * Event triggered when a proxy exclusion is added.
   * @param {!Event} event The add proxy exclusion event.
   * @private
   */
  onAddProxyExclusionTap_: function(event) {
    var value = this.$.proxyExclusion.value;
    if (!value)
      return;
    this.push('proxy_.ExcludeDomains', value);
    // Clear input.
    this.$.proxyExclusion.value = '';
    this.proxyModified_ = true;
  },

  /**
   * Event triggered when the proxy exclusion list changes.
   * @param {!Event} event The remove proxy exclusions change event.
   * @private
   */
  onProxyExclusionsChange_: function(event) {
    this.proxyModified_ = true;
  },

  /** @private */
  onSaveProxyTap_: function() {
    this.sendProxyChange_();
  },

  /**
   * @param {string} proxyType The proxy type.
   * @return {string} The description for |proxyType|.
   * @private
   */
  getProxyTypeDesc_: function(proxyType) {
    if (proxyType == CrOnc.ProxySettingsType.MANUAL)
      return this.i18n('networkProxyTypeManual');
    if (proxyType == CrOnc.ProxySettingsType.PAC)
      return this.i18n('networkProxyTypePac');
    if (proxyType == CrOnc.ProxySettingsType.WPAD)
      return this.i18n('networkProxyTypeWpad');
    return this.i18n('networkProxyTypeDirect');
  },

  /**
   * @return {!CrOnc.ManagedProperty|undefined}
   * @private
   */
  getProxySettingsTypeProperty_: function() {
    return /** @type {!CrOnc.ManagedProperty|undefined} */ (
        this.get('ProxySettings.Type', this.networkProperties));
  },

  /**
   * @param {string} propertyName
   * @return {boolean} Whether the named property setting is editable.
   * @private
   */
  isEditable_: function(propertyName) {
    if (!this.editable || (this.isShared_() && !this.useSharedProxies))
      return false;
    if (!this.networkProperties.hasOwnProperty('ProxySettings'))
      return true;  // No proxy settings defined, so not enforced.
    var property = /** @type {!CrOnc.ManagedProperty|undefined} */ (
        this.get('ProxySettings.' + propertyName, this.networkProperties));
    if (!property)
      return true;
    return this.isPropertyEditable_(property);
  },

  /**
   * @param {!CrOnc.ManagedProperty|undefined} property
   * @return {boolean} Whether |property| is editable.
   * @private
   */
  isPropertyEditable_: function(property) {
    return !this.isNetworkPolicyEnforced(property) &&
        !this.isExtensionControlled(property);
  },

  /**
   * @return {boolean}
   * @private
   */
  isShared_: function() {
    return this.networkProperties.Source == 'Device' ||
        this.networkProperties.Source == 'DevicePolicy';
  },

  /**
   * @return {boolean}
   * @private
   */
  isSaveManualProxyEnabled_: function() {
    if (!this.proxyModified_)
      return false;
    var manual = this.proxy_.Manual;
    var httpHost = this.get('HTTPProxy.Host', manual);
    if (this.useSameProxy_)
      return !!httpHost;
    return !!httpHost || !!this.get('SecureHTTPProxy.Host', manual) ||
        !!this.get('FTPProxy.Host', manual) || !!this.get('SOCKS.Host', manual);
  },

  /**
   * @param {string} property The property to test
   * @param {string} value The value to test against
   * @return {boolean} True if property == value
   * @private
   */
  matches_: function(property, value) {
    return property == value;
  },
});
