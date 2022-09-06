// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/shared_style_css.m.js';
import '//resources/cr_elements/shared_vars_css.m.js';
import '//resources/cr_elements/cr_icons_css.m.js';
import '//resources/cr_elements/cr_input/cr_input.m.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import './nearby_page_template.js';

import {I18nBehavior} from '//resources/js/i18n_behavior.m.js';
import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {NearbyShareOnboardingFinalState, processOnePageOnboardingCancelledMetrics, processOnePageOnboardingCompleteMetrics, processOnePageOnboardingInitiatedMetrics, processOnePageOnboardingVisibilityButtonOnInitialPageClickedMetrics} from './nearby_metrics_logger.js';
import {getNearbyShareSettings} from './nearby_share_settings.js';
import {NearbySettings} from './nearby_share_settings_behavior.js';

/**
 * @fileoverview The 'nearby-onboarding-one-page' component handles the Nearby
 * Share onboarding flow. It is embedded in chrome://os-settings,
 * chrome://settings and as a standalone dialog via chrome://nearby.
 */

/**
 * @type {string}
 */
const ONE_PAGE_ONBOARDING_SPLASH_LIGHT_ICON =
    'nearby-images:nearby-onboarding-splash-light';

/**
 * @type {string}
 */
const ONE_PAGE_ONBOARDING_SPLASH_DARK_ICON =
    'nearby-images:nearby-onboarding-splash-dark';

Polymer({
  _template: html`{__html_template__}`,
  is: 'nearby-onboarding-one-page',

  behaviors: [I18nBehavior],

  properties: {
    /** @type {?NearbySettings} */
    settings: {
      type: Object,
    },

    /** @type {string} */
    errorMessage: {
      type: String,
      value: '',
    },

    /**
     * Whether the onboarding page is being rendered in dark mode.
     * @private {boolean}
     */
    isDarkModeActive_: {
      type: Boolean,
      value: false,
    },
  },

  listeners: {
    'next': 'onNext_',
    'close': 'onClose_',
    'keydown': 'onKeydown_',
    'view-enter-start': 'onViewEnterStart_',
  },

  /** @private */
  onNext_() {
    this.finishOnboarding_();
  },

  /** @private */
  onClose_() {
    processOnePageOnboardingCancelledMetrics(
        NearbyShareOnboardingFinalState.INITIAL_PAGE);
    this.fire('onboarding-cancelled');
  },

  /**
   * @param {!KeyboardEvent} e Event containing the key
   * @private
   */
  onKeydown_(e) {
    e.stopPropagation();
    if (e.key === 'Enter') {
      this.finishOnboarding_();
      e.preventDefault();
    }
  },

  /** @private */
  onViewEnterStart_() {
    this.$$('#deviceName').focus();
    processOnePageOnboardingInitiatedMetrics(new URL(document.URL));
  },

  /** @private */
  onDeviceNameInput_() {
    getNearbyShareSettings()
        .validateDeviceName(this.$.deviceName.value)
        .then((result) => {
          this.updateErrorMessage_(result.result);
        });
  },

  /** @private */
  finishOnboarding_() {
    getNearbyShareSettings()
        .setDeviceName(this.$.deviceName.value)
        .then((result) => {
          this.updateErrorMessage_(result.result);
          if (result.result ===
              nearbyShare.mojom.DeviceNameValidationResult.kValid) {
            /**
             * TODO(crbug.com/1265562): remove this line once the old onboarding
             * is deprecated and default visibility is changed in
             * nearby_share_prefs.cc:kNearbySharingBackgroundVisibilityName
             */
            this.set('settings.visibility', this.getDefaultVisibility_());
            this.set('settings.isOnboardingComplete', true);
            this.set('settings.enabled', true);
            processOnePageOnboardingCompleteMetrics(
                NearbyShareOnboardingFinalState.INITIAL_PAGE,
                this.getDefaultVisibility_());
            this.fire('onboarding-complete');
          }
        });
  },

  /**
   * @private
   *
   * Switch to visibility selection page when the button is clicked
   */
  switchToVisibilitySelectionView_() {
    /**
     * TODO(crbug.com/1265562): remove this line once the old onboarding is
     * deprecated and default visibility is changed in
     * nearby_share_prefs.cc:kNearbySharingBackgroundVisibilityName
     */
    this.set('settings.visibility', this.getDefaultVisibility_());
    processOnePageOnboardingVisibilityButtonOnInitialPageClickedMetrics();
    this.fire('change-page', {page: 'visibility'});
  },

  /**
   * @private
   *
   * @param {!nearbyShare.mojom.DeviceNameValidationResult} validationResult
   * The error status from validating the provided device name.
   */
  updateErrorMessage_(validationResult) {
    switch (validationResult) {
      case nearbyShare.mojom.DeviceNameValidationResult.kErrorEmpty:
        this.errorMessage = this.i18n('nearbyShareDeviceNameEmptyError');
        break;
      case nearbyShare.mojom.DeviceNameValidationResult.kErrorTooLong:
        this.errorMessage = this.i18n('nearbyShareDeviceNameTooLongError');
        break;
      case nearbyShare.mojom.DeviceNameValidationResult.kErrorNotValidUtf8:
        this.errorMessage =
            this.i18n('nearbyShareDeviceNameInvalidCharactersError');
        break;
      default:
        this.errorMessage = '';
        break;
    }
  },

  /**
   * @private
   *
   * @param {!string} errorMessage The error message.
   * @return {boolean} Whether or not the error message exists.
   */
  hasErrorMessage_(errorMessage) {
    return errorMessage !== '';
  },

  /**
   * @private
   *
   * Returns the icon based on Light/Dark mode.
   * @return {string}
   */
  getOnboardingSplashIcon_() {
    return this.isDarkModeActive_ ? ONE_PAGE_ONBOARDING_SPLASH_DARK_ICON :
                                    ONE_PAGE_ONBOARDING_SPLASH_LIGHT_ICON;
  },

  /**
   * @private
   *
   * Temporary workaround to set default visibility. Changing the
   * kNearbySharingBackgroundVisibilityName in nearby_share_prefs.cc results in
   * setting visibility selection to 'all contacts' in nearby_visibility_page in
   * existing onboarding workflow.
   *
   * @return {?nearbyShare.mojom.Visibility} default visibility
   *
   * TODO(crbug.com/1265562): remove this function once the old onboarding is
   * deprecated and default visibility is changed in
   * nearby_share_prefs.cc:kNearbySharingBackgroundVisibilityName
   */
  getDefaultVisibility_() {
    if (this.settings.visibility === nearbyShare.mojom.Visibility.kUnknown) {
      return nearbyShare.mojom.Visibility.kAllContacts;
    }
    return this.settings.visibility;
  },

  /**
   * @private
   *
   * @return {string} Text displayed on visibility selection button
   */
  getVisibilitySelectionButtonText_() {
    const visibility = this.getDefaultVisibility_();
    switch (visibility) {
      case nearbyShare.mojom.Visibility.kAllContacts:
        return this.i18n('nearbyShareContactVisibilityAll');
      case nearbyShare.mojom.Visibility.kSelectedContacts:
        return this.i18n('nearbyShareContactVisibilitySome');
      case nearbyShare.mojom.Visibility.kNoOne:
        return this.i18n('nearbyShareContactVisibilityNone');
      default:
        return this.i18n('nearbyShareContactVisibilityAll');
    }
  },

  /**
   * @private
   *
   * @return {string} Icon displayed on visibility selection button
   */
  getVisibilitySelectionButtonIcon_() {
    const visibility = this.getDefaultVisibility_();
    switch (visibility) {
      case nearbyShare.mojom.Visibility.kAllContacts:
        return 'contact-all';
      case nearbyShare.mojom.Visibility.kSelectedContacts:
        return 'contact-group';
      case nearbyShare.mojom.Visibility.kNoOne:
        return 'visibility-off';
      default:
        return 'contact-all';
    }
  },

  /**
   * @private
   *
   * @return {string} Help text displayed under visibility selection button
   *
   * TODO(crbug.com/1265562): Add strings for other modes and switch based on
   * default visibility selection
   */
  getVisibilitySelectionButtonHelpText_() {
    return this.i18n(
        'nearbyShareOnboardingPageDeviceVisibilityHelpAllContacts');
  },
});
