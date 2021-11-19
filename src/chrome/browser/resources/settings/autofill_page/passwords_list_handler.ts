// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordsListHandler is a container for a passwords list
 * responsible for handling events associated with the overflow menu (copy,
 * editing, removal, moving to account).
 */

import './password_edit_dialog.js';
import './password_move_to_account_dialog.js';
import './password_remove_dialog.js';
import './password_list_item.js';
import './password_edit_dialog.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {StoredAccount, SyncBrowserProxyImpl} from '../people_page/sync_browser_proxy.js';

// <if expr="chromeos or lacros">
import {BlockingRequestManager} from './blocking_request_manager.js';
// </if>
import {MultiStorePasswordUiEntry} from './multi_store_password_ui_entry.js';
import {PasswordListItemElement, PasswordMoreActionsClickedEvent} from './password_list_item.js';
import {PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';
import {PasswordRemoveDialogPasswordsRemovedEvent} from './password_remove_dialog.js';

declare global {
  interface HTMLElementEventMap {
    'password-more-actions-clicked': PasswordMoreActionsClickedEvent;
    'password-remove-dialog-passwords-removed':
        PasswordRemoveDialogPasswordsRemovedEvent;
  }
}

export interface PasswordsListHandlerElement {
  $: {
    menu: CrActionMenuElement,
    toast: CrToastElement,
  };
}

const PasswordsListHandlerElementBase =
    WebUIListenerMixin(I18nMixin(PolymerElement));

export class PasswordsListHandlerElement extends
    PasswordsListHandlerElementBase {
  static get is() {
    return 'passwords-list-handler';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * Saved passwords after deduplicating versions that are repeated in the
       * account and on the device.
       */
      savedPasswords: {
        type: Array,
        value: () => [],
      },

      /**
       * If true, the edit dialog and removal notification show
       * information about which location(s) a password is stored.
       */
      isAccountStoreUser: {
        type: Boolean,
        value: false,
      },

      /**
       * Whether an option for moving a password to the account should be
       * offered in the overflow menu.
       */
      allowMoveToAccountOption: {
        type: Boolean,
        value: false,
      },

      // <if expr="chromeos or lacros">
      tokenRequestManager: Object,
      // </if>

      /**
       * The model for any active menus or dialogs. The value is reset to null
       * whenever actions from the menus/dialogs are concluded.
       */
      activePassword_: {
        type: Object,
        value: null,
      },

      /**
       * Check if entry isn't federation credential.
       */
      isEditDialog_: {
        type: Boolean,
        computed: 'computeIsEditDialog_(activePassword_)',
      },

      showPasswordEditDialog_: {type: Boolean, value: false},

      showPasswordMoveToAccountDialog_: {type: Boolean, value: false},

      showPasswordRemoveDialog_: {type: Boolean, value: false},

      /**
       * The element to return focus to, when the currently active dialog is
       * closed.
       */
      activeDialogAnchor_: {type: Object, value: null},

      /**
       * The message displayed in the toast following a password removal.
       */
      removalNotification_: {
        type: String,
        value: '',
      },

      /**
       * The email of the first signed-in account, or the empty string if
       * there's none.
       */
      firstSignedInAccountEmail_: {
        type: String,
        value: '',
      },
    };
  }

  savedPasswords: Array<MultiStorePasswordUiEntry>;
  isAccountStoreUser: boolean;
  allowMoveToAccountOption: boolean;

  // <if expr="chromeos or lacros">
  tokenRequestManager: BlockingRequestManager;
  // </if>

  private activePassword_: PasswordListItemElement|null;
  private isEditDialog_: boolean;
  private showPasswordEditDialog_: boolean;
  private showPasswordMoveToAccountDialog_: boolean;
  private showPasswordRemoveDialog_: boolean;
  private activeDialogAnchor_: HTMLElement|null;
  private removalNotification_: string;
  private firstSignedInAccountEmail_: string;
  private passwordManager_: PasswordManagerProxy =
      PasswordManagerImpl.getInstance();

  ready() {
    super.ready();

    this.addEventListener(
        'password-more-actions-clicked',
        this.passwordMoreActionsClickedHandler_);
    this.addEventListener(
        'password-remove-dialog-passwords-removed',
        this.passwordRemoveDialogPasswordsRemovedHandler_);
  }

  connectedCallback() {
    super.connectedCallback();

    const extractFirstAccountEmail = (accounts: Array<StoredAccount>) => {
      this.firstSignedInAccountEmail_ =
          accounts.length > 0 ? accounts[0].email : '';
    };
    SyncBrowserProxyImpl.getInstance().getStoredAccounts().then(
        extractFirstAccountEmail);
    this.addWebUIListener('stored-accounts-updated', extractFirstAccountEmail);
  }

  disconnectedCallback() {
    super.disconnectedCallback();

    if (this.$.toast.open) {
      this.$.toast.hide();
    }
  }

  /**
   * Closes the toast manager.
   */
  onSavedPasswordOrExceptionRemoved() {
    this.$.toast.hide();
  }

  /**
   * Opens the password action menu.
   */
  private passwordMoreActionsClickedHandler_(
      event: PasswordMoreActionsClickedEvent) {
    const target = event.detail.target;

    this.activePassword_ = event.detail.listItem;
    this.$.menu.showAt(target);
    this.activeDialogAnchor_ = target;
  }

  private passwordRemoveDialogPasswordsRemovedHandler_(
      event: PasswordRemoveDialogPasswordsRemovedEvent) {
    this.displayRemovalNotification_(
        event.detail.removedFromAccount, event.detail.removedFromDevice);
  }

  /**
   * Helper function that checks if entry isn't federation credential.
   */
  private computeIsEditDialog_(): boolean {
    return !this.activePassword_ || !this.activePassword_.entry.federationText;
  }

  /**
   * Requests the plaintext password for the current active password.
   * @param reason The reason why the plaintext password is requested.
   * @param callback The callback that gets invoked with the retrieved password.
   */
  private requestActivePlaintextPassword_(
      reason: chrome.passwordsPrivate.PlaintextReason,
      callback: (password: string) => void) {
    this.passwordManager_
        .requestPlaintextPassword(
            this.activePassword_!.entry.getAnyId(), reason)
        .then(callback, _error => {
          // <if expr="chromeos or lacros">
          // If no password was found, refresh auth token and retry.
          this.tokenRequestManager.request(() => {
            this.requestActivePlaintextPassword_(reason, callback);
          });
          // </if>
        });
  }

  private onMenuEditPasswordTap_() {
    if (this.isEditDialog_) {
      this.requestActivePlaintextPassword_(
          chrome.passwordsPrivate.PlaintextReason.EDIT, password => {
            this.set('activePassword_.entry.password', password);
            this.showPasswordEditDialog_ = true;
          });
    } else {
      this.showPasswordEditDialog_ = true;
    }
    this.$.menu.close();
    this.activePassword_!.hide();
  }

  private getMenuEditPasswordName_(): string {
    return this.isEditDialog_ ? this.i18n('editPassword') :
                                this.i18n('passwordViewDetails');
  }

  private onPasswordEditDialogClosed_() {
    this.showPasswordEditDialog_ = false;
    focusWithoutInk(assert(this.activeDialogAnchor_!));
    this.activeDialogAnchor_ = null;
    this.activePassword_!.hide();
    this.activePassword_ = null;
  }

  private onMovePasswordToAccountDialogClosed_() {
    this.showPasswordEditDialog_ = false;
    focusWithoutInk(assert(this.activeDialogAnchor_!));
    this.activeDialogAnchor_ = null;
    this.activePassword_ = null;
  }

  /**
   * Copy selected password to clipboard.
   */
  private onMenuCopyPasswordButtonTap_() {
    // Copy to clipboard occurs inside C++ and we don't expect getting
    // result back to javascript.
    this.requestActivePlaintextPassword_(
        chrome.passwordsPrivate.PlaintextReason.COPY, _ => {
          this.activePassword_ = null;
        });

    this.$.menu.close();
  }

  /**
   * Handler for the remove option in the overflow menu. If the password only
   * exists in one location, deletes it directly. Otherwise, opens the remove
   * dialog to allow choosing from which locations to remove.
   */
  private onMenuRemovePasswordTap_() {
    this.$.menu.close();

    if (this.activePassword_!.entry.isPresentOnDevice() &&
        this.activePassword_!.entry.isPresentInAccount()) {
      this.showPasswordRemoveDialog_ = true;
      return;
    }

    const idToRemove = this.activePassword_!.entry.isPresentInAccount() ?
        this.activePassword_!.entry.accountId :
        this.activePassword_!.entry.deviceId;
    this.passwordManager_.removeSavedPassword(idToRemove!);
    this.displayRemovalNotification_(
        this.activePassword_!.entry.isPresentInAccount(),
        this.activePassword_!.entry.isPresentOnDevice());
    this.activePassword_ = null;
  }

  /**
   * At least one of |removedFromAccount| or |removedFromDevice| must be true.
   */
  private displayRemovalNotification_(
      removedFromAccount: boolean, removedFromDevice: boolean) {
    assert(removedFromAccount || removedFromDevice);
    this.removalNotification_ = this.i18n('passwordDeleted');
    if (this.isAccountStoreUser) {
      if (removedFromAccount && removedFromDevice) {
        this.removalNotification_ =
            this.i18n('passwordDeletedFromAccountAndDevice');
      } else if (removedFromAccount) {
        this.removalNotification_ = this.i18n('passwordDeletedFromAccount');
      } else {
        this.removalNotification_ = this.i18n('passwordDeletedFromDevice');
      }
    }
    this.$.toast.show();
  }

  private onUndoButtonClick_() {
    this.passwordManager_.undoRemoveSavedPasswordOrException();
    this.onSavedPasswordOrExceptionRemoved();
  }

  /**
   * Should only be called when |activePassword_| has a device copy.
   */
  private onMenuMovePasswordToAccountTap_() {
    this.$.menu.close();
    this.showPasswordMoveToAccountDialog_ = true;
  }

  private onPasswordMoveToAccountDialogClosed_() {
    this.showPasswordMoveToAccountDialog_ = false;
    this.activePassword_ = null;

    // The entry possibly disappeared, so don't reset the focus.
    this.activeDialogAnchor_ = null;
  }

  private onPasswordRemoveDialogClosed_() {
    this.showPasswordRemoveDialog_ = false;
    this.activePassword_ = null;

    // A removal possibly happened, so don't reset the focus.
    this.activeDialogAnchor_ = null;
  }

  /**
   * Whether the move option should be present in the overflow menu.
   */
  private shouldShowMoveToAccountOption_(): boolean {
    const isFirstSignedInAccountPassword = !!this.activePassword_ &&
        this.activePassword_.entry.urls.origin.includes(
            'accounts.google.com') &&
        this.activePassword_.entry.username === this.firstSignedInAccountEmail_;
    // It's not useful to move a password for an account into that same account.
    return this.allowMoveToAccountOption && !isFirstSignedInAccountPassword;
  }
}

customElements.define(
    PasswordsListHandlerElement.is, PasswordsListHandlerElement);
