// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview PasswordManagerProxy is an abstraction over
 * chrome.passwordsPrivate which facilitates testing.
 */

type InsecureCredentials = Array<chrome.passwordsPrivate.InsecureCredential>;
export type SavedPasswordListChangedListener =
    (entries: Array<chrome.passwordsPrivate.PasswordUiEntry>) => void;
export type PasswordExceptionListChangedListener =
    (entries: Array<chrome.passwordsPrivate.ExceptionEntry>) => void;
export type PasswordsFileExportProgressListener =
    (progress: chrome.passwordsPrivate.PasswordExportProgress) => void;
export type AccountStorageOptInStateChangedListener = (optInState: boolean) =>
    void;
export type CredentialsChangedListener = (credentials: InsecureCredentials) =>
    void;
export type PasswordCheckStatusChangedListener =
    (status: chrome.passwordsPrivate.PasswordCheckStatus) => void;

/**
 * Interface for all callbacks to the password API.
 */
export interface PasswordManagerProxy {
  /**
   * Add an observer to the list of saved passwords.
   */
  addSavedPasswordListChangedListener(
      listener: SavedPasswordListChangedListener): void;

  /**
   * Remove an observer from the list of saved passwords.
   */
  removeSavedPasswordListChangedListener(
      listener: SavedPasswordListChangedListener): void;

  /**
   * Request the list of saved passwords.
   * TODO(https://crbug.com/919483): Return a promise instead of taking a
   * callback argument.
   */
  getSavedPasswordList(callback: SavedPasswordListChangedListener): void;

  /**
   * Log that the Passwords page was accessed from the Chrome Settings WebUI.
   */
  recordPasswordsPageAccessInSettings(): void;

  /**
   * Requests whether the account store is a default location for saving
   * passwords. False means the device store is a default one. Must be called
   * when the current user has already opted-in for account storage.
   * @return A promise that resolves to whether the account store is default.
   */
  isAccountStoreDefault(): Promise<boolean>;

  /**
   * Requests whether the given |url| meets the requirements to save a password
   * for it (e.g. valid, has proper scheme etc.).
   * @return A promise that resolves to the corresponding URLCollection on
   *     success and to null otherwise.
   */
  getUrlCollection(url: string):
      Promise<chrome.passwordsPrivate.UrlCollection|null>;

  /**
   * Saves a new password entry described by the given |options|.
   * @param options Details about a new password and storage to be used.
   * @return A promise that resolves when the new entry is added.
   */
  addPassword(options: chrome.passwordsPrivate.AddPasswordOptions):
      Promise<void>;

  /**
   * Changes the saved password corresponding to |ids|.
   * @param ids The ids for the password entry being updated.
   * @return A promise that resolves when the password is updated for all ids.
   */
  changeSavedPassword(
      ids: Array<number>, newUsername: string,
      newPassword: string): Promise<void>;

  /**
   * Should remove the saved password and notify that the list has changed.
   * @param id The id for the password entry being removed. No-op if |id| is not
   *     in the list.
   */
  removeSavedPassword(id: number): void;

  /**
   * Should remove the saved passwords and notify that the list has changed.
   * @param ids The ids for the password entries being removed. Any id not in
   *    the list is ignored.
   */
  removeSavedPasswords(ids: Array<number>): void;

  /**
   * Moves a list of passwords from the device to the account
   * @param ids The ids for the password entries being moved.
   */
  movePasswordsToAccount(ids: Array<number>): void;

  /**
   * Add an observer to the list of password exceptions.
   */
  addExceptionListChangedListener(
      listener: PasswordExceptionListChangedListener): void;

  /**
   * Remove an observer from the list of password exceptions.
   */
  removeExceptionListChangedListener(
      listener: PasswordExceptionListChangedListener): void;

  /**
   * Request the list of password exceptions.
   * TODO(https://crbug.com/919483): Return a promise instead of taking a
   * callback argument.
   */
  getExceptionList(callback: PasswordExceptionListChangedListener): void;

  /**
   * Should remove the password exception and notify that the list has changed.
   * @param id The id for the exception url entry being removed. No-op if |id|
   *     is not in the list.
   */
  removeException(id: number): void;

  /**
   * Should remove the password exceptions and notify that the list has changed.
   * @param ids The ids for the exception url entries being removed. Any |id|
   *     not in the list is ignored.
   */
  removeExceptions(ids: Array<number>): void;

  /**
   * Should undo the last saved password or exception removal and notify that
   * the list has changed.
   */
  undoRemoveSavedPasswordOrException(): void;

  /**
   * Gets the saved password for a given login pair.
   * @param id The id for the password entry being being retrieved.
   * @param reason The reason why the plaintext password is requested.
   * @return A promise that resolves to the plaintext password.
   */
  requestPlaintextPassword(
      id: number,
      reason: chrome.passwordsPrivate.PlaintextReason): Promise<string>;

  /**
   * Triggers the dialogue for importing passwords.
   */
  importPasswords(): void;

  /**
   * Triggers the dialogue for exporting passwords.
   * TODO(https://crbug.com/919483): Return a promise instead of taking a
   * callback argument.
   */
  exportPasswords(callback: () => void): void;

  /**
   * Queries the status of any ongoing export.
   * TODO(https://crbug.com/919483): Return a promise instead of taking a
   * callback argument.
   */
  requestExportProgressStatus(
      callback: (status: chrome.passwordsPrivate.ExportProgressStatus) => void):
      void;

  /**
   * Add an observer to the export progress.
   */
  addPasswordsFileExportProgressListener(
      listener: PasswordsFileExportProgressListener): void;

  /**
   * Remove an observer from the export progress.
   */
  removePasswordsFileExportProgressListener(
      listener: PasswordsFileExportProgressListener): void;

  cancelExportPasswords(): void;

  /**
   * Add an observer to the account storage opt-in state.
   */
  addAccountStorageOptInStateListener(
      listener: AccountStorageOptInStateChangedListener): void;

  /**
   * Remove an observer to the account storage opt-in state.
   */
  removeAccountStorageOptInStateListener(
      listener: AccountStorageOptInStateChangedListener): void;

  /**
   * Requests the account-storage opt-in state of the current user.
   * @return A promise that resolves to the opt-in state.
   */
  isOptedInForAccountStorage(): Promise<boolean>;

  /**
   * Triggers the opt-in or opt-out flow for the account storage.
   * @param optIn Whether the user wants to opt in or opt out.
   */
  optInForAccountStorage(optIn: boolean): void;

  /**
   * Requests the start of the bulk password check.
   */
  startBulkPasswordCheck(): Promise<void>;

  /**
   * Requests to interrupt an ongoing bulk password check.
   */
  stopBulkPasswordCheck(): void;

  /**
   * Requests the latest information about compromised credentials.
   */
  getCompromisedCredentials(): Promise<InsecureCredentials>;

  /**
   * Requests the latest information about weak credentials.
   */
  getWeakCredentials(): Promise<InsecureCredentials>;

  /**
   * Requests the current status of the check.
   */
  getPasswordCheckStatus():
      Promise<chrome.passwordsPrivate.PasswordCheckStatus>;

  /**
   * Requests to remove |insecureCredential| from the password store.
   */
  removeInsecureCredential(
      insecureCredential: chrome.passwordsPrivate.InsecureCredential): void;

  /**
   * Add an observer to the compromised passwords change.
   */
  addCompromisedCredentialsListener(listener: CredentialsChangedListener): void;

  /**
   * Remove an observer to the compromised passwords change.
   */
  removeCompromisedCredentialsListener(listener: CredentialsChangedListener):
      void;

  /**
   * Add an observer to the weak passwords change.
   */
  addWeakCredentialsListener(listener: CredentialsChangedListener): void;

  /**
   * Remove an observer to the weak passwords change.
   */
  removeWeakCredentialsListener(listener: CredentialsChangedListener): void;

  /**
   * Add an observer to the passwords check status change.
   */
  addPasswordCheckStatusListener(listener: PasswordCheckStatusChangedListener):
      void;

  /**
   * Remove an observer to the passwords check status change.
   */
  removePasswordCheckStatusListener(
      listener: PasswordCheckStatusChangedListener): void;

  /**
   * Requests the plaintext password for |credential|. |callback| gets invoked
   * with the same |credential|, whose |password| field will be set.
   * @return A promise that resolves to the InsecureCredential with the password
   *     field populated.
   */
  getPlaintextInsecurePassword(
      credential: chrome.passwordsPrivate.InsecureCredential,
      reason: chrome.passwordsPrivate.PlaintextReason):
      Promise<chrome.passwordsPrivate.InsecureCredential>;

  /**
   * Requests to change the password of |credential| to |new_password|.
   * @return A promise that resolves when the password is updated.
   */
  changeInsecureCredential(
      credential: chrome.passwordsPrivate.InsecureCredential,
      newPassword: string): Promise<void>;

  /**
   * Records a given interaction on the Password Check page.
   */
  recordPasswordCheckInteraction(interaction: PasswordCheckInteraction): void;

  /**
   * Records the referrer of a given navigation to the Password Check page.
   */
  recordPasswordCheckReferrer(referrer: PasswordCheckReferrer): void;
}

/**
 * Represents different interactions the user can perform on the Password Check
 * page.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Needs to stay in sync with PasswordCheckInteraction in enums.xml and
 * password_manager_metrics_util.h.
 */
export enum PasswordCheckInteraction {
  START_CHECK_AUTOMATICALLY = 0,
  START_CHECK_MANUALLY = 1,
  STOP_CHECK = 2,
  CHANGE_PASSWORD = 3,
  EDIT_PASSWORD = 4,
  REMOVE_PASSWORD = 5,
  SHOW_PASSWORD = 6,
  // Must be last.
  COUNT = 7,
}

/**
 * Represents different referrers when navigating to the Password Check page.
 *
 * These values are persisted to logs. Entries should not be renumbered and
 * numeric values should never be reused.
 *
 * Needs to stay in sync with PasswordCheckReferrer in enums.xml and
 * password_check_referrer.h.
 */
export enum PasswordCheckReferrer {
  SAFETY_CHECK = 0,            // Web UI, recorded in JavaScript.
  PASSWORD_SETTINGS = 1,       // Web UI, recorded in JavaScript.
  PHISH_GUARD_DIALOG = 2,      // Native UI, recorded in C++.
  PASSWORD_BREACH_DIALOG = 3,  // Native UI, recorded in C++.
  // Must be last.
  COUNT = 4,
}

/**
 * Implementation that accesses the private API.
 */
export class PasswordManagerImpl implements PasswordManagerProxy {
  addSavedPasswordListChangedListener(listener:
                                          SavedPasswordListChangedListener) {
    chrome.passwordsPrivate.onSavedPasswordsListChanged.addListener(listener);
  }

  removeSavedPasswordListChangedListener(listener:
                                             SavedPasswordListChangedListener) {
    chrome.passwordsPrivate.onSavedPasswordsListChanged.removeListener(
        listener);
  }

  getSavedPasswordList(callback: SavedPasswordListChangedListener) {
    chrome.passwordsPrivate.getSavedPasswordList(callback);
  }

  recordPasswordsPageAccessInSettings() {
    chrome.passwordsPrivate.recordPasswordsPageAccessInSettings();
  }

  isAccountStoreDefault() {
    return new Promise<boolean>(resolve => {
      chrome.passwordsPrivate.isAccountStoreDefault(resolve);
    });
  }

  getUrlCollection(url: string) {
    return new Promise<chrome.passwordsPrivate.UrlCollection|null>(resolve => {
      chrome.passwordsPrivate.getUrlCollection(url, urlCollection => {
        resolve(chrome.runtime.lastError ? null : urlCollection);
      });
    });
  }

  addPassword(options: chrome.passwordsPrivate.AddPasswordOptions) {
    return new Promise<void>(resolve => {
      chrome.passwordsPrivate.addPassword(options, resolve);
    });
  }

  changeSavedPassword(
      ids: Array<number>, newUsername: string, newPassword: string) {
    return new Promise<void>(resolve => {
      chrome.passwordsPrivate.changeSavedPassword(
          ids, newUsername, newPassword, resolve);
    });
  }

  removeSavedPassword(id: number) {
    chrome.passwordsPrivate.removeSavedPassword(id);
  }

  removeSavedPasswords(ids: Array<number>) {
    chrome.passwordsPrivate.removeSavedPasswords(ids);
  }

  movePasswordsToAccount(ids: Array<number>) {
    chrome.passwordsPrivate.movePasswordsToAccount(ids);
  }

  addExceptionListChangedListener(listener:
                                      PasswordExceptionListChangedListener) {
    chrome.passwordsPrivate.onPasswordExceptionsListChanged.addListener(
        listener);
  }

  removeExceptionListChangedListener(listener:
                                         PasswordExceptionListChangedListener) {
    chrome.passwordsPrivate.onPasswordExceptionsListChanged.removeListener(
        listener);
  }

  getExceptionList(callback: PasswordExceptionListChangedListener) {
    chrome.passwordsPrivate.getPasswordExceptionList(callback);
  }

  removeException(id: number) {
    chrome.passwordsPrivate.removePasswordException(id);
  }

  removeExceptions(ids: Array<number>) {
    chrome.passwordsPrivate.removePasswordExceptions(ids);
  }

  undoRemoveSavedPasswordOrException() {
    chrome.passwordsPrivate.undoRemoveSavedPasswordOrException();
  }

  requestPlaintextPassword(
      id: number, reason: chrome.passwordsPrivate.PlaintextReason) {
    return new Promise<string>((resolve, reject) => {
      chrome.passwordsPrivate.requestPlaintextPassword(
          id, reason, (password) => {
            if (chrome.runtime.lastError) {
              reject(chrome.runtime.lastError.message);
              return;
            }

            resolve(password);
          });
    });
  }

  importPasswords() {
    chrome.passwordsPrivate.importPasswords();
  }

  exportPasswords(callback: () => void) {
    chrome.passwordsPrivate.exportPasswords(callback);
  }

  requestExportProgressStatus(
      callback:
          (status: chrome.passwordsPrivate.ExportProgressStatus) => void) {
    chrome.passwordsPrivate.requestExportProgressStatus(callback);
  }

  addPasswordsFileExportProgressListener(
      listener: PasswordsFileExportProgressListener) {
    chrome.passwordsPrivate.onPasswordsFileExportProgress.addListener(listener);
  }

  removePasswordsFileExportProgressListener(
      listener: PasswordsFileExportProgressListener) {
    chrome.passwordsPrivate.onPasswordsFileExportProgress.removeListener(
        listener);
  }

  cancelExportPasswords() {
    chrome.passwordsPrivate.cancelExportPasswords();
  }

  addAccountStorageOptInStateListener(
      listener: AccountStorageOptInStateChangedListener) {
    chrome.passwordsPrivate.onAccountStorageOptInStateChanged.addListener(
        listener);
  }

  removeAccountStorageOptInStateListener(
      listener: AccountStorageOptInStateChangedListener) {
    chrome.passwordsPrivate.onAccountStorageOptInStateChanged.removeListener(
        listener);
  }

  isOptedInForAccountStorage() {
    return new Promise<boolean>(resolve => {
      chrome.passwordsPrivate.isOptedInForAccountStorage(resolve);
    });
  }

  getPasswordCheckStatus() {
    return new Promise<chrome.passwordsPrivate.PasswordCheckStatus>(resolve => {
      chrome.passwordsPrivate.getPasswordCheckStatus(resolve);
    });
  }

  optInForAccountStorage(optIn: boolean) {
    chrome.passwordsPrivate.optInForAccountStorage(optIn);
  }

  startBulkPasswordCheck() {
    return new Promise<void>((resolve, reject) => {
      chrome.passwordsPrivate.startPasswordCheck(() => {
        if (chrome.runtime.lastError) {
          reject(chrome.runtime.lastError.message);
          return;
        }
        resolve();
      });
    });
  }

  stopBulkPasswordCheck() {
    chrome.passwordsPrivate.stopPasswordCheck();
  }

  getCompromisedCredentials() {
    return new Promise<InsecureCredentials>(resolve => {
      chrome.passwordsPrivate.getCompromisedCredentials(resolve);
    });
  }

  getWeakCredentials() {
    return new Promise<InsecureCredentials>(resolve => {
      chrome.passwordsPrivate.getWeakCredentials(resolve);
    });
  }

  removeInsecureCredential(insecureCredential:
                               chrome.passwordsPrivate.InsecureCredential) {
    chrome.passwordsPrivate.removeInsecureCredential(insecureCredential);
  }

  addCompromisedCredentialsListener(listener: CredentialsChangedListener) {
    chrome.passwordsPrivate.onCompromisedCredentialsChanged.addListener(
        listener);
  }

  removeCompromisedCredentialsListener(listener: CredentialsChangedListener) {
    chrome.passwordsPrivate.onCompromisedCredentialsChanged.removeListener(
        listener);
  }

  addWeakCredentialsListener(listener: CredentialsChangedListener) {
    chrome.passwordsPrivate.onWeakCredentialsChanged.addListener(listener);
  }

  removeWeakCredentialsListener(listener: CredentialsChangedListener) {
    chrome.passwordsPrivate.onWeakCredentialsChanged.removeListener(listener);
  }

  addPasswordCheckStatusListener(listener: PasswordCheckStatusChangedListener) {
    chrome.passwordsPrivate.onPasswordCheckStatusChanged.addListener(listener);
  }

  removePasswordCheckStatusListener(listener:
                                        PasswordCheckStatusChangedListener) {
    chrome.passwordsPrivate.onPasswordCheckStatusChanged.removeListener(
        listener);
  }

  getPlaintextInsecurePassword(
      credential: chrome.passwordsPrivate.InsecureCredential,
      reason: chrome.passwordsPrivate.PlaintextReason) {
    return new Promise<chrome.passwordsPrivate.InsecureCredential>(
        (resolve, reject) => {
          chrome.passwordsPrivate.getPlaintextInsecurePassword(
              credential, reason, credentialWithPassword => {
                if (chrome.runtime.lastError) {
                  reject(chrome.runtime.lastError.message);
                  return;
                }

                resolve(credentialWithPassword);
              });
        });
  }

  changeInsecureCredential(
      credential: chrome.passwordsPrivate.InsecureCredential,
      newPassword: string) {
    return new Promise<void>(resolve => {
      chrome.passwordsPrivate.changeInsecureCredential(
          credential, newPassword, resolve);
    });
  }

  /** override */
  recordPasswordCheckInteraction(interaction: PasswordCheckInteraction) {
    chrome.metricsPrivate.recordEnumerationValue(
        'PasswordManager.BulkCheck.UserAction', interaction,
        PasswordCheckInteraction.COUNT);
  }

  /** override */
  recordPasswordCheckReferrer(referrer: PasswordCheckReferrer) {
    chrome.metricsPrivate.recordEnumerationValue(
        'PasswordManager.BulkCheck.PasswordCheckReferrer', referrer,
        PasswordCheckReferrer.COUNT);
  }

  static getInstance(): PasswordManagerProxy {
    return instance || (instance = new PasswordManagerImpl());
  }

  static setInstance(obj: PasswordManagerProxy) {
    instance = obj;
  }
}

let instance: PasswordManagerProxy|null = null;
