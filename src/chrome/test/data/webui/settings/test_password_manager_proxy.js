// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Test implementation of PasswordManagerProxy. */

/**
 * Test implementation
 * @implements {PasswordManagerProxy}
 * @constructor
 */
class TestPasswordManagerProxy extends TestBrowserProxy {
  constructor() {
    super(['getPlaintextPassword']);

    this.actual_ = new PasswordManagerExpectations();

    // Set these to have non-empty data.
    this.data = {
      passwords: [],
      exceptions: [],
    };

    // Holds the last callbacks so they can be called when needed/
    this.lastCallback = {
      addSavedPasswordListChangedListener: null,
      addExceptionListChangedListener: null,
      getPlaintextPassword: null,
    };

    this.plaintextPassword_ = '';
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

    if (this.onRemoveSavedPassword) {
      this.onRemoveSavedPassword(id);
    }
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

    if (this.onRemoveException) {
      this.onRemoveException(id);
    }
  }

  /** @override */
  getPlaintextPassword(id) {
    this.methodCalled('getPlaintextPassword', id);
    return Promise.resolve(this.plaintextPassword_);
  }

  setPlaintextPassword(plaintextPassword) {
    this.plaintextPassword_ = plaintextPassword;
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

    assertEquals(expected.removed.passwords, actual.removed.passwords);
    assertEquals(expected.removed.exceptions, actual.removed.exceptions);

    assertEquals(expected.listening.passwords, actual.listening.passwords);
    assertEquals(expected.listening.exceptions, actual.listening.exceptions);
  }
}
