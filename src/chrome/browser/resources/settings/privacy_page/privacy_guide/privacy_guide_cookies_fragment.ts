// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'privacy-guide-cookies-fragment' is the fragment in a privacy
 * guide card that contains the cookie settings and their descriptions.
 */
import '../../prefs/prefs.js';
import './privacy_guide_description_item.js';
import './privacy_guide_fragment_shared_css.js';
import '../../controls/settings_radio_group.js';
import '../../privacy_page/collapse_radio_button.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {MetricsBrowserProxy, MetricsBrowserProxyImpl, PrivacyGuideSettingsStates} from '../../metrics_browser_proxy.js';
import {PrefsMixin} from '../../prefs/prefs_mixin.js';
import {CookiePrimarySetting} from '../../site_settings/site_settings_prefs_browser_proxy.js';

import {getTemplate} from './privacy_guide_cookies_fragment.html.js';

const PrivacyGuideCookiesFragmentBase = PrefsMixin(PolymerElement);

export class PrivacyGuideCookiesFragmentElement extends
    PrivacyGuideCookiesFragmentBase {
  static get is() {
    return 'privacy-guide-cookies-fragment';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Preferences state.
       */
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * Primary cookie control states for use in bindings.
       */
      cookiePrimarySettingEnum_: {
        type: Object,
        value: CookiePrimarySetting,
      },
    };
  }

  private metricsBrowserProxy_: MetricsBrowserProxy =
      MetricsBrowserProxyImpl.getInstance();
  private startStateBlock3PIncognito_: boolean;

  ready() {
    super.ready();
    this.addEventListener('view-enter-start', this.onViewEnterStart_);
    this.addEventListener('view-exit-finish', this.onViewExitFinish_);
  }

  private onViewEnterStart_() {
    this.startStateBlock3PIncognito_ =
        this.getPref('generated.cookie_primary_setting').value ===
        CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO;
  }

  private onViewExitFinish_() {
    const endStateBlock3PIncognito =
        this.getPref('generated.cookie_primary_setting').value ===
        CookiePrimarySetting.BLOCK_THIRD_PARTY_INCOGNITO;

    let state: PrivacyGuideSettingsStates|null = null;
    if (this.startStateBlock3PIncognito_) {
      state = endStateBlock3PIncognito ?
          PrivacyGuideSettingsStates.BLOCK_3P_INCOGNITO_TO_3P_INCOGNITO :
          PrivacyGuideSettingsStates.BLOCK_3P_INCOGNITO_TO_3P;
    } else {
      state = endStateBlock3PIncognito ?
          PrivacyGuideSettingsStates.BLOCK_3P_TO_3P_INCOGNITO :
          PrivacyGuideSettingsStates.BLOCK_3P_TO_3P;
    }
    this.metricsBrowserProxy_.recordPrivacyGuideSettingsStatesHistogram(state!);
  }

  private onCookies3PIncognitoClick_() {
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacyGuide.ChangeCookiesBlock3PIncognito');
  }

  private onCookies3PClick_() {
    this.metricsBrowserProxy_.recordAction(
        'Settings.PrivacyGuide.ChangeCookiesBlock3P');
  }
}

customElements.define(
    PrivacyGuideCookiesFragmentElement.is, PrivacyGuideCookiesFragmentElement);
