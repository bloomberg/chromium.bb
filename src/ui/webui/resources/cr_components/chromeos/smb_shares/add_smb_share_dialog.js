// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'add-smb-share-dialog' is a component for adding an SMB Share.
 */

Polymer({
  is: 'add-smb-share-dialog',

  behaviors: [I18nBehavior, WebUIListenerBehavior],

  properties: {
    lastUrl: {
      type: String,
      value: '',
    },

    /** @private {string} */
    mountUrl_: {
      type: String,
      value: '',
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
      value: function() {
        return [];
      },
    },

    /** @private */
    isActiveDirectory_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('isActiveDirectoryUser');
      },
    },

    /** @private */
    authenticationMethod_: {
      type: String,
      value: function() {
        return loadTimeData.getBoolean('isActiveDirectoryUser') ?
            SmbAuthMethod.KERBEROS :
            SmbAuthMethod.CREDENTIALS;
      },
    },

    /** @private */
    addShareResultText_: String,

    /** @private */
    inProgress_: {
      type: Boolean,
      value: false,
    }
  },

  /** @private {?smb_shares.SmbBrowserProxy} */
  browserProxy_: null,

  /** @override */
  created: function() {
    this.browserProxy_ = smb_shares.SmbBrowserProxyImpl.getInstance();
  },

  /** @override */
  attached: function() {
    this.browserProxy_.startDiscovery();
    this.$.dialog.showModal();

    this.addWebUIListener('on-shares-found', this.onSharesFound_.bind(this));
    this.mountUrl_ = this.lastUrl;
  },

  /** @private */
  cancel_: function() {
    this.$.dialog.cancel();
  },

  /** @private */
  onAddButtonTap_: function() {
    this.inProgress_ = true;
    this.browserProxy_
        .smbMount(
            this.mountUrl_, this.mountName_.trim(), this.username_,
            this.password_, this.authenticationMethod_)
        .then(result => {
          this.onAddShare_(result);
        });
  },

  /** @private */
  onURLChanged_: function() {
    const parts = this.mountUrl_.split('\\');
    this.mountName_ = parts[parts.length - 1];
  },

  /**
   * @return {boolean}
   * @private
   */
  canAddShare_: function() {
    return !!this.mountUrl_ && !this.inProgress_;
  },

  /**
   * @param {!Array<string>} shares
   * @private
   */
  onSharesFound_: function(shares) {
    this.discoveredShares_ = this.discoveredShares_.concat(shares);
  },

  /**
   * @return {boolean}
   * @private
   */
  shouldShowCredentialUI_: function() {
    return this.authenticationMethod_ == SmbAuthMethod.CREDENTIALS;
  },

  /**
   * @param {SmbMountResult} result
   * @private
   */
  onAddShare_: function(result) {
    this.inProgress_ = false;
    switch (result) {
      case SmbMountResult.SUCCESS:
        this.$.dialog.close();
        break;
      case SmbMountResult.AUTHENTICATION_FAILED:
        this.addShareResultText_ =
            loadTimeData.getString('smbShareAddedAuthFailedMessage');
        break;
      case SmbMountResult.NOT_FOUND:
        this.addShareResultText_ =
            loadTimeData.getString('smbShareAddedNotFoundMessage');
        break;
      case SmbMountResult.UNSUPPORTED_DEVICE:
        this.addShareResultText_ =
            loadTimeData.getString('smbShareAddedUnsupportedDeviceMessage');
        break;
      case SmbMountResult.MOUNT_EXISTS:
        this.addShareResultText_ =
            loadTimeData.getString('smbShareAddedMountExistsMessage');
        break;
      case SmbMountResult.INVALID_URL:
        this.addShareResultText_ =
            loadTimeData.getString('smbShareAddedInvalidURLMessage');
        break;
      default:
        this.addShareResultText_ =
            loadTimeData.getString('smbShareAddedErrorMessage');
    }
    this.$.errorToast.show();
  },

});
