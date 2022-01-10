// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-payments-section' is the section containing saved
 * credit cards for use in autofill and payments APIs.
 */

import 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_link_row/cr_link_row.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '../settings_shared_css.js';
import '../controls/settings_toggle_button.js';
import '../prefs/prefs.js';
import './credit_card_edit_dialog.js';
import './passwords_shared_css.js';
import './payments_list.js';

import {CrActionMenuElement} from 'chrome://resources/cr_elements/cr_action_menu/cr_action_menu.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {focusWithoutInk} from 'chrome://resources/js/cr/ui/focus_without_ink.m.js';
import {I18nMixin} from 'chrome://resources/js/i18n_mixin.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {MetricsBrowserProxyImpl, PrivacyElementInteractions} from '../metrics_browser_proxy.js';

import {PersonalDataChangedListener} from './autofill_manager_proxy.js';
import {PaymentsManagerImpl, PaymentsManagerProxy} from './payments_manager_proxy.js';


type DotsCardMenuiClickEvent = CustomEvent<{
  creditCard: chrome.autofillPrivate.CreditCardEntry,
  anchorElement: HTMLElement,
}>;

declare global {
  interface HTMLElementEventMap {
    'dots-card-menu-click': DotsCardMenuiClickEvent;
  }
}

export interface SettingsPaymentsSectionElement {
  $: {
    addCreditCard: CrButtonElement,
    autofillCreditCardToggle: SettingsToggleButtonElement,
    canMakePaymentToggle: SettingsToggleButtonElement,
    creditCardSharedMenu: CrActionMenuElement,
    menuClearCreditCard: HTMLElement,
    menuEditCreditCard: HTMLElement,
    menuRemoveCreditCard: HTMLElement,
    migrateCreditCards: HTMLElement,
    paymentsList: HTMLElement,
  };
}

const SettingsPaymentsSectionElementBase = I18nMixin(PolymerElement);

export class SettingsPaymentsSectionElement extends
    SettingsPaymentsSectionElementBase {
  static get is() {
    return 'settings-payments-section';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      prefs: Object,

      /**
       * An array of all saved credit cards.
       */
      creditCards: {
        type: Array,
        value: () => [],
      },

      /**
       * An array of all saved UPI IDs.
       */
      upiIds: {
        type: Array,
        value: () => [],
      },

      /**
       * Set to true if user can be verified through FIDO authentication.
       */
      userIsFidoVerifiable_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean(
              'fidoAuthenticationAvailableForAutofill');
        },
      },

      /**
       * The model for any credit card related action menus or dialogs.
       */
      activeCreditCard_: Object,

      showCreditCardDialog_: Boolean,
      migratableCreditCardsInfo_: String,

      /**
       * Whether migration local card on settings page is enabled.
       */
      migrationEnabled_: {
        type: Boolean,
        value() {
          return loadTimeData.getBoolean('migrationEnabled');
        },
        readOnly: true,
      },
    };
  }

  prefs: {[key: string]: any};
  creditCards: Array<chrome.autofillPrivate.CreditCardEntry>;
  upiIds: Array<string>;
  private userIsFidoVerifiable_: boolean;
  private activeCreditCard_: chrome.autofillPrivate.CreditCardEntry|null;
  private showCreditCardDialog_: boolean;
  private migratableCreditCardsInfo_: string;
  private migrationEnabled_: boolean;
  private activeDialogAnchor_: HTMLElement|null;
  private paymentsManager_: PaymentsManagerProxy =
      PaymentsManagerImpl.getInstance();
  private setPersonalDataListener_: PersonalDataChangedListener|null = null;

  constructor() {
    super();

    /**
     * The element to return focus to; when the currently active dialog is
     * closed.
     */
    this.activeDialogAnchor_ = null;
  }

  ready() {
    super.ready();

    this.addEventListener('save-credit-card', this.saveCreditCard_);
    this.addEventListener(
        'dots-card-menu-click', this.onCreditCardDotsMenuTap_);
    this.addEventListener(
        'remote-card-menu-click', this.onRemoteEditCreditCardTap_);
  }

  connectedCallback() {
    super.connectedCallback();

    // Create listener function.
    const setCreditCardsListener =
        (cardList: Array<chrome.autofillPrivate.CreditCardEntry>) => {
          this.creditCards = cardList;
        };

    // Update |userIsFidoVerifiable_| based on the availability of a platform
    // authenticator.
    if (window.PublicKeyCredential) {
      window.PublicKeyCredential.isUserVerifyingPlatformAuthenticatorAvailable()
          .then(r => {
            this.userIsFidoVerifiable_ = this.userIsFidoVerifiable_ && r;
          });
    }

    const setPersonalDataListener: PersonalDataChangedListener =
        (_addressList, cardList) => {
          this.creditCards = cardList;
        };

    const setUpiIdsListener = (upiIdList: Array<string>) => {
      this.upiIds = upiIdList;
    };

    // Remember the bound reference in order to detach.
    this.setPersonalDataListener_ = setPersonalDataListener;

    // Request initial data.
    this.paymentsManager_.getCreditCardList(setCreditCardsListener);
    this.paymentsManager_.getUpiIdList(setUpiIdsListener);

    // Listen for changes.
    this.paymentsManager_.setPersonalDataManagerListener(
        setPersonalDataListener);

    // Record that the user opened the payments settings.
    chrome.metricsPrivate.recordUserAction('AutofillCreditCardsViewed');
  }

  disconnectedCallback() {
    super.disconnectedCallback();

    this.paymentsManager_.removePersonalDataManagerListener(
        this.setPersonalDataListener_!);
    this.setPersonalDataListener_ = null;
  }

  /**
   * Opens the credit card action menu.
   */
  private onCreditCardDotsMenuTap_(e: DotsCardMenuiClickEvent) {
    // Copy item so dialog won't update model on cancel.
    this.activeCreditCard_ = e.detail.creditCard;

    this.$.creditCardSharedMenu.showAt(e.detail.anchorElement);
    this.activeDialogAnchor_ = e.detail.anchorElement;
  }

  /**
   * Handles tapping on the "Add credit card" button.
   */
  private onAddCreditCardTap_(e: Event) {
    e.preventDefault();
    const date = new Date();  // Default to current month/year.
    const expirationMonth = date.getMonth() + 1;  // Months are 0 based.
    this.activeCreditCard_ = {
      expirationMonth: expirationMonth.toString(),
      expirationYear: date.getFullYear().toString(),
    };
    this.showCreditCardDialog_ = true;
    this.activeDialogAnchor_ = this.$.addCreditCard;
  }

  private onCreditCardDialogClose_() {
    this.showCreditCardDialog_ = false;
    focusWithoutInk(assert(this.activeDialogAnchor_!));
    this.activeDialogAnchor_ = null;
    this.activeCreditCard_ = null;
  }

  /**
   * Handles tapping on the "Edit" credit card button.
   */
  private onMenuEditCreditCardTap_(e: Event) {
    e.preventDefault();

    if (this.activeCreditCard_!.metadata!.isLocal) {
      this.showCreditCardDialog_ = true;
    } else {
      this.onRemoteEditCreditCardTap_();
    }

    this.$.creditCardSharedMenu.close();
  }

  private onRemoteEditCreditCardTap_() {
    this.paymentsManager_.logServerCardLinkClicked();
    window.open(loadTimeData.getString('manageCreditCardsUrl'));
  }

  /**
   * Handles tapping on the "Remove" credit card button.
   */
  private onMenuRemoveCreditCardTap_() {
    this.paymentsManager_.removeCreditCard(this.activeCreditCard_!.guid!);
    this.$.creditCardSharedMenu.close();
    this.activeCreditCard_ = null;
  }

  /**
   * Handles tapping on the "Clear copy" button for cached credit cards.
   */
  private onMenuClearCreditCardTap_() {
    this.paymentsManager_.clearCachedCreditCard(this.activeCreditCard_!.guid!);
    this.$.creditCardSharedMenu.close();
    this.activeCreditCard_ = null;
  }

  /**
   * Handles clicking on the "Migrate" button for migrate local credit
   * cards.
   */
  private onMigrateCreditCardsClick_() {
    this.paymentsManager_.migrateCreditCards();
  }

  /**
   * Records changes made to the "Allow sites to check if you have payment
   * methods saved" setting to a histogram.
   */
  private onCanMakePaymentChange_() {
    MetricsBrowserProxyImpl.getInstance().recordSettingsPageHistogram(
        PrivacyElementInteractions.PAYMENT_METHOD);
  }

  /**
   * Listens for the save-credit-card event, and calls the private API.
   */
  private saveCreditCard_(
      event: CustomEvent<chrome.autofillPrivate.CreditCardEntry>) {
    this.paymentsManager_.saveCreditCard(event.detail);
  }

  /**
   * @return Whether the user is verifiable through FIDO authentication.
   */
  private shouldShowFidoToggle_(
      creditCardEnabled: boolean, userIsFidoVerifiable: boolean): boolean {
    return creditCardEnabled && userIsFidoVerifiable;
  }

  /**
   * Listens for the enable-authentication event, and calls the private API.
   */
  private setFIDOAuthenticationEnabledState_() {
    this.paymentsManager_.setCreditCardFIDOAuthEnabledState(
        this.shadowRoot!
            .querySelector<SettingsToggleButtonElement>(
                '#autofillCreditCardFIDOAuthToggle')!.checked);
  }

  /**
   * @return Whether to show the migration button.
   */
  private checkIfMigratable_(
      creditCards: Array<chrome.autofillPrivate.CreditCardEntry>,
      creditCardEnabled: boolean): boolean {
    // If migration prerequisites are not met, return false.
    if (!this.migrationEnabled_) {
      return false;
    }

    // If credit card enabled pref is false, return false.
    if (!creditCardEnabled) {
      return false;
    }

    const numberOfMigratableCreditCard =
        creditCards.filter(card => card.metadata!.isMigratable).length;
    // Check whether exist at least one local valid card for migration.
    if (numberOfMigratableCreditCard === 0) {
      return false;
    }

    // Update the display text depends on the number of migratable credit
    // cards.
    this.migratableCreditCardsInfo_ = numberOfMigratableCreditCard === 1 ?
        this.i18n('migratableCardsInfoSingle') :
        this.i18n('migratableCardsInfoMultiple');

    return true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'settings-payments-section': SettingsPaymentsSectionElement;
  }
}

customElements.define(
    SettingsPaymentsSectionElement.is, SettingsPaymentsSectionElement);
