// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'add-smb-share-dialog' is a component for adding an SMB Share.
 *
 * This component can only be used once to add an SMB share, and must be
 * destroyed when finished, and re-created when shown again.
 */

/** @enum{number} */
const MountErrorType = {
  NO_ERROR: 0,
  CREDENTIAL_ERROR: 1,
  PATH_ERROR: 2,
  GENERAL_ERROR: 3,
};

/**
 * Regular expression that matches SMB share URLs of the form
 * smb://server/share or \\server\share. This is a coarse regexp intended for
 * quick UI feedback and does not reject all invalid URLs.
 *
 * @type {!RegExp}
 */
const SMB_SHARE_URL_REGEX =
    /^((smb:\/\/[^\/]+\/[^\/].*)|(\\\\[^\\]+\\[^\\].*))$/;

Polymer({
  is: 'add-smb-share-dialog',

  behaviors: [I18nBehavior, WebUIListenerBehavior],

  properties: {
    lastUrl: {
      type: String,
      value: '',
    },

    shouldOpenFileManagerAfterMount: {
      type: Boolean,
      value: false,
    },

    /** @private {string} */
    mountUrl_: {
      type: String,
      value: '',
      observer: 'onURLChanged_',
    },

    /** @private {string} */
    mountName_: {
      type: String,
      value: '',
    },

    /** @private {string} */
    username_: {
      type: String,
      value: '',
    },

    /** @private {string} */
    password_: {
      type: String,
      value: '',
    },
    /** @private {!Array<string>}*/
    discoveredShares_: {
      type: Array,
      value() {
        return [];
      },
    },

    /** @private */
    discoveryActive_: {
      type: Boolean,
      value: true,
    },

    /** @private */
    isActiveDirectory_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isActiveDirectoryUser');
      },
    },

    /** @private */
    isKerberosEnabled_: {
      type: Boolean,
      value() {
        return loadTimeData.getBoolean('isKerberosEnabled');
      },
    },

    /** @private */
    authenticationMethod_: {
      type: String,
      value() {
        return loadTimeData.getBoolean('isActiveDirectoryUser') ||
                loadTimeData.getBoolean('isKerberosEnabled') ?
            SmbAuthMethod.KERBEROS :
            SmbAuthMethod.CREDENTIALS;
      },
      observer: 'onAuthenticationMethodChanged_',
    },

    /** @private */
    generalErrorText_: String,

    /** @private */
    inProgress_: {
      type: Boolean,
      value: false,
    },

    /** @private {!MountErrorType} */
    currentMountError_: {
      type: Number,
      value: MountErrorType.NO_ERROR,
    },
  },

  /** @private {?smb_shares.SmbBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created() {
    this.browserProxy_ = smb_shares.SmbBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached() {
    this.browserProxy_.startDiscovery();
    this.$.dialog.showModal();

    this.addWebUIListener('on-shares-found', this.onSharesFound_.bind(this));
    this.mountUrl_ = this.lastUrl;
  },

  /** @private */
  cancel_() {
    this.$.dialog.cancel();
  },

  /** @private */
  onAddButtonTap_() {
    this.resetErrorState_();
    this.inProgress_ = true;
    this.browserProxy_
        .smbMount(
            this.mountUrl_, this.mountName_.trim(), this.username_,
            this.password_, this.authenticationMethod_,
            this.shouldOpenFileManagerAfterMount,
            this.$.saveCredentialsCheckbox.checked)
        .then(result => {
          this.onAddShare_(result);
        });
  },

  /**
   * @param {string} newValue
   * @param {string} oldValue
   * @private
   */
  onURLChanged_(newValue, oldValue) {
    this.resetErrorState_();
    const parts = this.mountUrl_.split('\\');
    this.mountName_ = parts[parts.length - 1];
  },

  /**
   * @param {string} newValue
   * @param {string} oldValue
   * @private
   */
  onAuthenticationMethodChanged_(newValue, oldValue) {
    this.resetErrorState_();
  },

  /**
   * @return {boolean}
   * @private
   */
  canAddShare_() {
    return !!this.mountUrl_ && !this.inProgress_ && this.isShareUrlValid_();
  },

  /**
   * @param {!Array<string>} newSharesDiscovered New shares that have been
   * discovered since the last call.
   * @param {boolean} done Whether share discovery has finished.
   * @private
   */
  onSharesFound_(newSharesDiscovered, done) {
    this.discoveredShares_ = this.discoveredShares_.concat(newSharesDiscovered);
    this.discoveryActive_ = !done;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowCredentialUI_() {
    return this.authenticationMethod_ === SmbAuthMethod.CREDENTIALS;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowAuthenticationUI_() {
    return this.isActiveDirectory_ || this.isKerberosEnabled_;
  },

  /**
   * @param {SmbMountResult} result
   * @private
   */
  onAddShare_(result) {
    this.inProgress_ = false;

    // Success case. Close dialog.
    if (result === SmbMountResult.SUCCESS) {
      this.$.dialog.close();
      return;
    }

    switch (result) {
      // Credential Error
      case SmbMountResult.AUTHENTICATION_FAILED:
        if (this.authenticationMethod_ === SmbAuthMethod.KERBEROS) {
          this.setGeneralError_(
              loadTimeData.getString('smbShareAddedAuthFailedMessage'));
        } else {
          this.setCredentialError_(
              loadTimeData.getString('smbShareAddedAuthFailedMessage'));
        }
        break;

      // Path Errors
      case SmbMountResult.NOT_FOUND:
        this.setPathError_(
            loadTimeData.getString('smbShareAddedNotFoundMessage'));
        break;
      case SmbMountResult.INVALID_URL:
        this.setPathError_(
            loadTimeData.getString('smbShareAddedInvalidURLMessage'));
        break;
      case SmbMountResult.INVALID_SSO_URL:
        this.setPathError_(
            loadTimeData.getString('smbShareAddedInvalidSSOURLMessage'));
        break;

      // General Errors
      case SmbMountResult.UNSUPPORTED_DEVICE:
        this.setGeneralError_(
            loadTimeData.getString('smbShareAddedUnsupportedDeviceMessage'));
        break;
      case SmbMountResult.MOUNT_EXISTS:
        this.setGeneralError_(
            loadTimeData.getString('smbShareAddedMountExistsMessage'));
        break;
      case SmbMountResult.TOO_MANY_OPENED:
        this.setGeneralError_(
            loadTimeData.getString('smbShareAddedTooManyMountsMessage'));
        break;
      default:
        this.setGeneralError_(
            loadTimeData.getString('smbShareAddedErrorMessage'));
    }
  },

  /** @private */
  resetErrorState_() {
    this.currentMountError_ = MountErrorType.NO_ERROR;
    this.$.address.errorMessage = '';
    this.$.password.errorMessage = '';
    this.generalErrorText_ = '';
  },

  /**
   * @param {string} errorMessage
   * @private
   */
  setCredentialError_(errorMessage) {
    this.$.password.errorMessage = errorMessage;
    this.currentMountError_ = MountErrorType.CREDENTIAL_ERROR;
  },

  /**
   * @param {string} errorMessage
   * @private
   */
  setGeneralError_(errorMessage) {
    this.generalErrorText_ = errorMessage;
    this.currentMountError_ = MountErrorType.GENERAL_ERROR;
  },

  /**
   * @param {string} errorMessage
   * @private
   */
  setPathError_(errorMessage) {
    this.$.address.errorMessage = errorMessage;
    this.currentMountError_ = MountErrorType.PATH_ERROR;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowCredentialError_() {
    return this.currentMountError_ === MountErrorType.CREDENTIAL_ERROR;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowGeneralError_() {
    return this.currentMountError_ === MountErrorType.GENERAL_ERROR;
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowPathError_() {
    return this.currentMountError_ === MountErrorType.PATH_ERROR;
  },

  /**
   * @return {boolean}
   * @private
   */
  isShareUrlValid_() {
    if (!this.mountUrl_ || this.shouldShowPathError_()) {
      return false;
    }
    return SMB_SHARE_URL_REGEX.test(this.mountUrl_);
  },
});
