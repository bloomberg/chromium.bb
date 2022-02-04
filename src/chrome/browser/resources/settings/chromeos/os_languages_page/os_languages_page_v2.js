// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'os-settings-languages-page-v2' is the languages sub-page
 * for languages and inputs settings.
 */

/**
 * @type {number} Millisecond delay that can be used when closing an action
 * menu to keep it briefly on-screen so users can see the changes.
 */
const kMenuCloseDelay = 100;

import {afterNextRender, Polymer, html, flush, Templatizer, TemplateInstanceBase} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import '//resources/js/action_link.js';
import '//resources/cr_elements/action_link_css.m.js';
import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {sendWithPromise, removeWebUIListener, addWebUIListener, WebUIListener} from '//resources/js/cr.m.js';
import {focusWithoutInk} from '//resources/js/cr/ui/focus_without_ink.m.js';
import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import {CrActionMenuElement} from '//resources/cr_elements/cr_action_menu/cr_action_menu.js';
import '//resources/cr_elements/cr_lazy_render/cr_lazy_render.m.js';
import '//resources/cr_elements/cr_link_row/cr_link_row.js';
import './change_device_language_dialog.js';
import {LanguagesMetricsProxy, LanguagesMetricsProxyImpl, LanguagesPageInteraction, InputsShortcutReminderState} from './languages_metrics_proxy.js';
import './os_add_languages_dialog.js';
import '//resources/cr_components/chromeos/localized_link/localized_link.js';
import '../../controls/settings_toggle_button.js';
import {PrefsBehavior} from '../prefs_behavior.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.m.js';
import {routes} from '../os_route.m.js';
import {loadTimeData} from '../../i18n_setup.js';
import {Router, Route} from '../../router.js';
import {RouteObserverBehavior} from '../route_observer_behavior.js';
import '../../settings_shared_css.js';
import {recordSettingChange, recordSearch, setUserActionRecorderForTesting, recordPageFocus, recordPageBlur, recordClick, recordNavigation} from '../metrics_recorder.m.js';
import {LanguageHelper, LanguagesModel, LanguageState} from './languages_types.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'os-settings-languages-page-v2',

  behaviors: [
    DeepLinkingBehavior,
    I18nBehavior,
    PrefsBehavior,
    RouteObserverBehavior,
  ],

  properties: {
    /**
     * Preferences state.
     */
    prefs: {
      type: Object,
      notify: true,
    },

    /**
     * Read-only reference to the languages model provided by the
     * 'os-settings-languages' instance.
     * @type {!LanguagesModel|undefined}
     */
    languages: {
      type: Object,
      notify: true,
    },

    /** @type {!LanguageHelper} */
    languageHelper: Object,

    /**
     * The language to display the details for and its index.
     * @type {{state: !LanguageState, index: number}|undefined}
     * @private
     */
    detailLanguage_: Object,

    /** @private */
    showAddLanguagesDialog_: Boolean,

    /** @private */
    showChangeDeviceLanguageDialog_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    isGuest_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isGuest');
      },
    },

    /** @private */
    isSecondaryUser_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isSecondaryUser');
      },
    },

    /** @private */
    primaryUserEmail_: {
      type: String,
      value() {
        return loadTimeData.getString('primaryUserEmail');
      },
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kAddLanguage,
        chromeos.settings.mojom.Setting.kChangeDeviceLanguage,
        chromeos.settings.mojom.Setting.kOfferTranslation,
      ]),
    },

    /** @private */
    languageSettingsV2Update2Enabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('enableLanguageSettingsV2Update2');
      },
    },
  },

  /** @private {?LanguagesMetricsProxy} */
  languagesMetricsProxy_: null,

  /** @override */
  created() {
    this.languagesMetricsProxy_ = LanguagesMetricsProxyImpl.getInstance();
  },

  /**
   * @param {!Route} route
   * @param {!Route} oldRoute
   */
  currentRouteChanged(route, oldRoute) {
    // Does not apply to this page.
    if (route !== routes.OS_LANGUAGES_LANGUAGES) {
      return;
    }

    this.attemptDeepLink();
  },

  /**
   * @param {string} language
   * @return {string}
   * @private
   */
  getLanguageDisplayName_(language) {
    return this.languageHelper.getLanguage(language).displayName;
  },

  /** @private */
  onChangeDeviceLanguageClick_() {
    this.showChangeDeviceLanguageDialog_ = true;
  },

  /** @private */
  onChangeDeviceLanguageDialogClose_() {
    this.showChangeDeviceLanguageDialog_ = false;
    focusWithoutInk(assert(this.$$('#changeDeviceLanguage')));
  },

  /**
   * @param {string} language
   * @return {string}
   * @private
   */
  getChangeDeviceLanguageButtonDescription_(language) {
    return this.i18n(
        'changeDeviceLanguageButtonDescription',
        this.getLanguageDisplayName_(language));
  },

  /**
   * Stamps and opens the Add Languages dialog, registering a listener to
   * disable the dialog's dom-if again on close.
   * @param {!Event} e
   * @private
   */
  onAddLanguagesClick_(e) {
    e.preventDefault();
    this.languagesMetricsProxy_.recordAddLanguages();
    this.showAddLanguagesDialog_ = true;
  },

  /** @private */
  onAddLanguagesDialogClose_() {
    this.showAddLanguagesDialog_ = false;
    focusWithoutInk(assert(this.$.addLanguages));
  },

  /**
   * Checks if there are supported languages that are not enabled but can be
   * enabled.
   * @param {LanguagesModel|undefined} languages
   * @return {boolean} True if there is at least one available language.
   * @private
   */
  canEnableSomeSupportedLanguage_(languages) {
    return languages !== undefined && languages.supported.some(language => {
      return this.languageHelper.canEnableLanguage(language);
    });
  },

  /**
   * @return {boolean} True if the translate checkbox should be disabled.
   * @private
   */
  disableTranslateCheckbox_() {
    if (!this.detailLanguage_ || !this.detailLanguage_.state) {
      return true;
    }

    const languageState = this.detailLanguage_.state;
    if (!languageState.language || !languageState.language.supportsTranslate) {
      return true;
    }

    if (this.languageHelper.isOnlyTranslateBlockedLanguage(languageState)) {
      return true;
    }

    return this.languageHelper.convertLanguageCodeForTranslate(
               languageState.language.code) === this.languages.translateTarget;
  },

  /**
   * Handler for changes to the translate checkbox.
   * @param {!{target: !Element}} e
   * @private
   */
  onTranslateCheckboxChange_(e) {
    if (e.target.checked) {
      this.languageHelper.enableTranslateLanguage(
          this.detailLanguage_.state.language.code);
    } else {
      this.languageHelper.disableTranslateLanguage(
          this.detailLanguage_.state.language.code);
    }
    this.languagesMetricsProxy_.recordTranslateCheckboxChanged(
        e.target.checked);
    recordSettingChange();
    this.closeMenuSoon_();
  },

  /**
   * Closes the shared action menu after a short delay, so when a checkbox is
   * clicked it can be seen to change state before disappearing.
   * @private
   */
  closeMenuSoon_() {
    const menu = /** @type {!CrActionMenuElement} */ (this.$$('#menu').get());
    setTimeout(() => {
      if (menu.open) {
        menu.close();
      }
    }, kMenuCloseDelay);
  },

  /**
   * @return {boolean} True if the "Move to top" option for |language| should
   *     be visible.
   * @private
   */
  showMoveToTop_() {
    // "Move To Top" is a no-op for the top language.
    return this.detailLanguage_ !== undefined &&
        this.detailLanguage_.index === 0;
  },

  /**
   * @return {boolean} True if the "Move up" option for |language| should
   *     be visible.
   * @private
   */
  showMoveUp_() {
    // "Move up" is a no-op for the top language, and redundant with
    // "Move to top" for the 2nd language.
    return this.detailLanguage_ !== undefined &&
        this.detailLanguage_.index !== 0 && this.detailLanguage_.index !== 1;
  },

  /**
   * @return {boolean} True if the "Move down" option for |language| should be
   *     visible.
   * @private
   */
  showMoveDown_() {
    return this.languages !== undefined && this.detailLanguage_ !== undefined &&
        this.detailLanguage_.index !== this.languages.enabled.length - 1;
  },

  /**
   * Moves the language to the top of the list.
   * @private
   */
  onMoveToTopClick_() {
    /** @type {!CrActionMenuElement} */ (this.$.menu.get()).close();
    this.languageHelper.moveLanguageToFront(
        this.detailLanguage_.state.language.code);
    recordSettingChange();
  },

  /**
   * Moves the language up in the list.
   * @private
   */
  onMoveUpClick_() {
    /** @type {!CrActionMenuElement} */ (this.$.menu.get()).close();
    this.languageHelper.moveLanguage(
        this.detailLanguage_.state.language.code, /*upDirection=*/ true);
    recordSettingChange();
  },

  /**
   * Moves the language down in the list.
   * @private
   */
  onMoveDownClick_() {
    /** @type {!CrActionMenuElement} */ (this.$.menu.get()).close();
    this.languageHelper.moveLanguage(
        this.detailLanguage_.state.language.code, /*upDirection=*/ false);
    recordSettingChange();
  },

  /**
   * Disables the language.
   * @private
   */
  onRemoveLanguageClick_() {
    /** @type {!CrActionMenuElement} */ (this.$.menu.get()).close();
    this.languageHelper.disableLanguage(
        this.detailLanguage_.state.language.code);
    recordSettingChange();
  },

  /**
   * @param {!Event} e
   * @private
   */
  onDotsClick_(e) {
    // Sets a copy of the LanguageState object since it is not data-bound to
    // the languages model directly.
    this.detailLanguage_ =
        /** @type {{state: !LanguageState, index: number}} */ ({
          state: /** @type {!LanguageState} */ (e.model.item),
          index: /** @type {number} */ (e.model.index)
        });

    const menu = /** @type {!CrActionMenuElement} */ (this.$.menu.get());
    menu.showAt(/** @type {!Element} */ (e.target));
  },

  /**
   * @param {!Event} e
   * @private
   */
  onTranslateToggleChange_(e) {
    this.languagesMetricsProxy_.recordToggleTranslate(e.target.checked);
  },

  /**
   * @param {string} languageCode The language code identifying a language.
   * @param {string} translateTarget The translate target language.
   * @return {string} class name for whether it's a translate-target or not.
   * @private
   */
  getTranslationTargetClass_(languageCode, translateTarget) {
    return this.languageHelper.convertLanguageCodeForTranslate(languageCode) ===
            translateTarget ?
        'translate-target' :
        'non-translate-target';
  },

  /**
   * @param {boolean} update2Enabled
   * @return {string}
   * @private
   */
  getOfferTranslationLabel_(update2Enabled) {
    return this.i18n(
        update2Enabled ? 'offerGoogleTranslateLabel' : 'offerTranslationLabel');
  },

  /**
   * @param {boolean} update2Enabled
   * @return {string}
   * @private
   */
  getOfferTranslationSublabel_(update2Enabled) {
    return update2Enabled ? '' : this.i18n('offerTranslationSublabel');
  },

  /**
   * @param {boolean} update2Enabled
   * @return {string}
   * @private
   */
  getLanguagePreferenceTitle_(update2Enabled) {
    return this.i18n(
        update2Enabled ? 'websiteLanguagesTitle' : 'languagesPreferenceTitle');
  },

  /**
   * @param {boolean} update2Enabled
   * @return {string}
   * @private
   */
  getLanguagePreferenceDescription_(update2Enabled) {
    return this.i18nAdvanced(
        update2Enabled ? 'websiteLanguagesDescription' :
                         'languagesPreferenceDescription');
  },

  /** @private */
  openManageGoogleAccountLanguage_() {
    this.languagesMetricsProxy_.recordInteraction(
        LanguagesPageInteraction.OPEN_MANAGE_GOOGLE_ACCOUNT_LANGUAGE);
    window.open(loadTimeData.getString('googleAccountLanguagesURL'));
  },

  /** @private */
  onLanguagePreferenceDescriptionLinkClick_() {
    this.languagesMetricsProxy_.recordInteraction(
        LanguagesPageInteraction.OPEN_WEB_LANGUAGES_LEARN_MORE);
  },
});
