// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An element that represents an SSL certificate entry.
 */
import 'chrome://resources/cr_elements/cr_expand_button/cr_expand_button.m.js';
import 'chrome://resources/cr_elements/policy/cr_policy_indicator.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import './certificate_shared_css.js';
import './certificate_subentry.js';

import {CrPolicyIndicatorType} from 'chrome://resources/cr_elements/policy/cr_policy_indicator_behavior.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CertificatesOrgGroup, CertificateType} from './certificates_browser_proxy.js';

Polymer({
  is: 'certificate-entry',

  _template: html`{__html_template__}`,

  behaviors: [I18nBehavior],

  properties: {
    /** @type {!CertificatesOrgGroup} */
    model: Object,

    /** @type {!CertificateType} */
    certificateType: String,
  },

  /**
   * @param {number} index
   * @return {boolean} Whether the given index corresponds to the last sub-node.
   * @private
   */
  isLast_(index) {
    return index === this.model.subnodes.length - 1;
  },

  getPolicyIndicatorType_() {
    return this.model.containsPolicyCerts ? CrPolicyIndicatorType.USER_POLICY :
                                            CrPolicyIndicatorType.NONE;
  },
});
