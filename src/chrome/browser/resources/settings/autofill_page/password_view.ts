// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'password-view' is the subpage containing details about the
 * password such as the URL, the username, the password and the note.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import '../controls/settings_textarea.js';
import '../i18n_setup.js';
// <if expr="chromeos_ash or chromeos_lacros">
import '../controls/password_prompt_dialog.js';
// </if>
import '../settings_shared_css.js';
import './password_edit_dialog.js';
import './password_remove_dialog.js';
import './passwords_shared_css.js';

import {assert} from 'chrome://resources/js/assert_ts.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nMixin, I18nMixinInterface} from 'chrome://resources/js/i18n_mixin.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {loadTimeData} from '../i18n_setup.js';
import {routes} from '../route.js';
import {Route, RouteObserverMixin, RouteObserverMixinInterface, Router} from '../router.js';

// <if expr="chromeos_ash or chromeos_lacros">
import {BlockingRequestManager} from './blocking_request_manager.js';
// </if>
import {MergePasswordsStoreCopiesMixin, MergePasswordsStoreCopiesMixinInterface} from './merge_passwords_store_copies_mixin.js';
import {MultiStorePasswordUiEntry} from './multi_store_password_ui_entry.js';
import {SavedPasswordEditedEvent} from './password_edit_dialog.js';
import {PasswordRemovalMixin, PasswordRemovalMixinInterface} from './password_removal_mixin.js';
import {PasswordRemoveDialogPasswordsRemovedEvent} from './password_remove_dialog.js';
import {PasswordRequestorMixin, PasswordRequestorMixinInterface} from './password_requestor_mixin.js';
import {getTemplate} from './password_view.html.js';

const PasswordViewElementBase =
    PasswordRemovalMixin(PasswordRequestorMixin(MergePasswordsStoreCopiesMixin(
        RouteObserverMixin(I18nMixin(PolymerElement))))) as {
      new (): PolymerElement & I18nMixinInterface &
          RouteObserverMixinInterface &
          MergePasswordsStoreCopiesMixinInterface &
          PasswordRequestorMixinInterface & PasswordRemovalMixinInterface,
    };

export enum PasswordRemovalUrlParams {
  REMOVED_FROM_ACCOUNT = 'removedFromAccount',
  REMOVED_FROM_DEVICE = 'removedFromDevice',
}

export enum PasswordViewPageUrlParams {
  SITE = 'site',
  USERNAME = 'username',
}

export class PasswordViewElement extends PasswordViewElementBase {
  static get is() {
    return 'password-view';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      activeDialogAnchorStack_: {
        type: Array,
        value: () => [],
      },

      credential: {
        type: Object,
        value: null,
        notify: true,
      },

      isPasswordVisible_: {
        type: Boolean,
        value: false,
      },

      password_: {
        type: String,
        value: '',
      },

      // <if expr="chromeos_ash or chromeos_lacros">
      showPasswordPromptDialog_: Boolean,
      // </if>

      site: {
        type: String,
        value: '',
      },

      username: {
        type: String,
        value: '',
      },
    };
  }

  static get observers() {
    return ['savedPasswordsChanged_(savedPasswords.splices, site, username)'];
  }

  private activeDialogAnchorStack_: Array<HTMLElement>;
  credential: MultiStorePasswordUiEntry|null;
  private isPasswordVisible_: boolean;
  private password_: string;
  // <if expr="chromeos_ash or chromeos_lacros">
  private showPasswordPromptDialog_: boolean;
  // </if>
  private showEditDialog_: boolean;
  site: string;
  username: string;

  // <if expr="chromeos_ash or chromeos_lacros">
  override connectedCallback() {
    super.connectedCallback();

    // If the user's account supports the password check, an auth token will be
    // required in order for them to view or export passwords. Otherwise there
    // is no additional security so |tokenRequestManager| will immediately
    // resolve requests.
    if (loadTimeData.getBoolean('userCannotManuallyEnterPassword')) {
      this.tokenRequestManager = new BlockingRequestManager();
    } else {
      this.tokenRequestManager =
          new BlockingRequestManager(() => this.openPasswordPromptDialog_());
    }
  }
  // </if>

  override currentRouteChanged(route: Route): void {
    if (route !== routes.PASSWORD_VIEW) {
      this.site = '';
      this.username = '';
      this.password_ = '';
      this.credential = null;
      return;
    }
    const queryParameters = Router.getInstance().getQueryParameters();

    const site = queryParameters.get(PasswordViewPageUrlParams.SITE);
    if (!site) {
      return;
    }

    const username = queryParameters.get(PasswordViewPageUrlParams.USERNAME);
    if (!username) {
      return;
    }
    this.site = site;
    this.username = username;
  }

  override onPasswordRemoveDialogPasswordsRemoved(
      event: PasswordRemoveDialogPasswordsRemovedEvent) {
    super.onPasswordRemoveDialogPasswordsRemoved(event);
    this.rerouteAndShowRemovalNotification_(
        event.detail.removedFromAccount, event.detail.removedFromDevice);
  }

  /** Gets the title text for the show/hide icon. */
  private getPasswordButtonTitle_(): string {
    assert(!this.isFederated_());
    return this.i18n(this.isPasswordVisible_ ? 'hidePassword' : 'showPassword');
  }

  /** Get the right icon to display when hiding/showing a password. */
  private getIconClass_(): string {
    assert(!this.isFederated_());
    return this.isPasswordVisible_ ? 'icon-visibility-off' : 'icon-visibility';
  }

  /**
   * Show the password or a placeholder with 10 characters when password is not
   * set.
   */
  private getPassword_(): string {
    return this.password_ || ' '.repeat(10);
  }

  /**
   * Gets the password input's type. Should be 'text' when input content is
   * visible otherwise 'password'. If the entry is a federated credential,
   * the content (federation text) is always visible.
   */
  private getPasswordInputType_(): string {
    return this.isFederated_() || this.isPasswordVisible_ ? 'text' : 'password';
  }

  private isFederated_(): boolean {
    return !!this.credential && !!this.credential.federationText;
  }

  /** Handler to copy the password from the password field. */
  private onCopyPasswordButtonClick_() {
    assert(!this.isFederated_());
    this.requestPlaintextPassword(
            this.credential!.getAnyId(),
            chrome.passwordsPrivate.PlaintextReason.COPY)
        .catch(() => {});
  }

  /** Handler to copy the username from the username field. */
  private onCopyUsernameButtonClick_() {
    navigator.clipboard.writeText(this.credential!.username);
  }

  /** Handler for the remove button. */
  private onDeleteButtonClick_() {
    assert(this.credential);
    if (!this.removePassword(this.credential)) {
      return;
    }
    this.rerouteAndShowRemovalNotification_(
        this.credential.isPresentInAccount(),
        this.credential.isPresentOnDevice());
  }

  /** Handler to open edit dialog for the password. */
  private onEditButtonClick_() {
    assert(!this.isFederated_());
    this.requestPlaintextPassword(
            this.credential!.getAnyId(),
            chrome.passwordsPrivate.PlaintextReason.EDIT)
        .then(password => {
          this.credential!.password = password;
          this.showEditDialog_ = true;
        }, () => {});
  }

  private onEditDialogClosed_() {
    this.showEditDialog_ = false;
  }

  private onSavedPasswordEdited_(event: SavedPasswordEditedEvent) {
    const newUsername = event.detail.username;
    if (this.credential!.username === newUsername) {
      return;
    }
    // The dialog is recently closed. If the username has changed, reroute the
    // page to the new credential.
    const newParams = Router.getInstance().getQueryParameters();
    newParams.set(PasswordViewPageUrlParams.USERNAME, newUsername);
    Router.getInstance().updateRouteParams(newParams);
  }

  // <if expr="chromeos_ash or chromeos_lacros">
  /**
   * When this event fired, it means that the password-prompt-dialog succeeded
   * in creating a fresh token in the quickUnlockPrivate API. Because new tokens
   * can only ever be created immediately following a GAIA password check, the
   * passwordsPrivate API can now safely grant requests for secure data (i.e.
   * saved passwords) for a limited time. This observer resolves the request,
   * triggering a callback that requires a fresh auth token to succeed and that
   * was provided to the BlockingRequestManager by another DOM element seeking
   * secure data.
   *
   * @param e Contains newly created auth token
   *     chrome.quickUnlockPrivate.TokenInfo. Note that its precise value is not
   *     relevant here, only the facts that it's created.
   */
  private onTokenObtained_(
      e: CustomEvent<chrome.quickUnlockPrivate.TokenInfo>) {
    assert(e.detail);
    this.tokenRequestManager.resolve();
  }

  private onPasswordPromptClose_() {
    this.showPasswordPromptDialog_ = false;
    const toFocus = this.activeDialogAnchorStack_.pop();
    assert(toFocus);
    focusWithoutInk(toFocus);
  }

  private openPasswordPromptDialog_() {
    this.activeDialogAnchorStack_.push(getDeepActiveElement() as HTMLElement);
    this.showPasswordPromptDialog_ = true;
  }
  // </if>

  /** Handler for tapping the show/hide button. */
  private onShowPasswordButtonClick_() {
    assert(!this.isFederated_());
    if (this.isPasswordVisible_) {
      this.password_ = '';
      this.isPasswordVisible_ = false;
      return;
    }
    this.requestPlaintextPassword(
            this.credential!.getAnyId(),
            chrome.passwordsPrivate.PlaintextReason.VIEW)
        .then(password => {
          this.password_ = password;
          this.isPasswordVisible_ = true;
        }, () => {});
  }

  /** Reroutes to PASSWORDS page and shows the removal notification */
  private rerouteAndShowRemovalNotification_(
      removedFromAccount: boolean, removedFromDevice: boolean): void {
    // TODO(https://crbug.com/1298027): find a way to reroute to
    // DEVICE_PASSWORDS if view is opened from there.
    const params = new URLSearchParams();
    params.set(
        PasswordRemovalUrlParams.REMOVED_FROM_ACCOUNT,
        removedFromAccount.toString());
    params.set(
        PasswordRemovalUrlParams.REMOVED_FROM_DEVICE,
        removedFromDevice.toString());
    Router.getInstance().navigateTo(routes.PASSWORDS, params);
  }

  private savedPasswordsChanged_() {
    this.credential = null;
    this.password_ = '';
    this.isPasswordVisible_ = false;
    if (!this.savedPasswords.length || !this.site || !this.username) {
      return;
    }

    const item =
        this.savedPasswords.find((savedPassword: MultiStorePasswordUiEntry) => {
          return savedPassword.urls.shown === this.site &&
              savedPassword.username === this.username;
        });
    if (!item) {
      if (!this.showEditDialog_) {
        // Rerouting might have happened due to the edited username. Do not
        // reroute back.
        Router.getInstance().navigateTo(routes.PASSWORDS);
      }
      return;
    }

    this.credential = item;
    if (item.federationText) {
      this.password_ = item.federationText!;
    }
    this.showEditDialog_ = false;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'password-view': PasswordViewElement;
  }
}

customElements.define(PasswordViewElement.is, PasswordViewElement);
