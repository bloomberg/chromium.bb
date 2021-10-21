// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The 'certificate-manager' component manages SSL certificates.
 */
import '../../cr_elements/cr_tabs/cr_tabs.js';
import '../../cr_elements/hidden_style_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-pages/iron-pages.js';
import './ca_trust_edit_dialog.js';
import './certificate_delete_confirmation_dialog.js';
import './certificate_list.js';
import './certificate_password_decryption_dialog.js';
import './certificate_password_encryption_dialog.js';
import './certificates_error_dialog.js';
// <if expr="chromeos">
import './certificate_provisioning_list.js';
// </if>

import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {focusWithoutInk} from '../../js/cr/ui/focus_without_ink.m.js';
import {I18nMixin} from '../../js/i18n_mixin.js';
import {loadTimeData} from '../../js/load_time_data.m.js';
import {WebUIListenerMixin} from '../../js/web_ui_listener_mixin.js';

import {CertificateAction, CertificateActionEvent, CertificatesErrorEventDetail} from './certificate_manager_types.js';
import {CertificatesBrowserProxyImpl, CertificatesError, CertificatesImportError, CertificatesOrgGroup, CertificateSubnode, CertificateType, NewCertificateSubNode} from './certificates_browser_proxy.js';

const CertificateManagerElementBase =
    WebUIListenerMixin(I18nMixin(PolymerElement));

export class CertificateManagerElement extends CertificateManagerElementBase {
  static get is() {
    return 'certificate-manager';
  }

  static get properties() {
    return {
      selected: {
        type: Number,
        value: 0,
      },

      personalCerts: {
        type: Array,
        value() {
          return [];
        },
      },

      serverCerts: {
        type: Array,
        value() {
          return [];
        },
      },

      caCerts: {
        type: Array,
        value() {
          return [];
        },
      },

      otherCerts: {
        type: Array,
        value() {
          return [];
        },
      },

      /**
       * Indicates if client certificate import is allowed
       * by Chrome OS specific policy ClientCertificateManagementAllowed.
       * Value exists only for Chrome OS.
       */
      clientImportAllowed: {
        type: Boolean,
        value: false,
      },

      /**
       * Indicates if CA certificate import is allowed
       * by Chrome OS specific policy CACertificateManagementAllowed.
       * Value exists only for Chrome OS.
       */
      caImportAllowed: {
        type: Boolean,
        value: false,
      },

      certificateTypeEnum_: {
        type: Object,
        value: CertificateType,
        readOnly: true,
      },

      showCaTrustEditDialog_: Boolean,
      showDeleteConfirmationDialog_: Boolean,
      showPasswordEncryptionDialog_: Boolean,
      showPasswordDecryptionDialog_: Boolean,
      showErrorDialog_: Boolean,

      /**
       * The model to be passed to dialogs that refer to a given certificate.
       */
      dialogModel_: Object,

      /**
       * The certificate type to be passed to dialogs that refer to a given
       * certificate.
       */
      dialogModelCertificateType_: String,

      /**
       * The model to be passed to the error dialog.
       */
      errorDialogModel_: Object,

      /**
       * The element to return focus to, when the currently shown dialog is
       * closed.
       */
      activeDialogAnchor_: Object,

      isKiosk_: {
        type: Boolean,
        value() {
          return loadTimeData.valueExists('isKiosk') &&
              loadTimeData.getBoolean('isKiosk');
        },
      },

      tabNames_: {
        type: Array,
        computed: 'computeTabNames_(isKiosk_)',
      },
    };
  }

  selected: number;
  personalCerts: Array<CertificatesOrgGroup>;
  serverCerts: Array<CertificatesOrgGroup>;
  caCerts: Array<CertificatesOrgGroup>;
  otherCerts: Array<CertificatesOrgGroup>;
  clientImportAllowed: boolean;
  caImportAllowed: boolean;
  private showCaTrustEditDialog_: boolean;
  private showDeleteConfirmationDialog_: boolean;
  private showPasswordEncryptionDialog_: boolean;
  private showPasswordDecryptionDialog_: boolean;
  private showErrorDialog_: boolean;
  private dialogModel_: CertificateSubnode|NewCertificateSubNode|null;
  private dialogModelCertificateType_: CertificateType|null;
  private errorDialogModel_: CertificatesError|CertificatesImportError|null;
  private activeDialogAnchor_: HTMLElement|null;
  private isKiosk_: boolean;


  connectedCallback() {
    super.connectedCallback();
    this.addWebUIListener('certificates-changed', this.set.bind(this));
    this.addWebUIListener(
        'client-import-allowed-changed',
        this.setClientImportAllowed.bind(this));
    this.addWebUIListener(
        'ca-import-allowed-changed', this.setCAImportAllowed.bind(this));
    CertificatesBrowserProxyImpl.getInstance().refreshCertificates();
  }

  private setClientImportAllowed(allowed: boolean) {
    this.clientImportAllowed = allowed;
  }

  private setCAImportAllowed(allowed: boolean) {
    this.caImportAllowed = allowed;
  }

  /**
   * @return Whether to show tab at |tabIndex|.
   */
  private isTabSelected_(selectedIndex: number, tabIndex: number): boolean {
    return selectedIndex === tabIndex;
  }

  ready() {
    super.ready();
    this.addEventListener(CertificateActionEvent, event => {
      this.dialogModel_ = event.detail.subnode;
      this.dialogModelCertificateType_ = event.detail.certificateType;

      if (event.detail.action === CertificateAction.IMPORT) {
        if (event.detail.certificateType === CertificateType.PERSONAL) {
          this.openDialog_(
              'certificate-password-decryption-dialog',
              'showPasswordDecryptionDialog_', event.detail.anchor);
        } else if (event.detail.certificateType === CertificateType.CA) {
          this.openDialog_(
              'ca-trust-edit-dialog', 'showCaTrustEditDialog_',
              event.detail.anchor);
        }
      } else {
        if (event.detail.action === CertificateAction.EDIT) {
          this.openDialog_(
              'ca-trust-edit-dialog', 'showCaTrustEditDialog_',
              event.detail.anchor);
        } else if (event.detail.action === CertificateAction.DELETE) {
          this.openDialog_(
              'certificate-delete-confirmation-dialog',
              'showDeleteConfirmationDialog_', event.detail.anchor);
        } else if (event.detail.action === CertificateAction.EXPORT_PERSONAL) {
          this.openDialog_(
              'certificate-password-encryption-dialog',
              'showPasswordEncryptionDialog_', event.detail.anchor);
        }
      }

      event.stopPropagation();
    });

    this.addEventListener('certificates-error', event => {
      const detail = event.detail;
      this.errorDialogModel_ = detail.error;
      this.openDialog_(
          'certificates-error-dialog', 'showErrorDialog_', detail.anchor);
      event.stopPropagation();
    });
  }

  /**
   * Opens a dialog and registers a listener for removing the dialog from the
   * DOM once is closed. The listener is destroyed when the dialog is removed
   * (because of 'restamp').
   *
   * @param dialogTagName The tag name of the dialog to be shown.
   * @param domIfBooleanName The name of the boolean variable
   *     corresponding to the dialog.
   * @param anchor The element to focus when the dialog is
   *     closed. If null, the previous anchor element should be reused. This
   *     happens when a 'certificates-error-dialog' is opened, which when closed
   *     should focus the anchor of the previous dialog (the one that generated
   *     the error).
   */
  private openDialog_(
      dialogTagName: string, domIfBooleanName: string,
      anchor: HTMLElement|null) {
    if (anchor) {
      this.activeDialogAnchor_ = anchor;
    }
    this.set(domIfBooleanName, true);
    window.setTimeout(() => {
      const dialog = this.shadowRoot!.querySelector(dialogTagName)!;
      dialog.addEventListener('close', () => {
        this.set(domIfBooleanName, false);
        focusWithoutInk(this.activeDialogAnchor_!);
      });
    }, 0);
  }

  private computeTabNames_(): Array<string> {
    return [
      loadTimeData.getString('certificateManagerYourCertificates'),
      ...(this.isKiosk_ ?
              [] :
              [
                loadTimeData.getString('certificateManagerServers'),
                loadTimeData.getString('certificateManagerAuthorities'),
              ]),
      loadTimeData.getString('certificateManagerOthers'),
    ];
  }

  static get template() {
    return html`{__html_template__}`;
  }
}

customElements.define(CertificateManagerElement.is, CertificateManagerElement);
