// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
import {dedupingMixin, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CredentialsChangedListener, PasswordCheckStatusChangedListener, PasswordManagerImpl, PasswordManagerProxy} from './password_manager_proxy.js';

type Constructor<T> = new (...args: any[]) => T;

/**
 * This behavior bundles functionality required to get insecure credentials and
 * status of password check. It is used by <settings-password-check>
 * <passwords-section> and <settings-autofill-page>.
 */
export const PasswordCheckMixin = dedupingMixin(
    <T extends Constructor<PolymerElement>>(superClass: T): T&
    Constructor<PasswordCheckMixinInterface> => {
      class PasswordCheckMixin extends superClass {
        static get properties() {
          return {
            /**
             * The number of compromised passwords as a formatted string.
             */
            compromisedPasswordsCount: String,

            /**
             * The number of weak passwords as a formatted string.
             */
            weakPasswordsCount: String,

            /**
             * The number of insecure passwords as a formatted string.
             */
            insecurePasswordsCount: String,

            /**
             * An array of leaked passwords to display.
             */
            leakedPasswords: {
              type: Array,
              value: () => [],
            },

            /**
             * An array of weak passwords to display.
             */
            weakPasswords: {
              type: Array,
              value: () => [],
            },

            /**
             * The status indicates progress and affects banner, title and icon.
             */
            status: {
              type: Object,
              value: () =>
                  ({state: chrome.passwordsPrivate.PasswordCheckState.IDLE}),
            },

            /**
             * Stores whether the status was fetched from the backend.
             */
            isInitialStatus: {
              type: Boolean,
              value: true,
            },
          };
        }

        passwordManager: PasswordManagerProxy|null = null;
        leakedPasswords: Array<chrome.passwordsPrivate.InsecureCredential>;
        weakPasswords: Array<chrome.passwordsPrivate.InsecureCredential>;
        compromisedPasswordsCount: string;
        weakPasswordsCount: string;
        insecurePasswordsCount: string;
        status: chrome.passwordsPrivate.PasswordCheckStatus;
        isInitialStatus: boolean;

        private leakedCredentialsListener_: CredentialsChangedListener|null =
            null;
        private weakCredentialsListener_: CredentialsChangedListener|null =
            null;
        private statusChangedListener_: PasswordCheckStatusChangedListener|
            null = null;

        connectedCallback() {
          super.connectedCallback();

          this.statusChangedListener_ = status => {
            this.status = status;
            this.isInitialStatus = false;
          };

          this.leakedCredentialsListener_ = compromisedCredentials => {
            this.updateCompromisedPasswordList(compromisedCredentials);
            this.fetchPluralizedStrings_();
          };

          this.weakCredentialsListener_ = weakCredentials => {
            this.weakPasswords = weakCredentials;
            this.fetchPluralizedStrings_();
          };

          this.passwordManager = PasswordManagerImpl.getInstance();
          this.passwordManager!.getPasswordCheckStatus().then(
              this.statusChangedListener_);
          this.passwordManager!.getCompromisedCredentials().then(
              this.leakedCredentialsListener_);
          this.passwordManager!.getWeakCredentials().then(
              this.weakCredentialsListener_);

          this.passwordManager!.addPasswordCheckStatusListener(
              this.statusChangedListener_);
          this.passwordManager!.addCompromisedCredentialsListener(
              this.leakedCredentialsListener_);
          this.passwordManager!.addWeakCredentialsListener(
              this.weakCredentialsListener_);
        }

        disconnectedCallback() {
          super.disconnectedCallback();

          this.passwordManager!.removePasswordCheckStatusListener(
              assert(this.statusChangedListener_!));
          this.statusChangedListener_ = null;
          this.passwordManager!.removeCompromisedCredentialsListener(
              assert(this.leakedCredentialsListener_!));
          this.leakedCredentialsListener_ = null;
          this.passwordManager!.removeWeakCredentialsListener(
              assert(this.weakCredentialsListener_!));
          this.weakCredentialsListener_ = null;
        }

        /**
         * Helper that fetches pluralized strings corresponding to the number of
         * compromised, weak and insecure credentials.
         */
        private fetchPluralizedStrings_() {
          const proxy = PluralStringProxyImpl.getInstance();
          const compromised = this.leakedPasswords.length;
          const weak = this.weakPasswords.length;

          proxy.getPluralString('compromisedPasswords', compromised)
              .then(count => this.compromisedPasswordsCount = count);

          proxy.getPluralString('weakPasswords', weak)
              .then(count => this.weakPasswordsCount = count);

          (() => {
            if (compromised > 0 && weak > 0) {
              return proxy.getPluralStringTupleWithComma(
                  'safetyCheckPasswordsCompromised', compromised,
                  'safetyCheckPasswordsWeak', weak);
            }
            if (compromised > 0) {
              // Only compromised and no weak passwords.
              return proxy.getPluralString(
                  'safetyCheckPasswordsCompromised', compromised);
            }
            if (weak > 0) {
              // Only weak and no compromised passwords.
              return proxy.getPluralString('safetyCheckPasswordsWeak', weak);
            }
            // No security issues.
            return proxy.getPluralString('compromisedPasswords', 0);
          })().then(count => this.insecurePasswordsCount = count);
        }

        /**
         * Function to update compromised credentials in a proper way. New
         * entities should appear in the bottom.
         */
        private updateCompromisedPasswordList(
            newList: Array<chrome.passwordsPrivate.InsecureCredential>) {
          const oldList = this.leakedPasswords.slice();
          const map = new Map(newList.map(item => ([item.id, item])));

          const resultList: Array<chrome.passwordsPrivate.InsecureCredential> =
              [];

          for (const item of oldList) {
            // If element is present in newList
            if (map.has(item.id)) {
              // Replace old version with new
              resultList.push(map.get(item.id)!);
              map.delete(item.id);
            }
          }

          const addedResults = Array.from(map.values());
          addedResults.sort((lhs, rhs) => {
            // Phished passwords are always shown above leaked passwords.
            const isPhished =
                (cred: chrome.passwordsPrivate.InsecureCredential) =>
                    cred.compromisedInfo!.compromiseType !==
                chrome.passwordsPrivate.CompromiseType.LEAKED;
            if (isPhished(lhs) !== isPhished(rhs)) {
              return isPhished(lhs) ? -1 : 1;
            }

            // Sort by time only if the displayed elapsed time since compromise
            // is different.
            if (lhs.compromisedInfo!.elapsedTimeSinceCompromise !==
                rhs.compromisedInfo!.elapsedTimeSinceCompromise) {
              return rhs.compromisedInfo!.compromiseTime -
                  lhs.compromisedInfo!.compromiseTime;
            }

            // Otherwise sort by origin, or by username in case the origin is
            // the same.
            return lhs.formattedOrigin.localeCompare(rhs.formattedOrigin) ||
                lhs.username.localeCompare(rhs.username);
          });
          resultList.push(...addedResults);
          this.leakedPasswords = resultList;
        }
      }

      return PasswordCheckMixin;
    });

export interface PasswordCheckMixinInterface {
  passwordManager: PasswordManagerProxy|null;
  leakedPasswords: Array<chrome.passwordsPrivate.InsecureCredential>;
  weakPasswords: Array<chrome.passwordsPrivate.InsecureCredential>;
  compromisedPasswordsCount: string;
  weakPasswordsCount: string;
  insecurePasswordsCount: string;
  status: chrome.passwordsPrivate.PasswordCheckStatus;
  isInitialStatus: boolean;
}
