// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Externs generated from namespace: autofillPrivate */

/**
 * @const
 */
chrome.autofillPrivate = {};

/**
 * @enum {string}
 * @see https://developer.chrome.com/extensions/autofillPrivate#type-AddressField
 */
chrome.autofillPrivate.AddressField = {
  FULL_NAME: 'FULL_NAME',
  COMPANY_NAME: 'COMPANY_NAME',
  ADDRESS_LINES: 'ADDRESS_LINES',
  ADDRESS_LEVEL_1: 'ADDRESS_LEVEL_1',
  ADDRESS_LEVEL_2: 'ADDRESS_LEVEL_2',
  ADDRESS_LEVEL_3: 'ADDRESS_LEVEL_3',
  POSTAL_CODE: 'POSTAL_CODE',
  SORTING_CODE: 'SORTING_CODE',
  COUNTRY_CODE: 'COUNTRY_CODE',
};

/**
 * @typedef {{
 *   summaryLabel: string,
 *   summarySublabel: (string|undefined),
 *   isLocal: (boolean|undefined),
 *   isCached: (boolean|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/autofillPrivate#type-AutofillMetadata
 */
var AutofillMetadata;

/**
 * @typedef {{
 *   guid: (string|undefined),
 *   fullNames: (!Array<string>|undefined),
 *   companyName: (string|undefined),
 *   addressLines: (string|undefined),
 *   addressLevel1: (string|undefined),
 *   addressLevel2: (string|undefined),
 *   addressLevel3: (string|undefined),
 *   postalCode: (string|undefined),
 *   sortingCode: (string|undefined),
 *   countryCode: (string|undefined),
 *   phoneNumbers: (!Array<string>|undefined),
 *   emailAddresses: (!Array<string>|undefined),
 *   languageCode: (string|undefined),
 *   metadata: (AutofillMetadata|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/autofillPrivate#type-AddressEntry
 */
var AddressEntry;

/**
 * @typedef {{
 *   field: !chrome.autofillPrivate.AddressField,
 *   fieldName: string,
 *   isLongField: boolean,
 *   placeholder: (string|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/autofillPrivate#type-AddressComponent
 */
var AddressComponent;

/**
 * @typedef {{
 *   row: !Array<AddressComponent>
 * }}
 * @see https://developer.chrome.com/extensions/autofillPrivate#type-AddressComponentRow
 */
var AddressComponentRow;

/**
 * @typedef {{
 *   components: !Array<AddressComponentRow>,
 *   languageCode: string
 * }}
 * @see https://developer.chrome.com/extensions/autofillPrivate#type-AddressComponents
 */
var AddressComponents;

/**
 * @typedef {{
 *   guid: (string|undefined),
 *   name: (string|undefined),
 *   cardNumber: (string|undefined),
 *   expirationMonth: (string|undefined),
 *   expirationYear: (string|undefined),
 *   metadata: (AutofillMetadata|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/autofillPrivate#type-CreditCardEntry
 */
var CreditCardEntry;

/**
 * @typedef {{
 *   phoneNumbers: !Array<string>,
 *   indexOfNewNumber: number,
 *   countryCode: string
 * }}
 * @see https://developer.chrome.com/extensions/autofillPrivate#type-ValidatePhoneParams
 */
var ValidatePhoneParams;

/**
 * Saves the given address. If |address| has an empty string as its ID, it will
 * be assigned a new one and added as a new entry.
 * @param {AddressEntry} address The address entry to save.
 * @see https://developer.chrome.com/extensions/autofillPrivate#method-saveAddress
 */
chrome.autofillPrivate.saveAddress = function(address) {};

/**
 * Gets the address components for a given country code.
 * @param {string} countryCode A two-character string representing the address'
 *     country     whose components should be returned. See autofill_country.cc
 *     for a     list of valid codes.
 * @param {function(AddressComponents):void} callback Callback which will be
 *     called with components.
 * @see https://developer.chrome.com/extensions/autofillPrivate#method-getAddressComponents
 */
chrome.autofillPrivate.getAddressComponents = function(countryCode, callback) {};

/**
 * Saves the given credit card. If |card| has an empty string as its ID, it will
 * be assigned a new one and added as a new entry.
 * @param {CreditCardEntry} card The card entry to save.
 * @see https://developer.chrome.com/extensions/autofillPrivate#method-saveCreditCard
 */
chrome.autofillPrivate.saveCreditCard = function(card) {};

/**
 * Removes the entry (address or credit card) with the given ID.
 * @param {string} guid ID of the entry to remove.
 * @see https://developer.chrome.com/extensions/autofillPrivate#method-removeEntry
 */
chrome.autofillPrivate.removeEntry = function(guid) {};

/**
 * Validates a newly-added phone number and invokes the callback with a list of
 * validated numbers. Note that if the newly-added number was invalid, it will
 * not be returned in the list of valid numbers.
 * @param {ValidatePhoneParams} params The parameters to this function.
 * @param {function(!Array<string>):void} callback Callback which will be called
 *     with validated phone numbers.
 * @see https://developer.chrome.com/extensions/autofillPrivate#method-validatePhoneNumbers
 */
chrome.autofillPrivate.validatePhoneNumbers = function(params, callback) {};

/**
 * Clears the data associated with a wallet card which was saved locally so that
 * the saved copy is masked (e.g., "Card ending in 1234").
 * @param {string} guid GUID of the credit card to mask.
 * @see https://developer.chrome.com/extensions/autofillPrivate#method-maskCreditCard
 */
chrome.autofillPrivate.maskCreditCard = function(guid) {};

/**
 * Fired when the address list has changed, meaning that an entry has been
 * added, removed, or changed.  |entries| The updated list of entries.
 * @type {!ChromeEvent}
 * @see https://developer.chrome.com/extensions/autofillPrivate#event-onAddressListChanged
 */
chrome.autofillPrivate.onAddressListChanged;

/**
 * Fired when the credit card list has changed, meaning that an entry has been
 * added, removed, or changed.  |entries| The updated list of entries.
 * @type {!ChromeEvent}
 * @see https://developer.chrome.com/extensions/autofillPrivate#event-onCreditCardListChanged
 */
chrome.autofillPrivate.onCreditCardListChanged;


