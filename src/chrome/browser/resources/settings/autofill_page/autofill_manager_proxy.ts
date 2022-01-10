// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';

export type PersonalDataChangedListener =
    (addresses: Array<chrome.autofillPrivate.AddressEntry>,
     creditCards: Array<chrome.autofillPrivate.CreditCardEntry>) => void;

/**
 * Interface for all callbacks to the autofill API.
 */
export interface AutofillManagerProxy {
  /**
   * Add an observer to the list of personal data.
   */
  setPersonalDataManagerListener(listener: PersonalDataChangedListener): void;

  /**
   * Remove an observer from the list of personal data.
   */
  removePersonalDataManagerListener(listener: PersonalDataChangedListener):
      void;

  /**
   * Request the list of addresses.
   */
  getAddressList(
      callback: (entries: Array<chrome.autofillPrivate.AddressEntry>) => void):
      void;

  /**
   * Saves the given address.
   */
  saveAddress(address: chrome.autofillPrivate.AddressEntry): void;

  /** @param guid The guid of the address to remove.  */
  removeAddress(guid: string): void;
}

/**
 * Implementation that accesses the private API.
 */
export class AutofillManagerImpl implements AutofillManagerProxy {
  setPersonalDataManagerListener(listener: PersonalDataChangedListener) {
    chrome.autofillPrivate.onPersonalDataChanged.addListener(listener);
  }

  removePersonalDataManagerListener(listener: PersonalDataChangedListener) {
    chrome.autofillPrivate.onPersonalDataChanged.removeListener(listener);
  }

  getAddressList(
      callback: (entries: Array<chrome.autofillPrivate.AddressEntry>) => void) {
    chrome.autofillPrivate.getAddressList(callback);
  }

  saveAddress(address: chrome.autofillPrivate.AddressEntry) {
    chrome.autofillPrivate.saveAddress(address);
  }

  removeAddress(guid: string) {
    chrome.autofillPrivate.removeEntry(assert(guid));
  }

  static getInstance(): AutofillManagerProxy {
    return instance || (instance = new AutofillManagerImpl());
  }

  static setInstance(obj: AutofillManagerProxy) {
    instance = obj;
  }
}

let instance: AutofillManagerProxy|null = null;
