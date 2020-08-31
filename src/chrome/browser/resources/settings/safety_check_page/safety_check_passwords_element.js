// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'settings-safety-passwords-element' is the settings page containing the
 * safety check element showing the password status.
 */
import {assertNotReached} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {WebUIListenerBehavior} from 'chrome://resources/js/web_ui_listener_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PasswordManagerImpl, PasswordManagerProxy} from '../autofill_page/password_manager_proxy.js';
import {MetricsBrowserProxy, MetricsBrowserProxyImpl, SafetyCheckInteractions} from '../metrics_browser_proxy.js';
import {routes} from '../route.js';
import {Router} from '../router.m.js';

import {SafetyCheckCallbackConstants, SafetyCheckPasswordsStatus} from './safety_check_browser_proxy.js';
import {SafetyCheckIconStatus} from './safety_check_child.js';

/**
 * @typedef {{
 *   newState: SafetyCheckPasswordsStatus,
 *   displayString: string,
 * }}
 */
let PasswordsChangedEvent;

Polymer({
  is: 'settings-safety-check-passwords-element',

  _template: html`{__html_template__}`,

  behaviors: [
    I18nBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /**
     * Current state of the safety check passwords element.
     * @private {!SafetyCheckPasswordsStatus}
     */
    status_: {
      type: Number,
      value: SafetyCheckPasswordsStatus.CHECKING,
    },

    /** UI string to display for this child, received from the backend. */
    displayString_: String,
  },

  /** ?MetricsBrowserProxy */
  metricsBrowserProxy_: null,

  /** @override */
  attached: function() {
    this.metricsBrowserProxy_ = MetricsBrowserProxyImpl.getInstance();

    // Register for safety check status updates.
    this.addWebUIListener(
        SafetyCheckCallbackConstants.PASSWORDS_CHANGED,
        this.onSafetyCheckPasswordsChanged_.bind(this));
  },

  /**
   * @param {!PasswordsChangedEvent} event
   * @private
   */
  onSafetyCheckPasswordsChanged_: function(event) {
    this.status_ = event.newState;
    this.displayString_ = event.displayString;
  },

  /**
   * @return {SafetyCheckIconStatus}
   * @private
   */
  getIconStatus_: function() {
    switch (this.status_) {
      case SafetyCheckPasswordsStatus.CHECKING:
        return SafetyCheckIconStatus.RUNNING;
      case SafetyCheckPasswordsStatus.SAFE:
        return SafetyCheckIconStatus.SAFE;
      case SafetyCheckPasswordsStatus.COMPROMISED:
        return SafetyCheckIconStatus.WARNING;
      case SafetyCheckPasswordsStatus.OFFLINE:
      case SafetyCheckPasswordsStatus.NO_PASSWORDS:
      case SafetyCheckPasswordsStatus.SIGNED_OUT:
      case SafetyCheckPasswordsStatus.QUOTA_LIMIT:
      case SafetyCheckPasswordsStatus.ERROR:
        return SafetyCheckIconStatus.INFO;
      default:
        assertNotReached();
    }
  },

  /**
   * @private
   * @return {?string}
   */
  getButtonLabel_: function() {
    switch (this.status_) {
      case SafetyCheckPasswordsStatus.COMPROMISED:
        return this.i18n('safetyCheckReview');
      default:
        return null;
    }
  },

  /** @private */
  onButtonClick_: function() {
    // Log click both in action and histogram.
    this.metricsBrowserProxy_.recordSafetyCheckInteractionHistogram(
        SafetyCheckInteractions.SAFETY_CHECK_PASSWORDS_MANAGE);
    this.metricsBrowserProxy_.recordAction(
        'Settings.SafetyCheck.ManagePasswords');

    Router.getInstance().navigateTo(routes.CHECK_PASSWORDS);
    PasswordManagerImpl.getInstance().recordPasswordCheckReferrer(
        PasswordManagerProxy.PasswordCheckReferrer.SAFETY_CHECK,
        /* dynamicParams= */ null, /* removeSearch= */ true);
  },
});
