// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying information about a network
 * in a list based on ONC state properties.
 */

Polymer({
  is: 'network-list-item',

  behaviors: [
    CrPolicyNetworkBehaviorMojo,
    I18nBehavior,
    cr.ui.FocusRowBehavior,
  ],

  properties: {
    /**
     * Dims the UI, disables click and keyboard event handlers.
     * @private
     */
    disabled_: {
      type: Boolean,
      reflectToAttribute: true,
      observer: 'disabledChanged_',
      computed: 'computeDisabled_(deviceState, deviceState.inhibitReason)'
    },

    /** @type {!NetworkList.NetworkListItemType|undefined} */
    item: {
      type: Object,
      observer: 'itemChanged_',
    },

    /**
     * The ONC data properties used to display the list item.
     * @type {!OncMojo.NetworkStateProperties|undefined}
     */
    networkState: {
      type: Object,
      observer: 'networkStateChanged_',
    },

    /** Whether to show any buttons for network items. Defaults to false. */
    showButtons: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /**
     * Reflect the element's tabindex attribute to a property so that embedded
     * elements (e.g. the show subpage button) can become keyboard focusable
     * when this element has keyboard focus.
     */
    tabindex: {
      type: Number,
      value: -1,
    },

    /**
     * Expose the itemName so it can be used as a label for a11y.  It will be
     * added as an attribute on this top-level network-list-item, and can
     * be used by any sub-element which applies it.
     */
    rowLabel: {
      type: String,
      notify: true,
      computed:
          'getRowLabel_(item, networkState, subtitle_, isPSimPendingActivationNetwork_)',
    },

    buttonLabel: {
      type: String,
      computed: 'getButtonLabel_(item)',
    },

    /** Expose the aria role attribute as "button". */
    role: {
      type: String,
      reflectToAttribute: true,
      value: 'button',
    },

    /**
     * The cached ConnectionState for the network.
     * @type {!chromeos.networkConfig.mojom.ConnectionStateType|undefined}
     */
    connectionState_: Number,

    /** Whether to show technology badge on mobile network icon. */
    showTechnologyBadge: {
      type: Boolean,
      value: true,
    },

    /** Whether cellular activation is unavailable in the current context. */
    activationUnavailable: {
      type: Boolean,
      value: false,
    },

    /**
     * DeviceState associated with the network item type, or undefined if none
     * was provided.
     * @private {!OncMojo.DeviceStateProperties|undefined} deviceState
     */
    deviceState: Object,

    /** @private {?chromeos.networkConfig.mojom.ManagedProperties|undefined} */
    managedProperties_: Object,

    /**
     * Title containing the item's name and subtitle.
     * @private {string}
     */
    itemTitle_: {
      type: String,
      value: '',
    },

    /**
     * Subtitle for item.
     * @private {string}
     */
    subtitle_: {
      type: String,
      value: '',
    },

    /** @private */
    isUpdatedCellularUiEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('updatedCellularActivationUi');
      }
    },

    /**
     * Indicates the network item is a pSIM network not yet activated but
     * eligible for activation.
     * @private
     */
    isPSimPendingActivationNetwork_: {
      type: Boolean,
      reflectToAttribute: true,
      value: false,
      computed: 'computeIsPSimPendingActivationNetwork_(managedProperties_)',
    },

    /**
     * Indicates the network item is a pSIM network that is not activated nor
     * available to be activated.
     * @private
     */
    isPSimUnavailableNetwork_: {
      type: Boolean,
      reflectToAttribute: true,
      value: false,
      computed: 'computeIsPSimUnavailableNetwork_(managedProperties_)',
    },

    /**
     * Indicates the network item is a pSIM network currently activating.
     * @private
     */
    isPSimActivatingNetwork_: {
      type: Boolean,
      reflectToAttribute: true,
      value: false,
      computed: 'computeIsPSimActivatingNetwork_(networkState.*)',
    },

    /**
     * Whether the network item is a cellular one and is of an esim
     * pending profile.
     * @private
     */
    isESimPendingProfile_: {
      type: Boolean,
      reflectToAttribute: true,
      value: false,
      computed: 'computeIsESimPendingProfile_(item, item.customItemType)',
    },

    /**
     * Whether the network item is a cellular one and is of an esim
     * installing profile.
     * @private
     */
    isESimInstallingProfile_: {
      type: Boolean,
      reflectToAttribute: true,
      value: false,
      computed: 'computeIsESimInstallingProfile_(item, item.customItemType)',
    },

    /**@private {boolean} */
    isCellularUnlockDialogOpen_: {
      type: Boolean,
      value: false,
    },
  },

  /** @private {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  networkConfig_: null,

  /** @override */
  created() {
    this.networkConfig_ = network_config.MojoInterfaceProviderImpl.getInstance()
                              .getMojoServiceRemote();
  },

  /** @override */
  attached() {
    this.listen(this, 'keydown', 'onKeydown_');
  },

  /** @override */
  detached() {
    this.unlisten(this, 'keydown', 'onKeydown_');
  },

  /** @private */
  async itemChanged_() {
    if (this.item && !this.item.hasOwnProperty('customItemType')) {
      this.networkState =
          /** @type {!OncMojo.NetworkStateProperties} */ (this.item);
    } else {
      this.networkState = undefined;
    }
    // The order each property is set here matters. We don't use observers to
    // set each property or else the ordering is indeterminate.
    await this.setSubtitle_();
    this.setItemTitle_();
  },

  /** @private */
  async setSubtitle_() {
    const mojom = chromeos.networkConfig.mojom;

    if (this.item.hasOwnProperty('customItemSubtitle') &&
        this.item.customItemSubtitle) {
      // Item is a custom OOBE network or pending eSIM profile.
      const item = /** @type {!NetworkList.CustomItemState} */ (this.item);
      this.subtitle_ = item.customItemSubtitle;
      return;
    }

    if (!this.isUpdatedCellularUiEnabled_) {
      return;
    }

    // Clear subtitle to ensure that stale values are not displayed when this
    // component is recycled for a case without subtitles.
    this.subtitle_ = '';

    // Show service provider subtext only when networkState is an eSIM cellular
    // network.
    if (!this.networkState ||
        this.networkState.type !== mojom.NetworkType.kCellular ||
        !this.networkState.typeState.cellular.eid ||
        !this.networkState.typeState.cellular.iccid) {
      return;
    }

    const properties = await cellular_setup.getESimProfileProperties(
        this.networkState.typeState.cellular.iccid);
    if (!properties) {
      return;
    }

    // The parent list component could recycle the same component to show
    // different networks. So networkState could have changed while the async
    // operations above were in progress. Skip updating subtitle if network
    // state does not match the fetched eSIM profile.
    if (!this.networkState || !this.networkState.typeState.cellular ||
        this.networkState.typeState.cellular.iccid !== properties.iccid) {
      return;
    }

    // Service provider from mojo API is a string16 value represented as an
    // array of characters. Convert to string for display.
    this.subtitle_ = properties.serviceProvider.data
                         .map((charCode) => String.fromCharCode(charCode))
                         .join('');
  },

  /** @private */
  networkStateChanged_() {
    if (!this.networkState) {
      this.managedProperties_ = undefined;
      return;
    }

    // network-list-item supports dummy networkStates that may have an empty
    // guid, such as those set by network-select. Only fetch managedProperties_
    // if the network's guid is defined.
    if (this.networkState.guid) {
      this.networkConfig_.getManagedProperties(this.networkState.guid)
          .then((response) => {
            this.managedProperties_ = response.result;
          });
    } else {
      this.managedProperties_ = undefined;
    }

    const connectionState = this.networkState.connectionState;
    if (connectionState === this.connectionState_) {
      return;
    }
    this.connectionState_ = connectionState;
    this.fire('network-connect-changed', this.networkState);
  },

  /** @private */
  setItemTitle_() {
    const itemName = this.getItemName_();
    const subtitle = this.getSubtitle();
    if (!subtitle) {
      this.itemTitle_ = itemName;
      return;
    }
    this.itemTitle_ = this.i18n('networkListItemTitle', itemName, subtitle);
  },

  /**
   * This gets called for network items and custom items.
   * @return {string}
   * @private
   */
  getItemName_() {
    if (this.item.hasOwnProperty('customItemName')) {
      const item = /** @type {!NetworkList.CustomItemState} */ (this.item);
      return this.i18nExists(item.customItemName) ?
          this.i18n(item.customItemName) :
          item.customItemName;
    }
    return OncMojo.getNetworkStateDisplayName(
        /** @type {!OncMojo.NetworkStateProperties} */ (this.item));
  },

  /**
   * The aria label for the subpage button.
   * @return {string}
   * @private
   */
  getButtonLabel_() {
    return this.i18n('networkListItemSubpageButtonLabel', this.getItemName_());
  },

  /**
   * @return {boolean}
   * @private
   */
  computeDisabled_() {
    if (!this.deviceState || !this.isUpdatedCellularUiEnabled_) {
      return false;
    }
    return OncMojo.deviceIsInhibited(this.deviceState);
  },

  /**
   * Label for the row, used for accessibility announcement.
   * @return {string}
   * @private
   */
  getRowLabel_() {
    if (!this.item) {
      return '';
    }

    const NetworkType = chromeos.networkConfig.mojom.NetworkType;
    const OncSource = chromeos.networkConfig.mojom.OncSource;
    const SecurityType = chromeos.networkConfig.mojom.SecurityType;
    const status = this.getNetworkStateText_();
    const isManaged = this.item.source === OncSource.kDevicePolicy ||
        this.item.source === OncSource.kUserPolicy;

    // TODO(jonmann): Reaching into the parent element breaks encapsulation so
    // refactor this logic into the parent (NetworkList) and pass into
    // NetworkListItem as a property.
    let index;
    let total;
    if (this.parentElement.items) {
      index = this.parentElement.items.indexOf(this.item) + 1;
      total = this.parentElement.items.length;
    } else {
      // This should only happen in tests; see TODO above.
      index = 0;
      total = 1;
    }

    switch (this.item.type) {
      case NetworkType.kCellular:
        if (isManaged) {
          if (status) {
            if (this.subtitle_) {
              return this.i18n(
                  'networkListItemLabelCellularManagedWithConnectionStatusAndProviderName',
                  index, total, this.getItemName_(), this.subtitle_, status,
                  this.item.typeState.cellular.signalStrength);
            }
            return this.i18n(
                'networkListItemLabelCellularManagedWithConnectionStatus',
                index, total, this.getItemName_(), status,
                this.item.typeState.cellular.signalStrength);
          }
          if (this.subtitle_) {
            return this.i18n(
                'networkListItemLabelCellularManagedWithProviderName', index,
                total, this.getItemName_(), this.subtitle_,
                this.item.typeState.cellular.signalStrength);
          }
          return this.i18n(
              'networkListItemLabelCellularManaged', index, total,
              this.getItemName_(), this.item.typeState.cellular.signalStrength);
        }
        if (status) {
          if (this.isPSimPendingActivationNetwork_) {
            return this.i18n(
                'networkListItemLabelCellularUnactivatedWithConnectionStatus',
                index, total, this.getItemName_(), status,
                this.item.typeState.cellular.signalStrength);
          }
          if (this.subtitle_) {
            return this.i18n(
                'networkListItemLabelCellularWithConnectionStatusAndProviderName',
                index, total, this.getItemName_(), this.subtitle_, status,
                this.item.typeState.cellular.signalStrength);
          }
          return this.i18n(
              'networkListItemLabelCellularWithConnectionStatus', index, total,
              this.getItemName_(), status,
              this.item.typeState.cellular.signalStrength);
        }

        if (this.isPSimPendingActivationNetwork_) {
          return this.i18n(
              'networkListItemLabelCellularUnactivated', index, total,
              this.getItemName_(), this.item.typeState.cellular.signalStrength);
        }

        if (this.subtitle_) {
          return this.i18n(
              'networkListItemLabelCellularWithProviderName', index, total,
              this.getItemName_(), this.subtitle_,
              this.item.typeState.cellular.signalStrength);
        }
        return this.i18n(
            'networkListItemLabelCellular', index, total, this.getItemName_(),
            this.item.typeState.cellular.signalStrength);
      case NetworkType.kEthernet:
        if (isManaged) {
          if (status) {
            return this.i18n(
                'networkListItemLabelCellularManagedWithConnectionStatus',
                index, total, this.getItemName_(), status);
          }
          return this.i18n(
              'networkListItemLabelEthernetManaged', index, total,
              this.getItemName_());
        }
        if (status) {
          return this.i18n(
              'networkListItemLabelEthernetWithConnectionStatus', index, total,
              this.getItemName_(), status);
        }
        return this.i18n(
            'networkListItemLabel', index, total, this.getItemName_());
      case NetworkType.kTether:
        // Tether networks will never be controlled by policy (only disabled).
        if (status) {
          if (this.subtitle_) {
            return this.i18n(
                'networkListItemLabelTetherWithConnectionStatusAndProviderName',
                index, total, this.getItemName_(), this.subtitle_, status,
                this.item.typeState.tether.signalStrength,
                this.item.typeState.tether.batteryPercentage);
          }
          return this.i18n(
              'networkListItemLabelTetherWithConnectionStatus', index, total,
              this.getItemName_(), status,
              this.item.typeState.tether.signalStrength,
              this.item.typeState.tether.batteryPercentage);
        }
        if (this.subtitle_) {
          return this.i18n(
              'networkListItemLabelTetherWithProviderName', index, total,
              this.getItemName_(), this.subtitle_,
              this.item.typeState.tether.signalStrength,
              this.item.typeState.tether.batteryPercentage);
        }
        return this.i18n(
            'networkListItemLabelTether', index, total, this.getItemName_(),
            this.item.typeState.tether.signalStrength,
            this.item.typeState.tether.batteryPercentage);
      case NetworkType.kWiFi:
        const secured =
            this.item.typeState.wifi.security === SecurityType.kNone ?
            this.i18n('wifiNetworkStatusUnsecured') :
            this.i18n('wifiNetworkStatusSecured');
        if (isManaged) {
          if (status) {
            return this.i18n(
                'networkListItemLabelWifiManagedWithConnectionStatus', index,
                total, this.getItemName_(), secured, status,
                this.item.typeState.wifi.signalStrength);
          }
          return this.i18n(
              'networkListItemLabelWifiManaged', index, total,
              this.getItemName_(), secured,
              this.item.typeState.wifi.signalStrength);
        }
        if (status) {
          return this.i18n(
              'networkListItemLabelWifiWithConnectionStatus', index, total,
              this.getItemName_(), secured, status,
              this.item.typeState.wifi.signalStrength);
        }
        return this.i18n(
            'networkListItemLabelWifi', index, total, this.getItemName_(),
            secured, this.item.typeState.wifi.signalStrength);
      default:
        if (this.isESimPendingProfile_) {
          if (this.subtitle_) {
            return this.i18n(
                'networkListItemLabelESimPendingProfileWithProviderName', index,
                total, this.getItemName_(), this.subtitle_);
          }
          return this.i18n(
              'networkListItemLabelESimPendingProfile', index, total,
              this.getItemName_());
        } else if (this.isESimInstallingProfile_) {
          if (this.subtitle_) {
            return this.i18n(
                'networkListItemLabelESimPendingProfileWithProviderNameInstalling',
                index, total, this.getItemName_(), this.subtitle_);
          }
          return this.i18n(
              'networkListItemLabelESimPendingProfileInstalling', index, total,
              this.getItemName_());
        }
        return this.i18n(
            'networkListItemLabel', index, total, this.getItemName_());
    }
  },

  /**
   * @return {boolean}
   * @private
   */
  isStateTextVisible_() {
    return !!this.networkState && !!this.getNetworkStateText_();
  },

  /**
   * This only gets called for network items once networkState is set.
   * @return {string}
   * @private
   */
  getNetworkStateText_() {
    const mojom = chromeos.networkConfig.mojom;
    if (!this.networkState) {
      return '';
    }

    if (this.networkState.type === mojom.NetworkType.kCellular) {
      if (this.networkState.typeState.cellular.simLocked) {
        return this.isUpdatedCellularUiEnabled_ ?
            this.i18n('networkListItemUpdatedCellularSimCardLocked') :
            this.i18n('networkListItemSimCardLocked');
      }
      if (!this.isUpdatedCellularUiEnabled_ &&
          this.shouldShowNotAvailableText_()) {
        return this.i18n('networkListItemNotAvailable');
      }
      if (!this.isUpdatedCellularUiEnabled_ &&
          this.isCellularNetworkScanning_()) {
        return this.i18n('networkListItemScanning');
      }
      if (this.isPSimUnavailableNetwork_) {
        return this.i18n('networkListItemUnavailableSimNetwork');
      }
    }

    const connectionState = this.networkState.connectionState;
    if (OncMojo.connectionStateIsConnected(connectionState)) {
      // TODO(khorimoto): Consider differentiating between Portal, Connected,
      // and Online.
      return this.i18n('networkListItemConnected');
    }
    if (connectionState === mojom.ConnectionStateType.kConnecting) {
      return this.i18n('networkListItemConnecting');
    }
    return '';
  },

  /**
   * @return {string}
   * @private
   */
  getNetworkStateTextClass_() {
    const mojom = chromeos.networkConfig.mojom;
    if (this.networkState &&
        this.networkState.type === mojom.NetworkType.kCellular &&
        this.networkState.typeState.cellular.simLocked &&
        this.isUpdatedCellularUiEnabled_) {
      return 'warning';
    }
    if (this.isPSimUnavailableNetwork_) {
      return 'warning';
    }
    return 'cr-secondary-text';
  },

  /**
   * @return {string}
   * @private
   */
  getSubtitle() {
    return this.subtitle_ ? this.subtitle_ : '';
  },

  /**
   * @param {!OncMojo.NetworkStateProperties|undefined} networkState
   * @param {boolean} showButtons
   * @return {boolean}
   * @private
   */
  isSubpageButtonVisible_(networkState, showButtons, disabled_) {
    if (!this.showButtons) {
      return false;
    }
    if (this.isPSimPendingActivationNetwork_ || this.isPSimActivatingNetwork_) {
      return true;
    }
    return !!networkState && !disabled_ && !this.shouldShowUnlockButton_();
  },

  /**
   * @return {boolean} Whether this element's contents describe an "active"
   *     network. In this case, an active network is connected and may have
   *     additional properties (e.g., must be activated for cellular networks).
   * @private
   */
  isStateTextActive_() {
    if (!this.networkState) {
      return false;
    }
    if (this.shouldShowNotAvailableText_()) {
      return false;
    }
    return OncMojo.connectionStateIsConnected(
        this.networkState.connectionState);
  },

  /**
   * @param {!KeyboardEvent} event
   * @private
   */
  onKeydown_(event) {
    if (event.key !== 'Enter' && event.key !== ' ') {
      return;
    }

    this.onSelected_(event);

    // The default event for pressing Enter on a focused button is to simulate a
    // click on the button. Prevent this action, since it would navigate a
    // second time to the details page and cause an unnecessary entry to be
    // added to the back stack. See https://crbug.com/736963.
    event.preventDefault();
  },

  /**
   * @param {!Event} event
   * @private
   */
  onSelected_(event) {
    if (this.disabled_) {
      event.stopImmediatePropagation();
      return;
    }
    if (this.isSubpageButtonVisible_(
            this.networkState, this.showButtons, this.disabled_) &&
        this.$$('#subpageButton') === this.shadowRoot.activeElement) {
      this.fireShowDetails_(event);
    } else if (this.shouldShowInstallButton_()) {
      this.onInstallButtonClick_(event);
    } else if (this.shouldShowUnlockButton_()) {
      this.onUnlockButtonClick_();
    } else if (this.item && this.item.hasOwnProperty('customItemName')) {
      this.fire('custom-item-selected', this.item);
    } else if (this.shouldShowActivateButton_()) {
      this.fireShowDetails_(event);
    } else if (
        this.showButtons &&
        (this.isPSimUnavailableNetwork_ || this.isPSimActivatingNetwork_)) {
      this.fireShowDetails_(event);
    } else {
      this.fire('selected', this.item);
      this.focusRequested_ = true;
    }
  },

  /**
   * @param {!MouseEvent} event
   * @private
   */
  onSubpageArrowClick_(event) {
    this.fireShowDetails_(event);
  },

  /**
   * Fires a 'show-details' event with |this.networkState| as the details.
   * @param {!Event} event
   * @private
   */
  fireShowDetails_(event) {
    assert(this.networkState);
    this.fire('show-detail', this.networkState);
    event.stopPropagation();
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowNotAvailableText_() {
    if (!this.networkState || !this.activationUnavailable) {
      return false;
    }

    // If cellular activation is not currently available and |this.networkState|
    // describes an unactivated cellular network, the text should be shown.
    const mojom = chromeos.networkConfig.mojom;
    return this.networkState.type === mojom.NetworkType.kCellular &&
        this.networkState.typeState.cellular.activationState !==
        mojom.ActivationStateType.kActivated;
  },

  /**
   * When the row is focused, this enables aria-live in "polite" mode to notify
   * a11y users when details about the network change or when the list gets
   * re-ordered because of changing signal strengths.
   * @param {boolean} isFocused
   * @return {string}
   * @private
   */
  getLiveStatus_(isFocused) {
    // isFocused is supplied by FocusRowBehavior.
    return this.isFocused ? 'polite' : 'off';
  },

  /**
   * @param {!Event} event
   * @private
   */
  onInstallButtonClick_(event) {
    if (this.disabled_) {
      return;
    }
    this.fire('install-profile', {iccid: this.item.customData.iccid});
    // Stop click from propagating to 'onSelected_()' and firing event twice.
    event.stopPropagation();
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsESimPendingProfile_() {
    return !!this.item && this.item.hasOwnProperty('customItemType') &&
        this.item.customItemType ===
        NetworkList.CustomItemType.ESIM_PENDING_PROFILE;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsESimInstallingProfile_() {
    return !!this.item && this.item.hasOwnProperty('customItemType') &&
        this.item.customItemType ===
        NetworkList.CustomItemType.ESIM_INSTALLING_PROFILE;
  },

  /**
   * @param {?chromeos.networkConfig.mojom.ManagedCellularProperties|undefined}
   *     cellularProperties
   * @return {boolean}
   * @private
   */
  isUnactivatedPSimNetwork_(cellularProperties) {
    if (!cellularProperties || cellularProperties.eid) {
      return false;
    }
    return cellularProperties.activationState ===
        chromeos.networkConfig.mojom.ActivationStateType.kNotActivated;
  },

  /**
   * @param {?chromeos.networkConfig.mojom.ManagedCellularProperties|undefined}
   *     cellularProperties
   * @return {boolean}
   * @private
   */
  hasPaymentPortalInfo_(cellularProperties) {
    if (!cellularProperties) {
      return false;
    }
    return !!(
        cellularProperties.paymentPortal &&
        cellularProperties.paymentPortal.url);
  },

  /**
   * @param {?chromeos.networkConfig.mojom.ManagedProperties|undefined}
   *     managedProperties
   * @return {boolean}
   * @private
   */
  computeIsPSimPendingActivationNetwork_(managedProperties) {
    if (!this.isUpdatedCellularUiEnabled_) {
      return false;
    }
    if (!managedProperties) {
      return false;
    }
    const cellularProperties = managedProperties.typeProperties.cellular;
    return this.isUnactivatedPSimNetwork_(cellularProperties) &&
        this.hasPaymentPortalInfo_(cellularProperties);
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowActivateButton_() {
    if (!this.showButtons) {
      return false;
    }
    return this.isPSimPendingActivationNetwork_;
  },

  /**
   * @return {string}
   * @private
   */
  getActivateBtnA11yLabel_() {
    return this.i18n('networkListItemActivateA11yLabel', this.getItemName_());
  },

  /**
   * @param {!Event} event
   * @private
   */
  onActivateButtonClick_(event) {
    this.fire(
        'show-cellular-setup',
        {pageName: cellularSetup.CellularSetupPageName.PSIM_FLOW_UI});
    event.stopPropagation();
  },

  /**
   * @param {?chromeos.networkConfig.mojom.ManagedProperties|undefined}
   *     managedProperties
   * @return {boolean}
   * @private
   */
  computeIsPSimUnavailableNetwork_(managedProperties) {
    if (!this.isUpdatedCellularUiEnabled_) {
      return false;
    }
    if (!managedProperties) {
      return false;
    }
    const cellularProperties = managedProperties.typeProperties.cellular;
    return this.isUnactivatedPSimNetwork_(cellularProperties) &&
        !this.hasPaymentPortalInfo_(cellularProperties);
  },

  /**
   * @return {boolean}
   * @private
   */
  computeIsPSimActivatingNetwork_() {
    if (!this.isUpdatedCellularUiEnabled_) {
      return false;
    }
    if (!this.networkState || !this.networkState.typeState.cellular ||
        this.networkState.typeState.cellular.eid) {
      return false;
    }
    return this.networkState.typeState.cellular.activationState ===
        chromeos.networkConfig.mojom.ActivationStateType.kActivating;
  },

  /**
   * @return {boolean}
   * @private
   */
  isCellularNetworkScanning_() {
    if (!this.deviceState || !this.deviceState.scanning) {
      return false;
    }

    const iccid = this.networkState && this.networkState.typeState.cellular &&
        this.networkState.typeState.cellular.iccid;
    if (!iccid) {
      return false;
    }

    // Scanning state should be shown only for the active SIM.
    return this.deviceState.simInfos.some(simInfo => {
      return simInfo.iccid === iccid && simInfo.isPrimary;
    });
  },

  /** @private */
  onUnlockButtonClick_() {
    this.isCellularUnlockDialogOpen_ = true;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowUnlockButton_() {
    if (!this.showButtons) {
      return false;
    }
    if (!this.networkState || !this.networkState.typeState.cellular ||
        !this.isUpdatedCellularUiEnabled_) {
      return false;
    }
    return this.networkState.typeState.cellular.simLocked;
  },

  /**
   * @return {string}
   * @private
   */
  getUnlockBtnA11yLabel_() {
    return this.i18n('networkListItemUnlockA11YLabel', this.getItemName_());
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowInstallButton_() {
    if (!this.showButtons) {
      return false;
    }
    return this.isESimPendingProfile_;
  },

  /**
   * @return {string}
   * @private
   */
  getInstallBtnA11yLabel_() {
    return this.i18n('networkListItemDownloadA11yLabel', this.getItemName_());
  },

  /**
   * @param {boolean} newValue
   * @param {boolean|undefined} oldValue
   * @private
   */
  disabledChanged_(newValue, oldValue) {
    if (!newValue && oldValue === undefined) {
      return;
    }
    if (this.disabled_) {
      this.blur();
    }
    this.setAttribute('aria-disabled', !!this.disabled_);
  },
});
