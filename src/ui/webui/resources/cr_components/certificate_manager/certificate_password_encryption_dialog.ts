// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A dialog prompting the user to encrypt a personal certificate
 * before it is exported to disk.
 */
import '../../cr_elements/cr_button/cr_button.m.js';
import '../../cr_elements/cr_dialog/cr_dialog.m.js';
import '../../cr_elements/cr_input/cr_input.m.js';
import '../../cr_elements/shared_vars_css.m.js';
import './certificate_shared.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrButtonElement} from '../../cr_elements/cr_button/cr_button.m.js';
import {CrDialogElement} from '../../cr_elements/cr_dialog/cr_dialog.m.js';
import {I18nMixin} from '../../js/i18n_mixin.js';

import {getTemplate} from './certificate_password_encryption_dialog.html.js';
import {CertificatesBrowserProxyImpl} from './certificates_browser_proxy.js';

export interface CertificatePasswordEncryptionDialogElement {
  $: {
    dialog: CrDialogElement,
    ok: CrButtonElement,
  };
}

const CertificatePasswordEncryptionDialogElementBase =
    I18nMixin(PolymerElement);

export class CertificatePasswordEncryptionDialogElement extends
    CertificatePasswordEncryptionDialogElementBase {
  static get is() {
    return 'certificate-password-encryption-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      password_: {
        type: String,
        value: '',
      },

      confirmPassword_: {
        type: String,
        value: '',
      },
    };
  }

  private password_: string;
  private confirmPassword_: string;

  override connectedCallback() {
    super.connectedCallback();
    this.$.dialog.showModal();
  }

  private onCancelTap_() {
    this.$.dialog.close();
  }

  private onOkTap_() {
    CertificatesBrowserProxyImpl.getInstance()
        .exportPersonalCertificatePasswordSelected(this.password_)
        .then(
            () => {
              this.$.dialog.close();
            },
            error => {
              if (error === null) {
                return;
              }
              this.$.dialog.close();
              this.dispatchEvent(new CustomEvent('certificates-error', {
                bubbles: true,
                composed: true,
                detail: {error: error, anchor: null},
              }));
            });
  }

  private validate_() {
    const isValid =
        this.password_ !== '' && this.password_ === this.confirmPassword_;
    this.$.ok.disabled = !isValid;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-password-encryption-dialog':
        CertificatePasswordEncryptionDialogElement;
  }
}

customElements.define(
    CertificatePasswordEncryptionDialogElement.is,
    CertificatePasswordEncryptionDialogElement);
