// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'ca-trust-edit-dialog' allows the user to:
 *  - specify the trust level of a certificate authority that is being
 *    imported.
 *  - edit the trust level of an already existing certificate authority.
 */
import '../../cr_elements/cr_button/cr_button.m.js';
import '../../cr_elements/cr_checkbox/cr_checkbox.m.js';
import '../../cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import './certificate_shared_css.js';

import {PaperSpinnerLiteElement} from 'chrome://resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrCheckboxElement} from '../../cr_elements/cr_checkbox/cr_checkbox.m.js';
import {CrDialogElement} from '../../cr_elements/cr_dialog/cr_dialog.m.js';
import {I18nMixin} from '../../js/i18n_mixin.js';
import {loadTimeData} from '../../js/load_time_data.m.js';

import {CaTrustInfo, CertificatesBrowserProxy, CertificatesBrowserProxyImpl, CertificateSubnode, NewCertificateSubNode} from './certificates_browser_proxy.js';

export interface CaTrustEditDialogElement {
  $: {
    dialog: CrDialogElement,
    ssl: CrCheckboxElement,
    email: CrCheckboxElement,
    objSign: CrCheckboxElement,
    spinner: PaperSpinnerLiteElement,
  };
}

const CaTrustEditDialogElementBase = I18nMixin(PolymerElement);

export class CaTrustEditDialogElement extends CaTrustEditDialogElementBase {
  static get is() {
    return 'ca-trust-edit-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      model: Object,
      trustInfo_: Object,
      explanationText_: String,
    };
  }

  model: CertificateSubnode|NewCertificateSubNode;
  private trustInfo_: CaTrustInfo|null;
  private explanationText_: string;
  private browserProxy_: CertificatesBrowserProxy|null = null;

  ready() {
    super.ready();
    this.browserProxy_ = CertificatesBrowserProxyImpl.getInstance();
  }

  connectedCallback() {
    super.connectedCallback();

    this.explanationText_ = loadTimeData.getStringF(
        'certificateManagerCaTrustEditDialogExplanation', this.model.name);

    // A non existing |model.id| indicates that a new certificate is being
    // imported, otherwise an existing certificate is being edited.
    if ((this.model as CertificateSubnode).id) {
      this.browserProxy_!
          .getCaCertificateTrust((this.model as CertificateSubnode).id)
          .then(trustInfo => {
            this.trustInfo_ = trustInfo;
            this.$.dialog.showModal();
          });
    } else {
      this.$.dialog.showModal();
    }
  }

  private onCancelTap_() {
    this.$.dialog.close();
  }

  private onOkTap_() {
    this.$.spinner.active = true;

    const whenDone = (this.model as CertificateSubnode).id ?
        this.browserProxy_!.editCaCertificateTrust(
            (this.model as CertificateSubnode).id, this.$.ssl.checked,
            this.$.email.checked, this.$.objSign.checked) :
        this.browserProxy_!.importCaCertificateTrustSelected(
            this.$.ssl.checked, this.$.email.checked, this.$.objSign.checked);

    whenDone.then(
        () => {
          this.$.spinner.active = false;
          this.$.dialog.close();
        },
        error => {
          this.$.dialog.close();
          this.dispatchEvent(new CustomEvent('certificates-error', {
            bubbles: true,
            composed: true,
            detail: {error: error, anchor: null},
          }));
        });
  }
}

customElements.define(CaTrustEditDialogElement.is, CaTrustEditDialogElement);
