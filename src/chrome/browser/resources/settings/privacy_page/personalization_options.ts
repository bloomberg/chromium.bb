// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'personalization-options' contains several toggles related to
 * personalizations.
 */
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_toggle/cr_toggle.m.js';
import '../controls/settings_toggle_button.js';
import '../people_page/signout_dialog.js';
import '../prefs/prefs.js';
import '../settings_shared_css.js';
// <if expr="not chromeos">
import '//resources/cr_elements/cr_toast/cr_toast.js';
// </if>

import {CrToastElement} from '//resources/cr_elements/cr_toast/cr_toast.js';
import {WebUIListenerMixin} from '//resources/js/web_ui_listener_mixin.js';
import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {LifetimeBrowserProxyImpl} from '../lifetime_browser_proxy.js';
import {PrivacyPageVisibility} from '../page_visibility.js';
import {SettingsSignoutDialogElement} from '../people_page/signout_dialog.js';
import {StatusAction, SyncStatus} from '../people_page/sync_browser_proxy.js';
import {PrefsMixin} from '../prefs/prefs_mixin.js';

import {MetricsReporting, PrivacyPageBrowserProxy, PrivacyPageBrowserProxyImpl} from './privacy_page_browser_proxy.js';


export interface SettingsPersonalizationOptionsElement {
  $: {
    toast: CrToastElement,
    signinAllowedToggle: SettingsToggleButtonElement,
    metricsReportingControl: SettingsToggleButtonElement,
    spellCheckControl: SettingsToggleButtonElement,
  };
}

const SettingsPersonalizationOptionsElementBase =
    WebUIListenerMixin(PrefsMixin(PolymerElement));

export class SettingsPersonalizationOptionsElement extends
    SettingsPersonalizationOptionsElementBase {
  static get is() {
    return 'settings-personalization-options';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * TODO(dpapad): Restore actual type !PrivacyPageVisibility after this
       * file is no longer reused by chrome://os-settings. Dictionary defining
       * page visibility.
       */
      pageVisibility: Object,

      syncStatus: Object,

      // <if expr="_google_chrome and not chromeos">
      // TODO(dbeam): make a virtual.* pref namespace and set/get this normally
      // (but handled differently in C++).
      metricsReportingPref_: {
        type: Object,
        value() {
          // TODO(dbeam): this is basically only to appease PrefControlMixin.
          // Maybe add a no-validate attribute instead? This makes little sense.
          return {};
        },
      },

      showRestart_: Boolean,
      // </if>

      showSignoutDialog_: Boolean,

      syncFirstSetupInProgress_: {
        type: Boolean,
        value: false,
        computed: 'computeSyncFirstSetupInProgress_(syncStatus)',
      },

      // <if expr="not chromeos and not lacros">
      signinAvailable_: {
        type: Boolean,
        value: () => loadTimeData.getBoolean('signinAvailable'),
      },
      // </if>

    };
  }

  pageVisibility: PrivacyPageVisibility;
  syncStatus: SyncStatus;

  // <if expr="_google_chrome and not chromeos">
  private metricsReportingPref_: chrome.settingsPrivate.PrefObject;
  private showRestart_: boolean;
  // </if>

  private showSignoutDialog_: boolean;
  private syncFirstSetupInProgress_: boolean;

  // <if expr="not chromeos and not lacros">
  private signinAvailable_: boolean;
  // </if>

  private browserProxy_: PrivacyPageBrowserProxy =
      PrivacyPageBrowserProxyImpl.getInstance();

  private computeSyncFirstSetupInProgress_(): boolean {
    return !!this.syncStatus && !!this.syncStatus.firstSetupInProgress;
  }

  ready() {
    super.ready();

    // <if expr="_google_chrome and not chromeos">
    const setMetricsReportingPref = (metricsReporting: MetricsReporting) =>
        this.setMetricsReportingPref_(metricsReporting);
    this.addWebUIListener('metrics-reporting-change', setMetricsReportingPref);
    this.browserProxy_.getMetricsReporting().then(setMetricsReportingPref);
    // </if>
  }

  // <if expr="chromeos">
  /**
   * @return the autocomplete search suggestions CrToggleElement.
   */
  getSearchSuggestToggle(): SettingsToggleButtonElement|null {
    return this.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#searchSuggestToggle');
  }

  /**
   * @return the anonymized URL collection CrToggleElement.
   */
  getUrlCollectionToggle(): SettingsToggleButtonElement|null {
    return this.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#urlCollectionToggle');
  }

  /**
   * @return the Drive suggestions CrToggleElement.
   */
  getDriveSuggestToggle(): SettingsToggleButtonElement|null {
    return this.shadowRoot!.querySelector<SettingsToggleButtonElement>(
        '#driveSuggestControl');
  }
  // </if>

  // <if expr="_google_chrome and not chromeos">
  private onMetricsReportingChange_() {
    const enabled = this.$.metricsReportingControl.checked;
    this.browserProxy_.setMetricsReportingEnabled(enabled);
  }

  private setMetricsReportingPref_(metricsReporting: MetricsReporting) {
    const hadPreviousPref = this.metricsReportingPref_.value !== undefined;
    const pref: chrome.settingsPrivate.PrefObject = {
      key: '',
      type: chrome.settingsPrivate.PrefType.BOOLEAN,
      value: metricsReporting.enabled,
    };
    if (metricsReporting.managed) {
      pref.enforcement = chrome.settingsPrivate.Enforcement.ENFORCED;
      pref.controlledBy = chrome.settingsPrivate.ControlledBy.USER_POLICY;
    }

    // Ignore the next change because it will happen when we set the pref.
    this.metricsReportingPref_ = pref;

    // TODO(dbeam): remember whether metrics reporting was enabled when Chrome
    // started.
    if (metricsReporting.managed) {
      this.showRestart_ = false;
    } else if (hadPreviousPref) {
      this.showRestart_ = true;
    }
  }
  // </if>

  private showSearchSuggestToggle_(): boolean {
    // <if expr="chromeos">
    if (loadTimeData.getBoolean('syncSettingsCategorizationEnabled') &&
        loadTimeData.getBoolean('isOSSettings')) {
      // Should be hidden in OS settings.
      return false;
    }
    // </if>
    if (this.pageVisibility === undefined) {
      // pageVisibility isn't defined in non-Guest profiles (crbug.com/1288911).
      return true;
    }
    return this.pageVisibility.searchPrediction;
  }

  // <if expr="chromeos">
  private showMetricsReportingAsLink_(): boolean {
    // If SyncSettingsCategorization is enabled, browser settings should show
    // a link to the OS settings.
    return loadTimeData.getBoolean('syncSettingsCategorizationEnabled') &&
        !loadTimeData.getBoolean('isOSSettings');
  }

  private onMetricsReportingLinkClick_() {
    const chromeOSSyncSettingsPath =
        loadTimeData.getString('chromeOSSyncSettingsPath');
    window.location.href = `chrome://os-settings/${chromeOSSyncSettingsPath}`;
  }
  // </if>

  private showUrlCollectionToggle_(): boolean {
    // <if expr="chromeos">
    if (loadTimeData.getBoolean('syncSettingsCategorizationEnabled')) {
      // Should be hidden in OS settings.
      return !loadTimeData.getBoolean('isOSSettings');
    }
    // </if>
    return true;
  }

  // <if expr="_google_chrome">
  private onUseSpellingServiceToggle_(event: Event) {
    // If turning on using the spelling service, automatically turn on
    // spellcheck so that the spelling service can run.
    if ((event.target as SettingsToggleButtonElement).checked) {
      this.setPrefValue('browser.enable_spellchecking', true);
    }
  }
  // </if>

  private showSpellCheckControl_(): boolean {
    return (
        !!(this.prefs as {spellcheck?: any}).spellcheck &&
        (this.getPref('spellcheck.dictionaries').value as Array<string>)
                .length > 0);
  }

  private shouldShowDriveSuggest_(): boolean {
    // <if expr="chromeos">
    if (loadTimeData.getBoolean('syncSettingsCategorizationEnabled') &&
        loadTimeData.getBoolean('isOSSettings')) {
      // Should be hidden in OS settings.
      return false;
    }
    // </if>
    return loadTimeData.getBoolean('driveSuggestAvailable') &&
        !!this.syncStatus && !!this.syncStatus.signedIn &&
        this.syncStatus.statusAction !== StatusAction.REAUTHENTICATE;
  }

  private onSigninAllowedChange_() {
    if (this.syncStatus.signedIn && !this.$.signinAllowedToggle.checked) {
      // Switch the toggle back on and show the signout dialog.
      this.$.signinAllowedToggle.checked = true;
      this.showSignoutDialog_ = true;
    } else {
      this.$.signinAllowedToggle.sendPrefChange();
      this.$.toast.show();
    }
  }

  private onSignoutDialogClosed_() {
    if (this.shadowRoot!
            .querySelector<SettingsSignoutDialogElement>(
                'settings-signout-dialog')!.wasConfirmed()) {
      this.$.signinAllowedToggle.checked = false;
      this.$.signinAllowedToggle.sendPrefChange();
      this.$.toast.show();
    }
    this.showSignoutDialog_ = false;
  }

  private onRestartTap_(e: Event) {
    e.stopPropagation();
    LifetimeBrowserProxyImpl.getInstance().restart();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-personalization-options': SettingsPersonalizationOptionsElement;
  }
}

customElements.define(
    SettingsPersonalizationOptionsElement.is,
    SettingsPersonalizationOptionsElement);
