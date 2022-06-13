// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'certificate-list' is an element that displays a list of
 * certificates.
 */
import '../../cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import './certificate_entry.js';
import './certificate_shared_css.js';

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assertNotReached} from '../../js/assert.m.js';
import {I18nMixin} from '../../js/i18n_mixin.js';
import {loadTimeData} from '../../js/load_time_data.m.js';

import {CertificateAction, CertificateActionEvent, CertificateActionEventDetail} from './certificate_manager_types.js';
import {CertificatesBrowserProxy, CertificatesBrowserProxyImpl, CertificatesError, CertificatesImportError, CertificatesOrgGroup, CertificateType, NewCertificateSubNode} from './certificates_browser_proxy.js';

const CertificateListElementBase = I18nMixin(PolymerElement);

export class CertificateListElement extends CertificateListElementBase {
  static get is() {
    return 'certificate-list';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      certificates: {
        type: Array,
        value() {
          return [];
        },
      },

      certificateType: String,
      importAllowed: Boolean,

      // <if expr="chromeos or lacros">
      isGuest_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('isGuest') &&
              loadTimeData.getBoolean('isGuest');
        },
      },
      // </if>

      isKiosk_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('isKiosk') &&
              loadTimeData.getBoolean('isKiosk');
        },
      },
    };
  }

  certificates: Array<CertificatesOrgGroup>;
  certificateType: CertificateType;
  importAllowed: boolean;
  // <if expr="chromeos or lacros">
  private isGuest_: boolean;
  // </if>
  private isKiosk_: boolean;

  private getDescription_(): string {
    if (this.certificates.length === 0) {
      return this.i18n('certificateManagerNoCertificates');
    }

    switch (this.certificateType) {
      case CertificateType.PERSONAL:
        return this.i18n('certificateManagerYourCertificatesDescription');
      case CertificateType.SERVER:
        return this.i18n('certificateManagerServersDescription');
      case CertificateType.CA:
        return this.i18n('certificateManagerAuthoritiesDescription');
      case CertificateType.OTHER:
        return this.i18n('certificateManagerOthersDescription');
    }

    assertNotReached();
  }

  private canImport_(): boolean {
    return !this.isKiosk_ && this.certificateType !== CertificateType.OTHER &&
        this.importAllowed;
  }

  // <if expr="chromeos or lacros">
  private canImportAndBind_(): boolean {
    return !this.isGuest_ &&
        this.certificateType === CertificateType.PERSONAL && this.importAllowed;
  }
  // </if>

  /**
   * Handles a rejected Promise returned from |browserProxy_|.
   */
  private onRejected_(
      anchor: HTMLElement,
      error: CertificatesError|CertificatesImportError|null) {
    if (error === null) {
      // Nothing to do here. Null indicates that the user clicked "cancel" on
      // a native file chooser dialog.
      return;
    }

    // Otherwise propagate the error to the parents, such that a dialog
    // displaying the error will be shown.
    this.dispatchEvent(new CustomEvent('certificates-error', {
      bubbles: true,
      composed: true,
      detail: {error, anchor},
    }));
  }

  private dispatchImportActionEvent_(
      subnode: NewCertificateSubNode|null, anchor: HTMLElement) {
    this.dispatchEvent(new CustomEvent(CertificateActionEvent, {
      bubbles: true,
      composed: true,
      detail: {
        action: CertificateAction.IMPORT,
        subnode: subnode,
        certificateType: this.certificateType,
        anchor: anchor,
      },
    }));
  }

  private onImportTap_(e: Event) {
    this.handleImport_(false, e.target as HTMLElement);
  }

  // <if expr="chromeos or lacros">
  private onImportAndBindTap_(e: Event) {
    this.handleImport_(true, e.target as HTMLElement);
  }
  // </if>

  private handleImport_(useHardwareBacked: boolean, anchor: HTMLElement) {
    const browserProxy = CertificatesBrowserProxyImpl.getInstance();
    if (this.certificateType === CertificateType.PERSONAL) {
      browserProxy.importPersonalCertificate(useHardwareBacked)
          .then(showPasswordPrompt => {
            if (showPasswordPrompt) {
              this.dispatchImportActionEvent_(null, anchor);
            }
          }, this.onRejected_.bind(this, anchor));
    } else if (this.certificateType === CertificateType.CA) {
      browserProxy.importCaCertificate().then(certificateName => {
        this.dispatchImportActionEvent_({name: certificateName}, anchor);
      }, this.onRejected_.bind(this, anchor));
    } else if (this.certificateType === CertificateType.SERVER) {
      browserProxy.importServerCertificate().catch(
          this.onRejected_.bind(this, anchor));
    } else {
      assertNotReached();
    }
  }
}

customElements.define(CertificateListElement.is, CertificateListElement);
