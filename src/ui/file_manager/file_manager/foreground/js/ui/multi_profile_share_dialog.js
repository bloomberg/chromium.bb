// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Dialog to confirm the share between profiles.
 *
 */
class MultiProfileShareDialog extends FileManagerDialogBase {
  /**
   * @param {HTMLElement} parentNode Node to be parent for this dialog.
   */
  constructor(parentNode) {
    super(parentNode);

    this.mailLabel_ = parentNode.ownerDocument.createElement('label');
    this.mailLabel_.className = 'mail-label';

    const canEdit = parentNode.ownerDocument.createElement('option');
    canEdit.textContent = str('DRIVE_SHARE_TYPE_CAN_EDIT');
    canEdit.value = MultiProfileShareDialog.Result.CAN_EDIT;

    const canComment = parentNode.ownerDocument.createElement('option');
    canComment.textContent = str('DRIVE_SHARE_TYPE_CAN_COMMENT');
    canComment.value = MultiProfileShareDialog.Result.CAN_COMMET;

    const canView = parentNode.ownerDocument.createElement('option');
    canView.textContent = str('DRIVE_SHARE_TYPE_CAN_VIEW');
    canView.value = MultiProfileShareDialog.Result.CAN_VIEW;

    this.shareTypeSelect_ = parentNode.ownerDocument.createElement('select');
    this.shareTypeSelect_.setAttribute('size', 1);
    this.shareTypeSelect_.appendChild(canEdit);
    this.shareTypeSelect_.appendChild(canComment);
    this.shareTypeSelect_.appendChild(canView);

    const shareLine = parentNode.ownerDocument.createElement('div');
    shareLine.className = 'share-line';
    shareLine.appendChild(this.mailLabel_);
    shareLine.appendChild(this.shareTypeSelect_);

    this.frame.insertBefore(shareLine, this.buttons);
    this.frame.id = 'multi-profile-share-dialog';

    this.currentProfileId_ = new Promise(callback => {
      chrome.fileManagerPrivate.getProfiles(
          (profiles, currentId, displayedId) => {
            callback(currentId);
          });
    });
  }

  /**
   * Shows the dialog.
   * @param {boolean} plural Whether to use message of plural or not.
   * @return {!Promise} Promise fulfilled with the result of dialog. If the
   *     dialog is already opened, it returns null.
   */
  showMultiProfileShareDialog(plural) {
    return this.currentProfileId_.then(currentProfileId => {
      return new Promise((fulfill, reject) => {
        this.shareTypeSelect_.selectedIndex = 0;
        this.mailLabel_.textContent = currentProfileId;
        const result = super.showOkCancelDialog(
            str(plural ? 'MULTI_PROFILE_SHARE_DIALOG_TITLE_PLURAL' :
                         'MULTI_PROFILE_SHARE_DIALOG_TITLE'),
            str(plural ? 'MULTI_PROFILE_SHARE_DIALOG_MESSAGE_PLURAL' :
                         'MULTI_PROFILE_SHARE_DIALOG_MESSAGE'),
            () => {
              fulfill(this.shareTypeSelect_.value);
            },
            () => {
              fulfill(MultiProfileShareDialog.Result.CANCEL);
            });
        if (!result) {
          reject(new Error('Another dialog has already shown.'));
        }
      });
    });
  }
}

/**
 * Result of the dialog box.
 * @enum {string}
 * @const
 */
MultiProfileShareDialog.Result = {
  CAN_EDIT: 'can_edit',
  CAN_COMMET: 'can_comment',
  CAN_VIEW: 'can_view',
  CANCEL: 'cancel'
};
Object.freeze(MultiProfileShareDialog.Result);
