// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A helper object used from the "add SMB share" dialog to
 * interact with the browser. Used only on Chrome OS.
 */

/**
 *  @enum {number}
 *  These values must be kept in sync with the SmbMountResult enum in
 *  chrome/browser/chromeos/smb_client/smb_service.h.
 */
const SmbMountResult = {
  SUCCESS: 0,
  UNKNOWN_FAILURE: 1,
  AUTHENTICATION_FAILED: 2,
  NOT_FOUND: 3,
  UNSUPPORTED_DEVICE: 4,
  MOUNT_EXISTS: 5,
  INVALID_URL: 6,
};

/** @enum {string} */
const SmbAuthMethod = {
  KERBEROS: 'kerberos',
  CREDENTIALS: 'credentials',
};

cr.define('smb_shares', function() {
  /** @interface */
  class SmbBrowserProxy {
    /**
     * Attempts to mount an Smb filesystem with the provided url.
     * @param {string} smbUrl File Share URL.
     * @param {string} smbName Display name for the File Share.
     * @param {string} username
     * @param {string} password
     * @param {string} authMethod
     * @return {!Promise<SmbMountResult>}
     */
    smbMount(smbUrl, smbName, username, password, authMethod) {}

    /**
     * Starts the file share discovery process.
     */
    startDiscovery() {}
  }

  /** @implements {smb_shares.SmbBrowserProxy} */
  class SmbBrowserProxyImpl {
    /** @override */
    smbMount(smbUrl, smbName, username, password, authMethod) {
      return cr.sendWithPromise(
          'smbMount', smbUrl, smbName, username, password,
          authMethod == SmbAuthMethod.KERBEROS);
    }

    /** @override */
    startDiscovery() {
      chrome.send('startDiscovery');
    }
  }

  cr.addSingletonGetter(SmbBrowserProxyImpl);

  return {
    SmbBrowserProxy: SmbBrowserProxy,
    SmbBrowserProxyImpl: SmbBrowserProxyImpl,
  };

});
