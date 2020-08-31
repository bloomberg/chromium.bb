// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-credit-card-edit-dialog' is the dialog that allows
 * editing or creating a credit card entry.
 */

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/cr_elements/md_select_css.m.js';
import '../settings_shared_css.m.js';
import '../settings_vars_css.m.js';

import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';


Polymer({
  is: 'settings-credit-card-edit-dialog',

  _template: html`{__html_template__}`,

  properties: {
    /**
     * The credit card being edited.
     * @type {!chrome.autofillPrivate.CreditCardEntry}
     */
    creditCard: Object,

    /**
     * The actual title that's used for this dialog. Will be context sensitive
     * based on if |creditCard| is being created or edited.
     * @private
     */
    title_: String,

    /**
     * The list of months to show in the dropdown.
     * @private {!Array<string>}
     */
    monthList_: {
      type: Array,
      value: [
        '01', '02', '03', '04', '05', '06', '07', '08', '09', '10', '11', '12'
      ],
    },

    /**
     * The list of years to show in the dropdown.
     * @private {!Array<string>}
     */
    yearList_: Array,

    /** @private */
    expirationYear_: String,

    /** @private {string|undefined} */
    expirationMonth_: String,
  },

  behaviors: [
    I18nBehavior,
  ],

  /**
   * @return {boolean} True iff the provided expiration date is passed.
   * @private
   */
  checkIfCardExpired_(expirationMonth_, expirationYear_) {
    const now = new Date();
    return (
        expirationYear_ < now.getFullYear() ||
        (expirationYear_ == now.getFullYear() &&
         expirationMonth_ <= now.getMonth()));
  },

  /** @override */
  attached() {
    this.title_ = this.i18n(
        this.creditCard.guid ? 'editCreditCardTitle' : 'addCreditCardTitle');

    // Needed to initialize the disabled state of the Save button.
    this.onCreditCardNameOrNumberChanged_();

    // Add a leading '0' if a month is 1 char.
    if (this.creditCard.expirationMonth.length == 1) {
      this.creditCard.expirationMonth = '0' + this.creditCard.expirationMonth;
    }

    const date = new Date();
    let firstYear = date.getFullYear();
    let lastYear = firstYear + 19;  // Show next 19 years (20 total).
    let selectedYear = parseInt(this.creditCard.expirationYear, 10);

    // |selectedYear| must be valid and between first and last years.
    if (!selectedYear) {
      selectedYear = firstYear;
    } else if (selectedYear < firstYear) {
      firstYear = selectedYear;
    } else if (selectedYear > lastYear) {
      lastYear = selectedYear;
    }

    const yearList = [];
    for (let i = firstYear; i <= lastYear; ++i) {
      yearList.push(i.toString());
    }
    this.yearList_ = yearList;

    this.async(() => {
      this.expirationYear_ = selectedYear.toString();
      this.expirationMonth_ = this.creditCard.expirationMonth;
      this.$.dialog.showModal();
    });
  },

  /** Closes the dialog. */
  close() {
    this.$.dialog.close();
  },

  /**
   * Handler for tapping the 'cancel' button. Should just dismiss the dialog.
   * @private
   */
  onCancelButtonTap_() {
    this.$.dialog.cancel();
  },

  /**
   * Handler for tapping the save button.
   * @private
   */
  onSaveButtonTap_() {
    if (!this.saveEnabled_()) {
      return;
    }

    this.creditCard.expirationYear = this.expirationYear_;
    this.creditCard.expirationMonth = this.expirationMonth_;
    this.fire('save-credit-card', this.creditCard);
    this.close();
  },

  /** @private */
  onMonthChange_() {
    this.expirationMonth_ = this.monthList_[this.$.month.selectedIndex];
    this.$.saveButton.disabled = !this.saveEnabled_();
  },

  /** @private */
  onYearChange_() {
    this.expirationYear_ = this.yearList_[this.$.year.selectedIndex];
    this.$.saveButton.disabled = !this.saveEnabled_();
  },

  /** @private */
  onCreditCardNameOrNumberChanged_() {
    this.$.saveButton.disabled = !this.saveEnabled_();
  },

  /** @private */
  saveEnabled_() {
    // The save button is enabled if:
    // There is and name or number for the card
    // and the expiration date is valid.
    return ((this.creditCard.name && this.creditCard.name.trim()) ||
            (this.creditCard.cardNumber &&
             this.creditCard.cardNumber.trim())) &&
        !this.checkIfCardExpired_(this.expirationMonth_, this.expirationYear_);
  },
});
