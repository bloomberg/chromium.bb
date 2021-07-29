// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'certificate-provisioning-details-dialog' allows the user to
 * view the details of an in-progress certiifcate provisioning process.
 */
import '../../cr_elements/cr_expand_button/cr_expand_button.m.js';
import '../../cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';

import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrDialogElement} from '../../cr_elements/cr_dialog/cr_dialog.m.js';
import {I18nBehavior} from '../../js/i18n_behavior.m.js';

import {CertificateProvisioningBrowserProxyImpl, CertificateProvisioningProcess} from './certificate_provisioning_browser_proxy.js';

export interface CertificateProvisioningDetailsDialogElement {
  $: {
    dialog: CrDialogElement,
  };
}

const CertificateProvisioningDetailsDialogElementBase =
    mixinBehaviors([I18nBehavior], PolymerElement) as
    {new (): PolymerElement & I18nBehavior};

export class CertificateProvisioningDetailsDialogElement extends
    CertificateProvisioningDetailsDialogElementBase {
  static get is() {
    return 'certificate-provisioning-details-dialog';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      model: Object,
      advancedExpanded_: Boolean,
    };
  }

  model: CertificateProvisioningProcess;
  private advancedExpanded_: boolean;

  close() {
    this.$.dialog.close();
  }

  private onRefresh_() {
    CertificateProvisioningBrowserProxyImpl.getInstance()
        .triggerCertificateProvisioningProcessUpdate(
            this.model.certProfileId, this.model.isDeviceWide);
  }

  private arrowState_(opened: boolean): string {
    return opened ? 'cr:arrow-drop-up' : 'cr:arrow-drop-down';
  }

  private boolToString_(bool: boolean): string {
    return bool.toString();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'certificate-provisioning-details-dialog':
        CertificateProvisioningDetailsDialogElement;
  }
}

customElements.define(
    CertificateProvisioningDetailsDialogElement.is,
    CertificateProvisioningDetailsDialogElement);
