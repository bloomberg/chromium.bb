// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';

import {PluralStringProxyImpl} from '../plural_string_proxy.js';

import {PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';

/**
 * This behavior bundles functionality required to get compromised credentials
 * and status of password check.
 * It is used by <settings-password-check> <passwords-section> and
 * <settings-autofill-page>.
 *
 * @polymerBehavior
 */

export const PasswordCheckBehavior = {

  properties: {
    /**
     * The number of compromised passwords as a formatted string.
     */
    compromisedPasswordsCount: String,

    /**
     * An array of leaked passwords to display.
     * @type {!Array<!PasswordManagerProxy.CompromisedCredential>}
     */
    leakedPasswords: {
      type: Array,
      value: () => [],
    },

    /**
     * The status indicates progress and affects banner, title and icon.
     * @type {!PasswordManagerProxy.PasswordCheckStatus}
     */
    status: {
      type: Object,
      value: () => ({state: chrome.passwordsPrivate.PasswordCheckState.IDLE}),
    },
  },

  /**
   * @private {?function(!PasswordManagerProxy.CompromisedCredentials):void}
   */
  leakedCredentialsListener_: null,

  /**
   * @private {?function(!PasswordManagerProxy.PasswordCheckStatus):void}
   */
  statusChangedListener_: null,

  /** @type {?PasswordManagerProxy} */
  passwordManager: null,

  /** @override */
  attached() {
    this.statusChangedListener_ = status => this.status = status;
    this.leakedCredentialsListener_ = compromisedCredentials => {
      this.updateCompromisedPasswordList(compromisedCredentials);

      PluralStringProxyImpl.getInstance()
          .getPluralString('compromisedPasswords', this.leakedPasswords.length)
          .then(count => {
            this.compromisedPasswordsCount = count;
          });
    };

    this.passwordManager = PasswordManagerImpl.getInstance();
    this.passwordManager.getPasswordCheckStatus().then(
        this.statusChangedListener_);
    this.passwordManager.getCompromisedCredentials().then(
        this.leakedCredentialsListener_);

    this.passwordManager.addPasswordCheckStatusListener(
        this.statusChangedListener_);
    this.passwordManager.addCompromisedCredentialsListener(
        this.leakedCredentialsListener_);
  },

  /** @override */
  detached() {
    this.passwordManager.removeCompromisedCredentialsListener(
        assert(this.statusChangedListener_));
    this.statusChangedListener_ = null;
    this.passwordManager.removeCompromisedCredentialsListener(
        assert(this.leakedCredentialsListener_));
    this.leakedCredentialsListener_ = null;
  },

  /**
   * Function to update compromised credentials in a proper way. New entities
   * should appear in the bottom.
   * @param {!Array<!PasswordManagerProxy.CompromisedCredential>} newList
   * @private
   */
  updateCompromisedPasswordList(newList) {
    const oldList = this.leakedPasswords.slice();
    const map = new Map(newList.map(item => ([item.id, item])));

    const resultList = [];

    for (const item of oldList) {
      // If element is present in newList
      if (map.has(item.id)) {
        // Replace old version with new
        resultList.push(map.get(item.id));
        map.delete(item.id);
      }
    }

    const addedResults = Array.from(map.values());
    addedResults.sort((lhs, rhs) => {
      // Phished passwords are always shown above leaked passwords.
      const isPhished = cred =>
          cred.compromiseType !== chrome.passwordsPrivate.CompromiseType.LEAKED;
      if (isPhished(lhs) != isPhished(rhs)) {
        return isPhished(lhs) ? -1 : 1;
      }

      // Sort by time only if the displayed elapsed time since compromise is
      // different.
      if (lhs.elapsedTimeSinceCompromise != rhs.elapsedTimeSinceCompromise) {
        return rhs.compromiseTime - lhs.compromiseTime;
      }

      // Otherwise sort by origin, or by username in case the origin is the
      // same.
      return lhs.formattedOrigin.localeCompare(rhs.formattedOrigin) ||
          lhs.username.localeCompare(rhs.username);
    });
    resultList.push(...addedResults);
    this.leakedPasswords = resultList;
  },
};
