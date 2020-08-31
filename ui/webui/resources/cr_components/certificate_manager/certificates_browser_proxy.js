// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "Manage certificates" section
 * to interact with the browser.
 */

// clang-format off
import {addSingletonGetter, sendWithPromise} from 'chrome://resources/js/cr.m.js';
// clang-format on

/**
 * @typedef {{
 *   extractable: boolean,
 *   id: string,
 *   name: string,
 *   policy: boolean,
 *   webTrustAnchor: boolean,
 *   canBeDeleted: boolean,
 *   canBeEdited: boolean,
 *   untrusted: boolean,
 * }}
 * @see chrome/browser/ui/webui/settings/certificates_handler.cc
 */
export let CertificateSubnode;

/**
 * A data structure describing a certificate that is currently being imported,
 * therefore it has no ID yet, but it has a name. Used within JS only.
 * @typedef {{
 *   name: string,
 * }}
 */
export let NewCertificateSubNode;

/**
 * Top-level grouping node in a certificate list, representing an organization
 * and containing certs that belong to the organization in |subnodes|. If a
 * certificate does not have an organization name, it will be grouped under its
 * own CertificatesOrgGroup with |name| set to its display name.
 * @typedef {{
 *   id: string,
 *   name: string,
 *   containsPolicyCerts: boolean,
 *   subnodes: !Array<!CertificateSubnode>
 * }}
 * @see chrome/browser/ui/webui/settings/certificates_handler.cc
 */
export let CertificatesOrgGroup;

/**
 * @typedef {{
 *   ssl: boolean,
 *   email: boolean,
 *   objSign: boolean
 * }}
 */
export let CaTrustInfo;

/**
 * Generic error returned from C++ via a Promise reject callback.
 * @typedef {{
 *   title: string,
 *   description: string
 * }}
 * @see chrome/browser/ui/webui/settings/certificates_handler.cc
 */
export let CertificatesError;

/**
 * Enumeration of all possible certificate types.
 * @enum {string}
 */
export const CertificateType = {
  CA: 'ca',
  OTHER: 'other',
  PERSONAL: 'personal',
  SERVER: 'server',
};


/**
 * Error returned from C++ via a Promise reject callback, when some certificates
 * fail to be imported.
 * @typedef {{
 *   title: string,
 *   description: string,
 *   certificateErrors: !Array<{name: string, error: string}>
 * }}
 * @see chrome/browser/ui/webui/settings/certificates_handler.cc
 */
export let CertificatesImportError;

  /** @interface */
  export class CertificatesBrowserProxy {
    /**
     * Triggers 5 events in the following order
     * 1x 'client-import-allowed-changed' event.
     * 1x 'ca-import-allowed-changed' event.
     * 4x 'certificates-changed' event, one for each certificate category.
     */
    refreshCertificates() {}

    /** @param {string} id */
    viewCertificate(id) {}

    /** @param {string} id */
    exportCertificate(id) {}

    /**
     * @param {string} id
     * @return {!Promise} A promise resolved when the certificate has been
     *     deleted successfully or rejected with a CertificatesError.
     */
    deleteCertificate(id) {}

    /**
     * @param {string} id
     * @return {!Promise<!CaTrustInfo>}
     */
    getCaCertificateTrust(id) {}

    /**
     * @param {string} id
     * @param {boolean} ssl
     * @param {boolean} email
     * @param {boolean} objSign
     * @return {!Promise}
     */
    editCaCertificateTrust(id, ssl, email, objSign) {}

    cancelImportExportCertificate() {}

    /**
     * @param {string} id
     * @return {!Promise} A promise firing once the user has selected
     *     the export location. A prompt should be shown to asking for a
     *     password to use for encrypting the file. The password should be
     *     passed back via a call to
     *     exportPersonalCertificatePasswordSelected().
     */
    exportPersonalCertificate(id) {}

    /**
     * @param {string} password
     * @return {!Promise}
     */
    exportPersonalCertificatePasswordSelected(password) {}

    /**
     * @param {boolean} useHardwareBacked
     * @return {!Promise<boolean>} A promise firing once the user has selected
     *     the file to be imported. If true a password prompt should be shown to
     *     the user, and the password should be passed back via a call to
     *     importPersonalCertificatePasswordSelected().
     */
    importPersonalCertificate(useHardwareBacked) {}

    /**
     * @param {string} password
     * @return {!Promise}
     */
    importPersonalCertificatePasswordSelected(password) {}

    /**
     * @return {!Promise} A promise firing once the user has selected
     *     the file to be imported, or failing with CertificatesError.
     *     Upon success, a prompt should be shown to the user to specify the
     *     trust levels, and that information should be passed back via a call
     *     to importCaCertificateTrustSelected().
     */
    importCaCertificate() {}

    /**
     * @param {boolean} ssl
     * @param {boolean} email
     * @param {boolean} objSign
     * @return {!Promise} A promise firing once the trust level for the imported
     *     certificate has been successfully set. The promise is rejected if an
     *     error occurred with either a CertificatesError or
     *     CertificatesImportError.
     */
    importCaCertificateTrustSelected(ssl, email, objSign) {}

    /**
     * @return {!Promise} A promise firing once the certificate has been
     *     imported. The promise is rejected if an error occurred, with either
     *     a CertificatesError or CertificatesImportError.
     */
    importServerCertificate() {}
  }

  /**
   * @implements {CertificatesBrowserProxy}
   */
  export class CertificatesBrowserProxyImpl {
    /** @override */
    refreshCertificates() {
      chrome.send('refreshCertificates');
    }

    /** @override */
    viewCertificate(id) {
      chrome.send('viewCertificate', [id]);
    }

    /** @override */
    exportCertificate(id) {
      chrome.send('exportCertificate', [id]);
    }

    /** @override */
    deleteCertificate(id) {
      return sendWithPromise('deleteCertificate', id);
    }

    /** @override */
    exportPersonalCertificate(id) {
      return sendWithPromise('exportPersonalCertificate', id);
    }

    /** @override */
    exportPersonalCertificatePasswordSelected(password) {
      return sendWithPromise(
          'exportPersonalCertificatePasswordSelected', password);
    }

    /** @override */
    importPersonalCertificate(useHardwareBacked) {
      return sendWithPromise('importPersonalCertificate', useHardwareBacked);
    }

    /** @override */
    importPersonalCertificatePasswordSelected(password) {
      return sendWithPromise(
          'importPersonalCertificatePasswordSelected', password);
    }

    /** @override */
    getCaCertificateTrust(id) {
      return sendWithPromise('getCaCertificateTrust', id);
    }

    /** @override */
    editCaCertificateTrust(id, ssl, email, objSign) {
      return sendWithPromise(
          'editCaCertificateTrust', id, ssl, email, objSign);
    }

    /** @override */
    importCaCertificateTrustSelected(ssl, email, objSign) {
      return sendWithPromise(
          'importCaCertificateTrustSelected', ssl, email, objSign);
    }

    /** @override */
    cancelImportExportCertificate() {
      chrome.send('cancelImportExportCertificate');
    }

    /** @override */
    importCaCertificate() {
      return sendWithPromise('importCaCertificate');
    }

    /** @override */
    importServerCertificate() {
      return sendWithPromise('importServerCertificate');
    }
  }

  // The singleton instance_ is replaced with a test version of this wrapper
  // during testing.
  addSingletonGetter(CertificatesBrowserProxyImpl);

