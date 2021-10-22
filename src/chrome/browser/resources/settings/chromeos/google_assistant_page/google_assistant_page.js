// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The types of Hotword enable status without Dsp support.
 * @enum {number}
 */
export const DspHotwordState = {
  DEFAULT_ON: 0,
  ALWAYS_ON: 1,
  OFF: 2,
};

/**
 * Indicates user's activity control consent status.
 *
 * Note: This should be kept in sync with ConsentStatus in
 * chromeos/services/assistant/public/cpp/assistant_prefs.h
 * @enum {number}
 */
export const ConsentStatus = {
  // The status is unknown.
  kUnknown: 0,

  // The user accepted activity control access.
  kActivityControlAccepted: 1,

  // The user is not authorized to give consent.
  kUnauthorized: 2,

  // The user's consent information is not found. This is typically the case
  // when consent from the user has never been requested.
  kNotFound: 3,
};

/**
 * @fileoverview 'settings-google-assistant-page' is the settings page
 * containing Google Assistant settings.
 */
import {afterNextRender, Polymer, html, flush, Templatizer, TemplateInstanceBase} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import '//resources/cr_elements/md_select_css.m.js';
import '//resources/cr_elements/policy/cr_policy_pref_indicator.m.js';
import {sendWithPromise, removeWebUIListener, addWebUIListener, WebUIListener} from '//resources/js/cr.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import '../../controls/controlled_button.js';
import '../../controls/settings_toggle_button.js';
import '../../prefs/prefs.js';
import {PrefsBehavior} from '../prefs_behavior.js';
import '../../prefs/pref_util.js';
import {Router, Route, RouteObserverBehavior} from '../../router.js';
import '../../settings_shared_css.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.m.js';
import {recordSettingChange, recordSearch, setUserActionRecorderForTesting, recordPageFocus, recordPageBlur, recordClick, recordNavigation} from '../metrics_recorder.m.js';
import {routes} from '../os_route.m.js';
import {GoogleAssistantBrowserProxy, GoogleAssistantBrowserProxyImpl} from './google_assistant_browser_proxy.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-google-assistant-page',

  behaviors: [
    DeepLinkingBehavior, I18nBehavior, PrefsBehavior, RouteObserverBehavior,
    WebUIListenerBehavior
  ],

  properties: {
    /** @private */
    shouldShowVoiceMatchSettings_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    hotwordDspAvailable_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('hotwordDspAvailable');
      },
    },

    /** @private */
    hotwordDropdownList_: {
      type: Array,
      value() {
        return [
          {
            name: loadTimeData.getString(
                'googleAssistantEnableHotwordWithoutDspRecommended'),
            value: DspHotwordState.DEFAULT_ON
          },
          {
            name: loadTimeData.getString(
                'googleAssistantEnableHotwordWithoutDspAlwaysOn'),
            value: DspHotwordState.ALWAYS_ON
          },
          {
            name: loadTimeData.getString(
                'googleAssistantEnableHotwordWithoutDspOff'),
            value: DspHotwordState.OFF
          }
        ];
      },
    },

    /** @private */
    hotwordEnforced_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    hotwordEnforcedForChild_: {
      type: Boolean,
      value: false,
    },

    /** @private {DspHotwordState} */
    dspHotwordState_: Number,

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kAssistantOnOff,
        chromeos.settings.mojom.Setting.kAssistantRelatedInfo,
        chromeos.settings.mojom.Setting.kAssistantOkGoogle,
        chromeos.settings.mojom.Setting.kAssistantNotifications,
        chromeos.settings.mojom.Setting.kAssistantVoiceInput,
        chromeos.settings.mojom.Setting.kTrainAssistantVoiceModel,
      ]),
    },
  },

  observers: [
    'onPrefsChanged_(' +
        'prefs.settings.voice_interaction.hotword.enabled.value, ' +
        'prefs.settings.voice_interaction.hotword.always_on.value, ' +
        'prefs.settings.voice_interaction.activity_control.consent_status' +
        '.value, ' +
        'prefs.settings.assistant.disabled_by_policy.value)',
  ],

  /** @private {?GoogleAssistantBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = GoogleAssistantBrowserProxyImpl.getInstance();
  },

  /** @override */
  ready() {
    this.addWebUIListener('hotwordDeviceUpdated', (hasHotword) => {
      this.hotwordDspAvailable_ = hasHotword;
    });

    chrome.send('initializeGoogleAssistantPage');
  },

  /**
   * @param {!Route} route
   * @param {!Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.GOOGLE_ASSISTANT) {
      return;
    }

    this.attemptDeepLink();
  },

  /**
   * @param {boolean} toggleValue
   * @return {string}
   * @private
   */
  getAssistantOnOffLabel_(toggleValue) {
    return this.i18n(
        toggleValue ? 'searchGoogleAssistantOn' : 'searchGoogleAssistantOff');
  },

  /** @private */
  onGoogleAssistantSettingsTapped_() {
    this.browserProxy_.showGoogleAssistantSettings();
    recordSettingChange();
  },

  /** @private */
  onRetrainVoiceModelTapped_() {
    this.browserProxy_.retrainAssistantVoiceModel();
    recordSettingChange();
  },

  /** @private */
  onEnableHotwordChange_(event) {
    if (event.target.checked) {
      this.browserProxy_.syncVoiceModelStatus();
    }
  },

  /** @private */
  onDspHotwordStateChange_() {
    switch (Number(this.$$('#dsp-hotword-state').value)) {
      case DspHotwordState.DEFAULT_ON:
        this.setPrefValue('settings.voice_interaction.hotword.enabled', true);
        this.setPrefValue(
            'settings.voice_interaction.hotword.always_on', false);
        this.browserProxy_.syncVoiceModelStatus();
        break;
      case DspHotwordState.ALWAYS_ON:
        this.setPrefValue('settings.voice_interaction.hotword.enabled', true);
        this.setPrefValue('settings.voice_interaction.hotword.always_on', true);
        this.browserProxy_.syncVoiceModelStatus();
        break;
      case DspHotwordState.OFF:
        this.setPrefValue('settings.voice_interaction.hotword.enabled', false);
        this.setPrefValue(
            'settings.voice_interaction.hotword.always_on', false);
        break;
      default:
        console.error('Invalid Dsp hotword settings state');
    }
  },

  /**
   * @param {number} state
   * @return {boolean}
   * @private
   */
  isDspHotwordStateMatch_(state) {
    return state === this.dspHotwordState_;
  },

  /** @private */
  onPrefsChanged_() {
    if (this.getPref('settings.assistant.disabled_by_policy').value) {
      this.setPrefValue('settings.voice_interaction.enabled', false);
      return;
    }

    this.refreshDspHotwordState_();

    this.shouldShowVoiceMatchSettings_ =
        !loadTimeData.getBoolean('voiceMatchDisabled') &&
        !!this.getPref('settings.voice_interaction.hotword.enabled').value;

    const hotwordEnabled =
        this.getPref('settings.voice_interaction.hotword.enabled');

    this.hotwordEnforced_ = hotwordEnabled.enforcement ===
        chrome.settingsPrivate.Enforcement.ENFORCED;

    this.hotwordEnforcedForChild_ = this.hotwordEnforced_ &&
        hotwordEnabled.controlledBy ===
            chrome.settingsPrivate.ControlledBy.CHILD_RESTRICTION;
  },

  /** @private */
  refreshDspHotwordState_() {
    if (!this.getPref('settings.voice_interaction.hotword.enabled').value) {
      this.dspHotwordState_ = DspHotwordState.OFF;
    } else if (this.getPref('settings.voice_interaction.hotword.always_on')
                   .value) {
      this.dspHotwordState_ = DspHotwordState.ALWAYS_ON;
    } else {
      this.dspHotwordState_ = DspHotwordState.DEFAULT_ON;
    }

    if (this.$$('#dsp-hotword-state')) {
      this.$$('#dsp-hotword-state').value = this.dspHotwordState_;
    }
  },
});
