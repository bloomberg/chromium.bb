// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test implementation of PasswordManagerProxy. */

// clang-format off
import {assertEquals} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

import {makePasswordCheckStatus} from './passwords_and_autofill_fake_data.js';

// clang-format on

export class PasswordManagerExpectations {
  constructor() {
    this.requested = {
      passwords: 0,
      exceptions: 0,
      plaintextPassword: 0,
      accountStorageOptInState: 0,
    };

    this.removed = {
      passwords: 0,
      exceptions: 0,
    };

    this.listening = {
      passwords: 0,
      exceptions: 0,
      accountStorageOptInState: 0,
    };
  }
}

/**
 * Test implementation
 * @implements {PasswordManagerProxy}
 */
export class TestPasswordManagerProxy extends TestBrowserProxy {
  constructor() {
    super([
      'requestPlaintextPassword',
      'startBulkPasswordCheck',
      'stopBulkPasswordCheck',
      'getCompromisedCredentials',
      'getWeakCredentials',
      'getPasswordCheckStatus',
      'getPlaintextInsecurePassword',
      'changeInsecureCredential',
      'removeInsecureCredential',
      'recordPasswordCheckInteraction',
      'recordPasswordCheckReferrer',
      'isOptedInForAccountStorage',
      'removeSavedPassword',
      'removeSavedPasswords',
      'movePasswordsToAccount',
      'removeException',
      'removeExceptions',
      'changeSavedPassword',
      'isAccountStoreDefault',
      'getUrlCollection',
      'addPassword',
    ]);

    /** @private {!PasswordManagerExpectations} */
    this.actual_ = new PasswordManagerExpectations();

    // Set these to have non-empty data.
    this.data = {
      passwords: [],
      exceptions: [],
      leakedCredentials: [],
      weakCredentials: [],
      checkStatus: makePasswordCheckStatus(),
    };

    // Holds the last callbacks so they can be called when needed/
    this.lastCallback = {
      addPasswordCheckStatusListener: null,
      addSavedPasswordListChangedListener: null,
      addExceptionListChangedListener: null,
      requestPlaintextPassword: null,
      addCompromisedCredentialsListener: null,
      addWeakCredentialsListener: null,
      addAccountStorageOptInStateListener: null,
    };

    /** @private {string} */
    this.plaintextPassword_ = '';

    /** @private {boolean} */
    this.isOptedInForAccountStorage_ = false;

    /** @private {boolean} */
    this.isAccountStoreDefault_ = false;

    /** @private {?chrome.passwordsPrivate.UrlCollection} */
    this.getUrlCollectionResponse_ = null;
  }

  /** @override */
  addSavedPasswordListChangedListener(listener) {
    this.actual_.listening.passwords++;
    this.lastCallback.addSavedPasswordListChangedListener = listener;
  }

  /** @override */
  removeSavedPasswordListChangedListener(listener) {
    this.actual_.listening.passwords--;
  }

  /** @override */
  getSavedPasswordList(callback) {
    this.actual_.requested.passwords++;
    callback(this.data.passwords);
  }

  /** @override */
  recordPasswordsPageAccessInSettings() {}

  /** @override */
  removeSavedPassword(id) {
    this.actual_.removed.passwords++;
    this.methodCalled('removeSavedPassword', id);
  }

  /** @override */
  movePasswordsToAccount(ids) {
    this.methodCalled('movePasswordsToAccount', ids);
  }

  /** @override */
  removeSavedPasswords(ids) {
    this.actual_.removed.passwords += ids.length;
    this.methodCalled('removeSavedPasswords', ids);
  }

  /** @override */
  addExceptionListChangedListener(listener) {
    this.actual_.listening.exceptions++;
    this.lastCallback.addExceptionListChangedListener = listener;
  }

  /** @override */
  removeExceptionListChangedListener(listener) {
    this.actual_.listening.exceptions--;
  }

  /** @override */
  getExceptionList(callback) {
    this.actual_.requested.exceptions++;
    callback(this.data.exceptions);
  }

  /** @override */
  removeException(id) {
    this.actual_.removed.exceptions++;
    this.methodCalled('removeException', id);
  }

  /** @override */
  removeExceptions(ids) {
    this.actual_.removed.exceptions += ids.length;
    this.methodCalled('removeExceptions', ids);
  }

  /** @override */
  requestPlaintextPassword(id, reason) {
    this.methodCalled('requestPlaintextPassword', {id, reason});
    return Promise.resolve(this.plaintextPassword_);
  }

  setPlaintextPassword(plaintextPassword) {
    this.plaintextPassword_ = plaintextPassword;
  }

  // Sets the return value of isOptedInForAccountStorage calls and notifies
  // the last added listener.
  setIsOptedInForAccountStorageAndNotify(optIn) {
    this.isOptedInForAccountStorage_ = optIn;
    if (this.lastCallback.addAccountStorageOptInStateListener) {
      this.lastCallback.addAccountStorageOptInStateListener(
          this.isOptedInForAccountStorage_);
    }
  }

  /** @override */
  addAccountStorageOptInStateListener(listener) {
    this.actual_.listening.accountStorageOptInState++;
    this.lastCallback.addAccountStorageOptInStateListener = listener;
  }

  /** @override */
  removeAccountStorageOptInStateListener(listener) {
    this.actual_.listening.accountStorageOptInState--;
  }

  /** @override */
  isOptedInForAccountStorage() {
    this.methodCalled('isOptedInForAccountStorage');
    this.actual_.requested.accountStorageOptInState++;
    return Promise.resolve(this.isOptedInForAccountStorage_);
  }

  /**
   * Verifies expectations.
   * @param {!PasswordManagerExpectations} expected
   */
  assertExpectations(expected) {
    const actual = this.actual_;

    assertEquals(expected.requested.passwords, actual.requested.passwords);
    assertEquals(expected.requested.exceptions, actual.requested.exceptions);
    assertEquals(
        expected.requested.plaintextPassword,
        actual.requested.plaintextPassword);
    assertEquals(
        expected.requested.accountStorageOptInState,
        actual.requested.accountStorageOptInState);

    assertEquals(expected.removed.passwords, actual.removed.passwords);
    assertEquals(expected.removed.exceptions, actual.removed.exceptions);

    assertEquals(expected.listening.passwords, actual.listening.passwords);
    assertEquals(expected.listening.exceptions, actual.listening.exceptions);
    assertEquals(
        expected.listening.accountStorageOptInState,
        actual.listening.accountStorageOptInState);
  }

  /** @override */
  startBulkPasswordCheck() {
    this.methodCalled('startBulkPasswordCheck');
    if (this.data.checkStatus.state ===
        chrome.passwordsPrivate.PasswordCheckState.NO_PASSWORDS) {
      return Promise.reject('error');
    }
    return Promise.resolve();
  }

  /** @override */
  stopBulkPasswordCheck() {
    this.methodCalled('stopBulkPasswordCheck');
  }

  /** @override */
  getCompromisedCredentials() {
    this.methodCalled('getCompromisedCredentials');
    return Promise.resolve(this.data.leakedCredentials);
  }

  /** @override */
  getWeakCredentials() {
    this.methodCalled('getWeakCredentials');
    return Promise.resolve(this.data.weakCredentials);
  }

  /** @override */
  getPasswordCheckStatus() {
    this.methodCalled('getPasswordCheckStatus');
    return Promise.resolve(this.data.checkStatus);
  }

  /** @override */
  addCompromisedCredentialsListener(listener) {
    this.lastCallback.addCompromisedCredentialsListener = listener;
  }

  /** @override */
  removeCompromisedCredentialsListener(listener) {}

  /** @override */
  addWeakCredentialsListener(listener) {
    this.lastCallback.addWeakCredentialsListener = listener;
  }

  /** @override */
  removeWeakCredentialsListener(listener) {}

  /** @override */
  addPasswordCheckStatusListener(listener) {
    this.lastCallback.addPasswordCheckStatusListener = listener;
  }

  /** @override */
  removePasswordCheckStatusListener(listener) {}

  /** @override */
  getPlaintextInsecurePassword(credential, reason) {
    this.methodCalled('getPlaintextInsecurePassword', {credential, reason});
    if (!this.plaintextPassword_) {
      return Promise.reject('Could not obtain plaintext password');
    }

    const newCredential =
        /** @type {chrome.passwordsPrivate.InsecureCredential} */ (
            Object.assign({}, credential));
    newCredential.password = this.plaintextPassword_;
    return Promise.resolve(newCredential);
  }

  /** @override */
  changeInsecureCredential(credential, newPassword) {
    this.methodCalled('changeInsecureCredential', {credential, newPassword});
    return Promise.resolve();
  }

  /** @override */
  removeInsecureCredential(insecureCredential) {
    this.methodCalled('removeInsecureCredential', insecureCredential);
  }

  /** @override */
  recordPasswordCheckInteraction(interaction) {
    this.methodCalled('recordPasswordCheckInteraction', interaction);
  }

  /** @override */
  recordPasswordCheckReferrer(referrer) {
    this.methodCalled('recordPasswordCheckReferrer', referrer);
  }

  /** @override */
  changeSavedPassword(ids, newUsername, newPassword) {
    this.methodCalled('changeSavedPassword', {ids, newUsername, newPassword});
    return Promise.resolve();
  }

  /**
   * Sets the value to be returned by isAccountStoreDefault.
   * @param {boolean} isDefault
   */
  setIsAccountStoreDefault(isDefault) {
    this.isAccountStoreDefault_ = isDefault;
  }

  /** @override */
  isAccountStoreDefault() {
    this.methodCalled('isAccountStoreDefault');
    return Promise.resolve(this.isAccountStoreDefault_);
  }

  /**
   * Sets the value to be returned by getUrlCollection.
   * @param {?chrome.passwordsPrivate.UrlCollection} urlCollection
   */
  setGetUrlCollectionResponse(urlCollection) {
    this.getUrlCollectionResponse_ = urlCollection;
  }

  /** @override */
  getUrlCollection(url) {
    this.methodCalled('getUrlCollection', url);
    return Promise.resolve(this.getUrlCollectionResponse_);
  }

  /** @override */
  addPassword(options) {
    this.methodCalled('addPassword', options);
    return Promise.resolve();
  }

  /** @override */
  addPasswordsFileExportProgressListener() {}

  /** @override */
  cancelExportPasswords() {}

  /** @override */
  exportPasswords() {}

  /** @override */
  importPasswords() {}

  /** @override */
  optInForAccountStorage() {}

  /** @override */
  removePasswordsFileExportProgressListener() {}

  /** @override */
  requestExportProgressStatus() {}

  /** @override */
  undoRemoveSavedPasswordOrException() {}
}
