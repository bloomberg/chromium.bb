// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'passwords-device-section' represents the page containing the
 * list of passwords which have at least one copy on the user device. The page
 * is only displayed for users of the account-scoped passwords storage. If other
 * users try to access it, they will be redirected to PasswordsSection.
 *
 * This page is *not* displayed on ChromeOS.
 */

import './passwords_list_handler.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import '../settings_shared_css.js';
import './avatar_icon.js';
import './passwords_shared_css.js';
import './password_list_item.js';
import './password_move_multiple_passwords_to_account_dialog.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import 'chrome://resources/polymer/v3_0/iron-list/iron-list.js';

import {CrToastElement} from 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {WebUIListenerMixin, WebUIListenerMixinInterface} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {GlobalScrollTargetMixin} from '../global_scroll_target_mixin.js';
import {loadTimeData} from '../i18n_setup.js';
import {OpenWindowProxyImpl} from '../open_window_proxy.js';
import {StoredAccount, SyncBrowserProxyImpl, SyncStatus} from '../people_page/sync_browser_proxy.js';
import {routes} from '../route.js';
import {Route, RouteObserverMixin, RouteObserverMixinInterface, Router} from '../router.js';

import {MergePasswordsStoreCopiesMixin, MergePasswordsStoreCopiesMixinInterface} from './merge_passwords_store_copies_mixin.js';
import {MultiStorePasswordUiEntry} from './multi_store_password_ui_entry.js';
import {AccountStorageOptInStateChangedListener, PasswordManagerImpl} from './password_manager_proxy.js';
import {PasswordsListHandlerElement} from './passwords_list_handler.js';

/**
 * Checks if an HTML element is an editable. An editable element is either a
 * text input or a text area.
 */
function isEditable(element: Element): boolean {
  const nodeName = element.nodeName.toLowerCase();
  return element.nodeType === Node.ELEMENT_NODE &&
      (nodeName === 'textarea' ||
       (nodeName === 'input' &&
        /^(?:text|search|email|number|tel|url|password)$/i.test(
            (element as HTMLInputElement).type)));
}

interface PasswordsDeviceSectionElement {
  $: {
    toast: CrToastElement,
    passwordsListHandler: PasswordsListHandlerElement,
  };
}

// TODO(crbug.com/1234307): Remove when RouteObserverMixin is converted to
// TypeScript.
type Constructor<T> = new (...args: any[]) => T;

const PasswordsDeviceSectionElementBase =
    MergePasswordsStoreCopiesMixin(GlobalScrollTargetMixin(WebUIListenerMixin(
        RouteObserverMixin(PolymerElement) as unknown as
        Constructor<PolymerElement>))) as {
      new (): PolymerElement & WebUIListenerMixinInterface &
      MergePasswordsStoreCopiesMixinInterface & RouteObserverMixinInterface
    };

class PasswordsDeviceSectionElement extends PasswordsDeviceSectionElementBase {
  static get is() {
    return 'passwords-device-section';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      subpageRoute: {
        type: Object,
        value: routes.DEVICE_PASSWORDS,
      },

      /** Search filter on the saved passwords. */
      filter: {
        type: String,
        value: '',
      },

      /**
       * Passwords displayed in the device-only subsection.
       */
      deviceOnlyPasswords_: {
        type: Array,
        value: () => [],
        computed:
            'computeDeviceOnlyPasswords_(savedPasswords, savedPasswords.splices)',
      },

      /**
       * Passwords displayed in the 'device and account' subsection.
       */
      deviceAndAccountPasswords_: {
        type: Array,
        value: () => [],
        computed: 'computeDeviceAndAccountPasswords_(savedPasswords, ' +
            'savedPasswords.splices)',
      },

      /**
       * Passwords displayed in both the device-only and 'device and account'
       * subsections.
       */
      allDevicePasswords_: {
        type: Array,
        value: () => [],
        computed: 'computeAllDevicePasswords_(savedPasswords.splices)',
        observer: 'onAllDevicePasswordsChanged_',
      },

      /**
       * Whether the entry point leading to the dialog to move multiple
       * passwords to the Google Account should be shown. It's shown only where
       * there is at least one password store on device.
       */
      shouldShowMoveMultiplePasswordsBanner_: {
        type: Boolean,
        value: false,
        computed: 'computeShouldShowMoveMultiplePasswordsBanner_(' +
            'savedPasswords, savedPasswords.splices)',
      },


      lastFocused_: Object,
      listBlurred_: Boolean,
      accountEmail_: String,

      isUserAllowedToAccessPage_: {
        type: Boolean,
        computed:
            'computeIsUserAllowedToAccessPage_(signedIn_, syncDisabled_,' +
            'optedInForAccountStorage_)',
      },

      /**
       * Whether the user is signed in, one of the requirements to view this
       * page.
       */
      signedIn_: {
        type: Boolean,
        value: null,
      },

      /**
       * Whether Sync is disabled, one of the requirements to view this page.
       */
      syncDisabled_: {
        type: Boolean,
        value: null,
      },

      /**
       * Whether the user has opted in to the account-scoped password storage,
       * one of the requirements to view this page.
       */
      optedInForAccountStorage_: {
        type: Boolean,
        value: null,
      },

      showMoveMultiplePasswordsDialog_: Boolean,

      currentRoute_: {
        type: Object,
        value: null,
      },

      devicePasswordsLabel_: {
        type: String,
        value: '',
      },
    };
  }

  static get observers() {
    return [
      'maybeRedirectToPasswordsPage_(isUserAllowedToAccessPage_, currentRoute_)'
    ];
  }

  subpageRoute: Route;
  filter: string;
  private deviceOnlyPasswords_: Array<MultiStorePasswordUiEntry>;
  private deviceAndAccountPasswords_: Array<MultiStorePasswordUiEntry>;
  private allDevicePasswords_: Array<MultiStorePasswordUiEntry>;
  private shouldShowMoveMultiplePasswordsBanner_: boolean;
  private lastFocused_: MultiStorePasswordUiEntry;
  private listBlurred_: boolean;
  private accountEmail_: string;
  private isUserAllowedToAccessPage_: boolean;
  private signedIn_: boolean|null;
  private syncDisabled_: boolean|null;
  private optedInForAccountStorage_: boolean|null;
  private showMoveMultiplePasswordsDialog_: boolean;
  private currentRoute_: Route|null;
  private devicePasswordsLabel_: string;
  private accountStorageOptInStateListener_:
      AccountStorageOptInStateChangedListener|null = null;

  connectedCallback() {
    super.connectedCallback();

    this.addListenersForAccountStorageRequirements_();
    this.currentRoute_ = Router.getInstance().currentRoute;

    const extractFirstStoredAccountEmail = (accounts: Array<StoredAccount>) => {
      this.accountEmail_ = accounts.length > 0 ? accounts[0].email : '';
    };
    SyncBrowserProxyImpl.getInstance().getStoredAccounts().then(
        extractFirstStoredAccountEmail);
    this.addWebUIListener(
        'stored-accounts-updated', extractFirstStoredAccountEmail);
  }

  ready() {
    super.ready();

    document.addEventListener('keydown', keyboardEvent => {
      // <if expr="is_macosx">
      if (keyboardEvent.metaKey && keyboardEvent.key === 'z') {
        this.onUndoKeyBinding_(keyboardEvent);
      }
      // </if>
      // <if expr="not is_macosx">
      if (keyboardEvent.ctrlKey && keyboardEvent.key === 'z') {
        this.onUndoKeyBinding_(keyboardEvent);
      }
      // </if>
    });
  }

  disconnectedCallback() {
    super.disconnectedCallback();

    PasswordManagerImpl.getInstance().removeAccountStorageOptInStateListener(
        this.accountStorageOptInStateListener_!);
    this.accountStorageOptInStateListener_ = null;
  }

  private computeAllDevicePasswords_(): Array<MultiStorePasswordUiEntry> {
    return this.savedPasswords.filter(p => p.isPresentOnDevice());
  }

  private computeDeviceOnlyPasswords_(): Array<MultiStorePasswordUiEntry> {
    return this.savedPasswords.filter(
        p => p.isPresentOnDevice() && !p.isPresentInAccount());
  }

  private computeDeviceAndAccountPasswords_():
      Array<MultiStorePasswordUiEntry> {
    return this.savedPasswords.filter(
        p => p.isPresentOnDevice() && p.isPresentInAccount());
  }

  private computeIsUserAllowedToAccessPage_(): boolean {
    // Only deny access when one of the requirements has already been computed
    // and is not satisfied.
    return (this.signedIn_ === null || !!this.signedIn_) &&
        (this.syncDisabled_ === null || !!this.syncDisabled_) &&
        (this.optedInForAccountStorage_ === null ||
         !!this.optedInForAccountStorage_);
  }

  private computeShouldShowMoveMultiplePasswordsBanner_(): boolean {
    if (this.allDevicePasswords_.length === 0) {
      return false;
    }

    // Check if all username, and urls are unique. The existence of two entries
    // with the same url and username indicate that they must have conflicting
    // passwords, otherwise, they would have been deduped in
    // MergePasswordsStoreCopiesBehavior. This however may mistakenly exclude
    // users who have conflicting duplicates within the same store, which is an
    // acceptable compromise.
    return this.savedPasswords.every(
        p1 =>
            (this.savedPasswords
                 .filter(
                     p2 => p1.username === p2.username &&
                         p1.urls.origin === p2.urls.origin)
                 .length === 1));
  }

  private async onAllDevicePasswordsChanged_() {
    this.devicePasswordsLabel_ =
        await PluralStringProxyImpl.getInstance().getPluralString(
            'movePasswordsToAccount', this.allDevicePasswords_.length);
  }

  /**
   * From RouteObserverMixin.
   */
  currentRouteChanged(route: Route) {
    super.currentRouteChanged(route);
    this.currentRoute_ = route || null;
  }

  private addListenersForAccountStorageRequirements_() {
    const setSyncDisabled = (syncStatus: SyncStatus) => {
      this.syncDisabled_ = !syncStatus.signedIn;
    };
    SyncBrowserProxyImpl.getInstance().getSyncStatus().then(setSyncDisabled);
    this.addWebUIListener('sync-status-changed', setSyncDisabled);

    const setSignedIn = (storedAccounts: Array<StoredAccount>) => {
      this.signedIn_ = storedAccounts.length > 0;
    };
    SyncBrowserProxyImpl.getInstance().getStoredAccounts().then(setSignedIn);
    this.addWebUIListener('stored-accounts-updated', setSignedIn);

    const setOptedIn = (optedInForAccountStorage: boolean) => {
      this.optedInForAccountStorage_ = optedInForAccountStorage;
    };
    PasswordManagerImpl.getInstance().isOptedInForAccountStorage().then(
        setOptedIn);
    PasswordManagerImpl.getInstance().addAccountStorageOptInStateListener(
        setOptedIn);
    this.accountStorageOptInStateListener_ = setOptedIn;
  }

  private isNonEmpty_(passwords: Array<MultiStorePasswordUiEntry>): boolean {
    return passwords.length > 0;
  }

  private getFilteredPasswords_(
      passwords: Array<MultiStorePasswordUiEntry>,
      filter: string): Array<MultiStorePasswordUiEntry> {
    if (!filter) {
      return passwords.slice();
    }

    return passwords.filter(
        p => [p.urls.shown, p.username].some(
            term => term.toLowerCase().includes(filter.toLowerCase())));
  }

  /**
   * Handle the undo shortcut.
   */
  // TODO(crbug.com/1102294): Consider grouping the ctrl-z related code into
  // a dedicated behavior.
  private onUndoKeyBinding_(event: Event) {
    const activeElement = getDeepActiveElement();
    if (!activeElement || !isEditable(activeElement)) {
      PasswordManagerImpl.getInstance().undoRemoveSavedPasswordOrException();
      this.$.passwordsListHandler.onSavedPasswordOrExceptionRemoved();
      // Preventing the default is necessary to not conflict with a possible
      // search action.
      event.preventDefault();
    }
  }

  private onManageAccountPasswordsClicked_() {
    OpenWindowProxyImpl.getInstance().openURL(
        loadTimeData.getString('googlePasswordManagerUrl'));
  }

  private onMoveMultiplePasswordsTap_() {
    this.showMoveMultiplePasswordsDialog_ = true;
  }

  private onMoveMultiplePasswordsDialogClose_() {
    if (this.shadowRoot!
            .querySelector(
                'password-move-multiple-passwords-to-account-dialog')!
            .wasConfirmed()) {
      this.$.toast.show();
    }
    this.showMoveMultiplePasswordsDialog_ = false;
  }

  private maybeRedirectToPasswordsPage_() {
    // The component can be attached even if the route is no longer
    // DEVICE_PASSWORDS, so check to avoid navigating when the user is viewing
    // other non-related pages.
    if (!this.isUserAllowedToAccessPage_ &&
        this.currentRoute_ === routes.DEVICE_PASSWORDS) {
      Router.getInstance().navigateTo(routes.PASSWORDS);
    }
  }
}

customElements.define(
    PasswordsDeviceSectionElement.is, PasswordsDeviceSectionElement);
