// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'certificate-provisioning-list' is an element that displays a
 * list of certificate provisioning processes.
 */
import '../../cr_elements/shared_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import './certificate_provisioning_details_dialog.js';
import './certificate_provisioning_entry.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {focusWithoutInk} from '../../js/cr/ui/focus_without_ink.m.js';
import {I18nMixin} from '../../js/i18n_mixin.js';
import {WebUIListenerMixin} from '../../js/web_ui_listener_mixin.js';

import {CertificateProvisioningActionEventDetail, CertificateProvisioningViewDetailsActionEvent} from './certificate_manager_types.js';
import {CertificateProvisioningBrowserProxyImpl, CertificateProvisioningProcess} from './certificate_provisioning_browser_proxy.js';

const CertificateProvisioningListElementBase =
    WebUIListenerMixin(I18nMixin(PolymerElement));

export class CertificateProvisioningListElement extends
    CertificateProvisioningListElementBase {
  static get is() {
    return 'certificate-provisioning-list';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      provisioningProcesses_: {
        type: Array,
        value() {
          return [];
        }
      },

      /**
       * The model to be passed to certificate provisioning details dialog.
       */
      provisioningDetailsDialogModel_: Object,

      showProvisioningDetailsDialog_: Boolean,
    };
  }

  private provisioningProcesses_: Array<CertificateProvisioningProcess>;
  private provisioningDetailsDialogModel_: CertificateProvisioningProcess|null;
  private showProvisioningDetailsDialog_: boolean;
  private previousAnchor_: HTMLElement|null = null;

  /**
   * @param provisioningProcesses The list of certificate provisioning
   *     processes.
   * @return Whether |provisioningProcesses| contains at least one entry.
   */
  private hasCertificateProvisioningEntries_(
      provisioningProcesses: Array<CertificateProvisioningProcess>): boolean {
    return provisioningProcesses.length !== 0;
  }

  /**
   * @param certProvisioningProcesses The currently active certificate
   *     provisioning processes
   */
  private onCertificateProvisioningProcessesChanged_(
      certProvisioningProcesses: Array<CertificateProvisioningProcess>) {
    this.provisioningProcesses_ = certProvisioningProcesses;

    // If a cert provisioning process details dialog is being shown, update its
    // model.
    if (!this.provisioningDetailsDialogModel_) {
      return;
    }

    const certProfileId = this.provisioningDetailsDialogModel_.certProfileId;
    const newDialogModel = this.provisioningProcesses_.find((process) => {
      return process.certProfileId === certProfileId;
    });
    if (newDialogModel) {
      this.provisioningDetailsDialogModel_ = newDialogModel;
    } else {
      // Close cert provisioning process details dialog if the process is no
      // longer in the list eg. when process completed successfully.
      this.shadowRoot!.querySelector(
                          'certificate-provisioning-details-dialog')!.close();
    }
  }

  connectedCallback() {
    super.connectedCallback();
    this.addWebUIListener(
        'certificate-provisioning-processes-changed',
        this.onCertificateProvisioningProcessesChanged_.bind(this));
    CertificateProvisioningBrowserProxyImpl.getInstance()
        .refreshCertificateProvisioningProcesses();
  }

  ready() {
    super.ready();
    this.addEventListener(
        CertificateProvisioningViewDetailsActionEvent, event => {
          const detail = event.detail;
          this.provisioningDetailsDialogModel_ = detail.model;
          this.previousAnchor_ = detail.anchor;
          this.showProvisioningDetailsDialog_ = true;
          event.stopPropagation();
        });
  }

  private onDialogClose_() {
    this.showProvisioningDetailsDialog_ = false;
    focusWithoutInk(this.previousAnchor_!);
    this.previousAnchor_ = null;
  }
}

customElements.define(
    CertificateProvisioningListElement.is, CertificateProvisioningListElement);
