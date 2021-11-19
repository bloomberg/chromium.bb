// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './base_page.js';
import './icons.js';
import './shimless_rma_shared_css.js';
import './strings.m.js';
import 'chrome://resources/cr_components/chromeos/network/network_config.m.js';
import 'chrome://resources/cr_components/chromeos/network/network_list.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';

import {NetworkListenerBehavior, NetworkListenerBehaviorInterface} from 'chrome://resources/cr_components/chromeos/network/network_listener_behavior.m.js';
import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior, I18nBehaviorInterface} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getNetworkConfigService, getShimlessRmaService} from './mojo_interface_provider.js';
import {NetworkConfigServiceInterface, ShimlessRmaServiceInterface, StateResult} from './shimless_rma_types.js';

/**
 * @fileoverview
 * 'onboarding-network-page' is the page where the user can choose to join a
 * network.
 */


/**
 * @constructor
 * @extends {PolymerElement}
 * @implements {I18nBehaviorInterface}
 * @implements {NetworkListenerBehaviorInterface}
 */
const OnboardingNetworkPageBase =
    mixinBehaviors([I18nBehavior, NetworkListenerBehavior], PolymerElement);

/** @polymer */
export class OnboardingNetworkPage extends OnboardingNetworkPageBase {
  static get is() {
    return 'onboarding-network-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Array of available networks
       * @protected
       * @type {!Array<chromeos.networkConfig.mojom.NetworkStateProperties>}
       */
      networks_: {
        type: Array,
        value: [],
      },

      /**
       * Tracks whether network has configuration to be connected
       * @protected
       */
      enableConnect_: {
        type: Boolean,
      },

      /**
       * The type of network to be configured as a string. May be set initially
       * or updated by network-config.
       * @protected
       */
      networkType_: {
        type: String,
        value: '',
      },

      /**
       * The name of the network. May be set initially or updated by
       * network-config.
       * @protected
       */
      networkName_: {
        type: String,
        value: '',
      },

      /**
       * The GUID when an existing network is being configured. This will be
       * empty when configuring a new network.
       * @protected
       */
      guid_: {
        type: String,
        value: '',
      },

      /**
       * Set to true to show the 'connect' button instead of 'disconnect'.
       * @protected
       */
      networkShowConnect_: {
        type: Boolean,
        value: true,
      },

      /**
       * Set by network-config when a configuration error occurs.
       * @private
       */
      error_: {
        type: String,
        value: '',
      },
    };
  }

  constructor() {
    super();
    /** @private {ShimlessRmaServiceInterface} */
    this.shimlessRmaService_ = getShimlessRmaService();
    /** @private {?NetworkConfigServiceInterface} */
    this.networkConfig_ = getNetworkConfigService();
  }

  /** @override */
  ready() {
    super.ready();
    this.refreshNetworks();
    this.dispatchEvent(new CustomEvent(
        'disable-next-button',
        {bubbles: true, composed: true, detail: false},
        ));
  }

  /** CrosNetworkConfigObserver impl */
  onNetworkStateListChanged() {
    this.refreshNetworks();
  }

  refreshNetworks() {
    const networkFilter = {
      filter: chromeos.networkConfig.mojom.FilterType.kVisible,
      networkType: chromeos.networkConfig.mojom.NetworkType.kAll,
      limit: chromeos.networkConfig.mojom.NO_LIMIT,
    };

    this.networkConfig_.getNetworkStateList(networkFilter).then(res => {
      // Filter again since networkFilter above doesn't take two network types.
      this.networks_ = res.result.filter(
          (network) => [chromeos.networkConfig.mojom.NetworkType.kWiFi,
                        chromeos.networkConfig.mojom.NetworkType.kEthernet,
      ].includes(network.type));
    });
  }

  /**
   * Event triggered when a network list item is selected.
   * @param {!{target: HTMLElement, detail: !OncMojo.NetworkStateProperties}}
   *     event
   * @protected
   */
  onNetworkSelected_(event) {
    const networkState = event.detail;
    const type = networkState.type;
    const displayName = OncMojo.getNetworkStateDisplayName(networkState);

    if (!this.canAttemptConnection_(networkState)) {
      this.showConfig_(type, networkState.guid, displayName);
      return;
    }

    this.networkConfig_.startConnect(networkState.guid).then(response => {
      this.refreshNetworks();
      if (response.result ===
          chromeos.networkConfig.mojom.StartConnectResult.kUnknown) {
        console.error(
            'startConnect failed for: ' + networkState.guid +
            ' Error: ' + response.message);
        return;
      }
    });
  }

  /**
   * Determines whether or not it is possible to attempt a connection to the
   * provided network (e.g., whether it's possible to connect or configure the
   * network for connection).
   * @param {!OncMojo.NetworkStateProperties} state The network state.
   * @private
   */
  canAttemptConnection_(state) {
    if (state.connectionState !==
        chromeos.networkConfig.mojom.ConnectionStateType.kNotConnected) {
      return false;
    }

    if (OncMojo.networkTypeHasConfigurationFlow(state.type) &&
        (!OncMojo.isNetworkConnectable(state) || !!state.errorState)) {
      return false;
    }

    return true;
  }

  /**
   * @param {chromeos.networkConfig.mojom.NetworkType} type
   * @param {string} guid
   * @param {string} name
   * @private
   */
  showConfig_(type, guid, name) {
    assert(
        type !== chromeos.networkConfig.mojom.NetworkType.kCellular &&
        type !== chromeos.networkConfig.mojom.NetworkType.kTether);

    this.networkType_ = OncMojo.getNetworkTypeString(type);
    this.networkName_ = name || '';
    this.guid_ = guid || '';

    const networkConfig =
        /** @type {!NetworkConfigElement} */ (
            this.shadowRoot.querySelector('#networkConfig'));
    networkConfig.init();

    const dialog = /** @type {!CrDialogElement} */ (
        this.shadowRoot.querySelector('#dialog'));
    if (!dialog.open) {
      dialog.showModal();
    }
  }

  /** @protected */
  closeConfig_() {
    const dialog = /** @type {!CrDialogElement} */ (
        this.shadowRoot.querySelector('#dialog'));
    if (dialog.open) {
      dialog.close();
    }
  }

  /** @protected */
  connectNetwork_() {
    const networkConfig =
        /** @type {!NetworkConfigElement} */ (
            this.shadowRoot.querySelector('#networkConfig'));
    networkConfig.connect();
  }

  /**
   * @return {string}
   * @private
   */
  getError_() {
    if (this.i18nExists(this.error_)) {
      return this.i18n(this.error_);
    }
    return this.i18n('networkErrorUnknown');
  }

  /**
   * @protected
   */
  onPropertiesSet_() {
    this.refreshNetworks();
  }

  /** @private */
  onConfigClose_() {
    this.closeConfig_();
    this.refreshNetworks();
  }

  /**
   * @return {string}
   * @protected
   */
  getDialogTitle_() {
    if (this.networkName_ && !this.networkShowConnect_) {
      return this.networkName_;
      // TODO(joonbug): Move these strings to //ui and uncomment this.
      // return this.i18n('internetConfigName', this.networkName_);
    }
    const type = this.i18n('OncType' + this.networkType_);
    return type;
    // return this.i18n('internetJoinType', type);
  }

  /** @return {!Promise<StateResult>} */
  onNextButtonClick() {
    return this.shimlessRmaService_.networkSelectionComplete();
  }
}

customElements.define(OnboardingNetworkPage.is, OnboardingNetworkPage);
