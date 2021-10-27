// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-detailed-build-info' contains detailed build
 * information for ChromeOS.
 */

import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/policy/cr_policy_indicator.m.js';
import '//resources/cr_elements/policy/cr_tooltip_icon.m.js';
import '../../settings_shared_css.js';
import '//resources/cr_components/chromeos/localized_link/localized_link.js';
import './channel_switcher_dialog.js';
import './edit_hostname_dialog.js';

import {CrPolicyIndicatorType} from '//resources/cr_elements/policy/cr_policy_indicator_behavior.m.js';
import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {focusWithoutInk} from '//resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../../i18n_setup.js';
import {Route, RouteObserverBehavior, Router} from '../../router.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.m.js';
import {routes} from '../os_route.m.js';

import {AboutPageBrowserProxy, AboutPageBrowserProxyImpl, AboutPageUpdateInfo, BrowserChannel, browserChannelToI18nId, ChannelInfo, isTargetChannelMoreStable, RegulatoryInfo, TPMFirmwareUpdateStatusChangedEvent, UpdateStatus, UpdateStatusChangedEvent, VersionInfo} from './about_page_browser_proxy.js';
import {DeviceNameBrowserProxy, DeviceNameBrowserProxyImpl, DeviceNameMetadata} from './device_name_browser_proxy.js';
import {DeviceNameState} from './device_name_util.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-detailed-build-info',

  behaviors: [
    DeepLinkingBehavior,
    WebUIListenerBehavior,
    I18nBehavior,
    RouteObserverBehavior,
  ],

  properties: {
    /** @private {!VersionInfo} */
    versionInfo_: Object,

    /** @private {!ChannelInfo} */
    channelInfo_: Object,

    /** @private {!DeviceNameMetadata} */
    deviceNameMetadata_: Object,

    /** @private */
    currentlyOnChannelText_: String,

    /** @private */
    showChannelSwitcherDialog_: Boolean,

    /** @private */
    showEditHostnameDialog_: Boolean,

    /** @private */
    canChangeChannel_: Boolean,

    eolMessageWithMonthAndYear: {
      type: String,
      value: '',
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kChangeChromeChannel,
        chromeos.settings.mojom.Setting.kChangeDeviceName,
        chromeos.settings.mojom.Setting.kCopyDetailedBuildInfo,
      ]),
    },

    /** @private */
    shouldHideEolInfo_: {
      type: Boolean,
      computed: 'computeShouldHideEolInfo_(eolMessageWithMonthAndYear)',
    },

    /** @private */
    isHostnameSettingEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isHostnameSettingEnabled');
      },
      readOnly: true,
    },

    /**
     * Whether the browser/ChromeOS is managed by their organization
     * through enterprise policies.
     * @private
     */
    isManaged_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isManaged');
      },
      readOnly: true,
    },
  },

  /** @override */
  ready() {
    const browserProxy = AboutPageBrowserProxyImpl.getInstance();
    browserProxy.pageReady();

    browserProxy.getVersionInfo().then(versionInfo => {
      this.versionInfo_ = versionInfo;
    });

    this.updateChannelInfo_();

    if (this.isHostnameSettingEnabled_) {
      this.addWebUIListener(
          'settings.updateDeviceNameMetadata',
          (data) => this.updateDeviceNameMetadata_(data));
      DeviceNameBrowserProxyImpl.getInstance().notifyReadyForDeviceName();
    }
  },

  /**
   * @param {!Route} route
   * @param {!Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.DETAILED_BUILD_INFO) {
      return;
    }

    this.attemptDeepLink();
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShouldHideEolInfo_() {
    return this.isManaged_ || !this.eolMessageWithMonthAndYear;
  },

  /** @private */
  updateChannelInfo_() {
    const browserProxy = AboutPageBrowserProxyImpl.getInstance();

    // canChangeChannel() call is expected to be low-latency, so fetch this
    // value by itself to ensure UI consistency (see https://crbug.com/848750).
    browserProxy.canChangeChannel().then(canChangeChannel => {
      this.canChangeChannel_ = canChangeChannel;
    });

    // getChannelInfo() may have considerable latency due to updates. Fetch this
    // metadata as part of a separate request.
    browserProxy.getChannelInfo().then(info => {
      this.channelInfo_ = info;
      // Display the target channel for the 'Currently on' message.
      const browserChannel =
          this.i18n(browserChannelToI18nId(info.targetChannel, info.isLts));
      // TODO(crbug.com/1259245) On LTS we should already show "Currently on
      // long-term support channel", whereas for other channels we still say
      // "Currently on stable", without the word "channel". This will be changed
      // and made consistent with the abovementioned ticket and this if-else
      // will be refactored.
      if (info.isLts) {
        this.currentlyOnChannelText_ =
            this.i18n('aboutCurrentlyOnChannelInfo', browserChannel);
      } else {
        this.currentlyOnChannelText_ =
            this.i18n('aboutCurrentlyOnChannel', browserChannel);
      }
    });
  },

  /**
   * @param {!DeviceNameMetadata} data
   * @private
   */
  updateDeviceNameMetadata_(data) {
    this.deviceNameMetadata_ = data;
  },

  /**
   * @return {string}
   * @private
   */
  getDeviceNameText_() {
    if (!this.deviceNameMetadata_) {
      return '';
    }

    return this.deviceNameMetadata_.deviceName;
  },

  /**
   * @return {string}
   * @private
   */
  getDeviceNameEditButtonA11yDescription_() {
    if (!this.deviceNameMetadata_) {
      return '';
    }

    return this.i18n(
        'aboutDeviceNameEditBtnA11yDescription', this.getDeviceNameText_());
  },

  /**
   * @return {boolean}
   * @private
   */
  canEditDeviceName_() {
    if (!this.deviceNameMetadata_) {
      return false;
    }

    return this.deviceNameMetadata_.deviceNameState ===
        DeviceNameState.CAN_BE_MODIFIED;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowPolicyIndicator_() {
    return this.getDeviceNameIndicatorType_() !== CrPolicyIndicatorType.NONE;
  },

  /**
   * @return {string}
   * @private
   */
  getDeviceNameIndicatorType_() {
    if (!this.deviceNameMetadata_) {
      return CrPolicyIndicatorType.NONE;
    }

    if (this.deviceNameMetadata_.deviceNameState ===
        DeviceNameState.CANNOT_BE_MODIFIED_BECAUSE_OF_POLICIES) {
      return CrPolicyIndicatorType.DEVICE_POLICY;
    }

    if (this.deviceNameMetadata_.deviceNameState ===
        DeviceNameState.CANNOT_BE_MODIFIED_BECAUSE_NOT_DEVICE_OWNER) {
      return CrPolicyIndicatorType.OWNER;
    }

    return CrPolicyIndicatorType.NONE;
  },

  /**
   * @param {boolean} canChangeChannel
   * @return {string}
   * @private
   */
  getChangeChannelIndicatorSourceName_(canChangeChannel) {
    return loadTimeData.getBoolean('aboutEnterpriseManaged') ?
        '' :
        loadTimeData.getString('ownerEmail');
  },

  /**
   * @param {boolean} canChangeChannel
   * @return {CrPolicyIndicatorType}
   * @private
   */
  getChangeChannelIndicatorType_(canChangeChannel) {
    if (canChangeChannel) {
      return CrPolicyIndicatorType.NONE;
    }
    return loadTimeData.getBoolean('aboutEnterpriseManaged') ?
        CrPolicyIndicatorType.DEVICE_POLICY :
        CrPolicyIndicatorType.OWNER;
  },

  /**
   * @param {!Event} e
   * @private
   */
  onChangeChannelTap_(e) {
    e.preventDefault();
    this.showChannelSwitcherDialog_ = true;
  },

  /**
   * @param {!Event} e
   * @private
   */
  onEditHostnameTap_(e) {
    e.preventDefault();
    this.showEditHostnameDialog_ = true;
  },

  /**
   * @return {boolean}
   * @private
   */
  copyToClipBoardEnabled_: function() {
    return !!this.versionInfo_ && !!this.channelInfo_;
  },

  /** @private */
  onCopyBuildDetailsToClipBoardTap_: function() {
    const buildInfo = {
      'application_label': loadTimeData.getString('aboutBrowserVersion'),
      'platform': this.versionInfo_.osVersion,
      'aboutChannelLabel': this.channelInfo_.targetChannel,
      'firmware_version': this.versionInfo_.osFirmware,
      'aboutIsArcStatusTitle': loadTimeData.getBoolean('aboutIsArcEnabled'),
      'arc_label': this.versionInfo_.arcVersion,
      'isEnterpriseManagedTitle':
          loadTimeData.getBoolean('aboutEnterpriseManaged'),
      'aboutIsDeveloperModeTitle':
          loadTimeData.getBoolean('aboutIsDeveloperMode'),
    };

    const entries = [];
    for (const key in buildInfo) {
      entries.push(this.i18n(key) + ': ' + buildInfo[key]);
    }

    navigator.clipboard.writeText(entries.join('\n'));
  },

  /**
   * @param {!Event} e
   * @private
   */
  onVisitBuildDetailsPageTap_(e) {
    e.preventDefault();
    window.open('chrome://version');
  },

  /** @private */
  onChannelSwitcherDialogClosed_() {
    this.showChannelSwitcherDialog_ = false;
    focusWithoutInk(assert(this.$$('cr-button')));
    this.updateChannelInfo_();
  },

  /** @private */
  onEditHostnameDialogClosed_() {
    this.showEditHostnameDialog_ = false;
    focusWithoutInk(assert(this.$$('cr-button')));
  },
});
