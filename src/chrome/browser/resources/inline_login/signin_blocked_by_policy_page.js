// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview
 * 'signin-blocked-by-policy-page' handles signinBlockedByPolicy view from
 * `chrome/browser/resources/inline_login/inline_login_app.js`
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import './account_manager_shared_css.js';

import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/** @polymer */
class SigninBlockedByPolicyPageElement extends PolymerElement {
  static get is() {
    return 'signin-blocked-by-policy-page';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  /**
   * Gets body text with the respective user email and hosted domain for the
   * user that went through the sign-in flow.
   * @param {string} email User's email used in the sign-in flow.
   * @param {string} hostedDomain Hosted domain of the user's email used in the
   *     sign-in flow.
   * @return {string}
   * @private
   */
  getBodyText_(email, hostedDomain) {
    return loadTimeData.getStringF(
        'accountManagerDialogSigninBlockedByPolicyBody', email, hostedDomain);
  }
}

customElements.define(
    SigninBlockedByPolicyPageElement.is, SigninBlockedByPolicyPageElement);
