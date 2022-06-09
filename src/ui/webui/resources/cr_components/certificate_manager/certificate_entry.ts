// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An element that represents an SSL certificate entry.
 */
import '../../cr_elements/cr_expand_button/cr_expand_button.m.js';
import '../../cr_elements/policy/cr_policy_indicator.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import './certificate_shared_css.js';
import './certificate_subentry.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrPolicyIndicatorType} from '../../cr_elements/policy/cr_policy_indicator_behavior.m.js';
import {I18nMixin} from '../../js/i18n_mixin.js';

import {CertificatesOrgGroup, CertificateType} from './certificates_browser_proxy.js';

const CertificateEntryElementBase = I18nMixin(PolymerElement);

class CertificateEntryElement extends CertificateEntryElementBase {
  static get is() {
    return 'certificate-entry';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      model: Object,
      certificateType: String,
    };
  }

  model: CertificatesOrgGroup;
  certificateType: CertificateType;

  /**
   * @return Whether the given index corresponds to the last sub-node.
   */
  private isLast_(index: number): boolean {
    return index === this.model.subnodes.length - 1;
  }

  private getPolicyIndicatorType_(): CrPolicyIndicatorType {
    return this.model.containsPolicyCerts ? CrPolicyIndicatorType.USER_POLICY :
                                            CrPolicyIndicatorType.NONE;
  }
}

customElements.define(CertificateEntryElement.is, CertificateEntryElement);
