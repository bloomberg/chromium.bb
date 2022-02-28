// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs the Polymer Password Edit Dialog tests. */

// clang-format off
import 'chrome://settings/lazy_load.js';

import {isChromeOS, isLacros} from 'chrome://resources/js/cr.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PasswordManagerImpl} from 'chrome://settings/settings.js';
import {eventToPromise, flushTasks} from 'chrome://webui-test/test_util.js';

import {createMultiStorePasswordEntry, PasswordSectionElementFactory} from './passwords_and_autofill_fake_data.js';
import {TestPasswordManagerProxy} from './test_password_manager_proxy.js';

// clang-format on

/**
 * Helper function to test if an element is visible.
 */
function isElementVisible(element) {
  return element && !element.hidden;
}

/**
 * Helper function to test if all components of edit dialog are shown correctly.
 */
function assertEditDialogParts(passwordDialog) {
  assertEquals(
      passwordDialog.i18n('editPasswordTitle'),
      passwordDialog.$.title.textContent.trim());
  assertTrue(passwordDialog.$.websiteInput.readonly);
  assertFalse(passwordDialog.$.usernameInput.readonly);
  assertFalse(passwordDialog.$.passwordInput.readonly);
  assertTrue(passwordDialog.$.passwordInput.required);
  assertFalse(isElementVisible(passwordDialog.$.storePicker));
  assertTrue(!!passwordDialog.shadowRoot.querySelector('#showPasswordButton'));
  assertTrue(isElementVisible(passwordDialog.$.footnote));
  assertTrue(isElementVisible(passwordDialog.$.cancel));
  assertEquals(
      passwordDialog.i18n('save'),
      passwordDialog.$.actionButton.textContent.trim());
  assertFalse(passwordDialog.$.actionButton.disabled);
}

/**
 * Helper function to test if all components of details dialog are shown
 * correctly.
 */
function assertDetailsDialogParts(passwordDialog) {
  assertEquals(
      passwordDialog.i18n('passwordDetailsTitle'),
      passwordDialog.$.title.textContent.trim());
  assertTrue(passwordDialog.$.websiteInput.readonly);
  assertTrue(passwordDialog.$.usernameInput.readonly);
  assertTrue(passwordDialog.$.passwordInput.readonly);
  assertFalse(passwordDialog.$.passwordInput.required);
  assertFalse(isElementVisible(passwordDialog.$.storePicker));
  assertFalse(!!passwordDialog.shadowRoot.querySelector('#showPasswordButton'));
  assertFalse(isElementVisible(passwordDialog.$.footnote));
  assertFalse(isElementVisible(passwordDialog.$.cancel));
  assertEquals(
      passwordDialog.i18n('done'),
      passwordDialog.$.actionButton.textContent.trim());
  assertFalse(passwordDialog.$.actionButton.disabled);
}

/**
 * Helper function to test if all components of add dialog are shown correctly.
 */
function assertAddDialogParts(passwordDialog) {
  assertEquals(
      passwordDialog.i18n('addPasswordTitle'),
      passwordDialog.$.title.textContent.trim());
  assertFalse(passwordDialog.$.websiteInput.readonly);
  assertFalse(passwordDialog.$.usernameInput.readonly);
  assertFalse(passwordDialog.$.passwordInput.readonly);
  assertTrue(passwordDialog.$.passwordInput.required);
  assertFalse(isElementVisible(passwordDialog.$.storageDetails));
  assertTrue(!!passwordDialog.shadowRoot.querySelector('#showPasswordButton'));
  assertTrue(isElementVisible(passwordDialog.$.footnote));
  assertTrue(isElementVisible(passwordDialog.$.cancel));
  assertEquals(
      passwordDialog.i18n('save'),
      passwordDialog.$.actionButton.textContent.trim());
  assertTrue(passwordDialog.$.actionButton.disabled);
}

/**
 * Helper function to update website input and trigger validation.
 */
async function updateWebsiteInput(
    dialog, passwordManager, newValue, isValid = true) {
  const shouldGetUrlCollection = !!newValue.length;
  if (shouldGetUrlCollection) {
    passwordManager.resetResolver('getUrlCollection');
    passwordManager.setGetUrlCollectionResponse(
        isValid ? {
          // Matches fake data pattern in createPasswordEntry.
          origin: `http://${newValue}/login`,
          shown: newValue,
          link: `http://${newValue}/login`,
        } :
                  null);
  }

  dialog.$.websiteInput.value = newValue;
  dialog.$.websiteInput.dispatchEvent(new CustomEvent('input'));

  if (shouldGetUrlCollection) {
    const url = await passwordManager.whenCalled('getUrlCollection');
    assertEquals(newValue, url);
  }
}

/**
 * Helper function to test change saved password behavior.
 * @param {!PasswordEditDialogElement} editDialog
 * @param {!Array<number>} entryIds Ids to be called as a changeSavedPassword
 *     parameter.
 * @param {TestPasswordManagerProxy} passwordManager
 */
async function changeSavedPasswordTestHelper(
    editDialog, entryIds, passwordManager) {
  const NEW_USERNAME = 'new_username';
  const NEW_PASSWORD = 'new_password';

  // Empty password should be considered invalid and disable the save button.
  editDialog.$.passwordInput.value = '';
  assertTrue(editDialog.$.passwordInput.invalid);
  assertTrue(editDialog.$.actionButton.disabled);

  editDialog.$.usernameInput.value = NEW_USERNAME;
  editDialog.$.passwordInput.value = NEW_PASSWORD;
  assertFalse(editDialog.$.passwordInput.invalid);
  assertFalse(editDialog.$.actionButton.disabled);

  editDialog.$.actionButton.click();

  // Check that the changeSavedPassword is called with the right arguments.
  const {ids, newUsername, newPassword} =
      await passwordManager.whenCalled('changeSavedPassword');
  assertEquals(NEW_USERNAME, newUsername);
  assertEquals(NEW_PASSWORD, newPassword);

  assertEquals(entryIds.length, ids.length);
  entryIds.forEach(entryId => assertTrue(ids.includes(entryId)));
}

/**
 * Helper function to test add password behavior.
 * @param {!PasswordEditDialogElement} addDialog
 * @param {!TestPasswordManagerProxy} passwordManager
 * @param {boolean} expectedUseAccountStore True for account store, false for
 *     device store.
 */
async function addPasswordTestHelper(
    addDialog, passwordManager, expectedUseAccountStore) {
  const WEBSITE = 'example.com';
  const USERNAME = 'username';
  const PASSWORD = 'password';

  await updateWebsiteInput(addDialog, passwordManager, WEBSITE);
  addDialog.$.usernameInput.value = USERNAME;
  addDialog.$.passwordInput.value = PASSWORD;

  addDialog.$.actionButton.click();

  const {url, username, password, useAccountStore} =
      await passwordManager.whenCalled('addPassword');
  assertEquals(WEBSITE, url);
  assertEquals(USERNAME, username);
  assertEquals(PASSWORD, password);
  assertEquals(expectedUseAccountStore, useAccountStore);

  await eventToPromise('close', addDialog);
}

suite('PasswordEditDialog', function() {
  /** @type {TestPasswordManagerProxy} */
  let passwordManager = null;

  /** @type {PasswordSectionElementFactory} */
  let elementFactory = null;

  setup(function() {
    PolymerTest.clearBody();
    // Override the PasswordManagerImpl for testing.
    passwordManager = new TestPasswordManagerProxy();
    PasswordManagerImpl.setInstance(passwordManager);
    elementFactory = new PasswordSectionElementFactory(document);
  });

  test('hasCorrectInitialStateWhenViewFederatedCredential', function() {
    const federationEntry = createMultiStorePasswordEntry(
        {federationText: 'with chromium.org', username: 'bart', deviceId: 42});
    const passwordDialog =
        elementFactory.createPasswordEditDialog(federationEntry);
    assertDetailsDialogParts(passwordDialog);
    assertEquals(
        federationEntry.urls.link, passwordDialog.$.websiteInput.value);
    assertEquals(
        federationEntry.username, passwordDialog.$.usernameInput.value);
    assertEquals(
        federationEntry.federationText, passwordDialog.$.passwordInput.value);
    assertEquals('text', passwordDialog.$.passwordInput.type);
    assertEquals(
        passwordDialog.i18n('editPasswordFootnote', federationEntry.urls.shown),
        passwordDialog.$.footnote.innerText.trim());
  });

  test('hasCorrectInitialStateWhenEditPassword', function() {
    const commonEntry = createMultiStorePasswordEntry(
        {url: 'goo.gl', username: 'bart', accountId: 42});
    const passwordDialog = elementFactory.createPasswordEditDialog(commonEntry);
    assertEditDialogParts(passwordDialog);
    assertEquals(commonEntry.urls.link, passwordDialog.$.websiteInput.value);
    assertEquals(commonEntry.username, passwordDialog.$.usernameInput.value);
    assertEquals(commonEntry.password, passwordDialog.$.passwordInput.value);
    assertEquals('password', passwordDialog.$.passwordInput.type);
    assertEquals(
        passwordDialog.i18n('editPasswordFootnote', commonEntry.urls.shown),
        passwordDialog.$.footnote.innerText.trim());
  });

  test('changesPasswordForAccountId', function() {
    const accountEntry = createMultiStorePasswordEntry(
        {url: 'goo.gl', username: 'bart', accountId: 42});
    const editDialog = elementFactory.createPasswordEditDialog(accountEntry);

    return changeSavedPasswordTestHelper(
        editDialog, [accountEntry.accountId], passwordManager);
  });

  test('changesPasswordForDeviceId', function() {
    const deviceEntry = createMultiStorePasswordEntry(
        {url: 'goo.gl', username: 'bart', deviceId: 42});
    const editDialog = elementFactory.createPasswordEditDialog(deviceEntry);

    return changeSavedPasswordTestHelper(
        editDialog, [deviceEntry.deviceId], passwordManager);
  });

  test('changesPasswordForBothId', function() {
    const multiEntry = createMultiStorePasswordEntry(
        {url: 'goo.gl', username: 'bart', accountId: 41, deviceId: 42});
    const editDialog = elementFactory.createPasswordEditDialog(multiEntry);

    return changeSavedPasswordTestHelper(
        editDialog, [multiEntry.accountId, multiEntry.deviceId],
        passwordManager);
  });

  test('changeUsernameFailsWhenReused', function() {
    const accountPasswords = [
      createMultiStorePasswordEntry(
          {url: 'goo.gl', username: 'bart', accountId: 0}),
      createMultiStorePasswordEntry(
          {url: 'goo.gl', username: 'mark', accountId: 0})
    ];
    const editDialog = elementFactory.createPasswordEditDialog(
        accountPasswords[0], accountPasswords);

    editDialog.$.usernameInput.value = 'mark';
    assertTrue(editDialog.$.usernameInput.invalid);
    assertTrue(editDialog.$.actionButton.disabled);

    editDialog.$.usernameInput.value = 'new_mark';
    assertFalse(editDialog.$.usernameInput.invalid);
    assertFalse(editDialog.$.actionButton.disabled);

    return changeSavedPasswordTestHelper(
        editDialog, [accountPasswords[0].accountId], passwordManager);
  });

  test('changesUsernameWhenReusedForDifferentStore', async function() {
    const passwords = [
      createMultiStorePasswordEntry(
          {url: 'goo.gl', username: 'bart', accountId: 0}),
      createMultiStorePasswordEntry(
          {url: 'goo.gl', username: 'mark', deviceId: 0})
    ];
    const editDialog =
        elementFactory.createPasswordEditDialog(passwords[0], passwords);

    // Changing the username to the value which is present for different store
    // type should work.
    editDialog.$.usernameInput.value = 'mark';
    assertFalse(editDialog.$.usernameInput.invalid);
    assertFalse(editDialog.$.actionButton.disabled);
  });

  // Test verifies that the edit dialog informs the password is stored in the
  // account.
  test('showsStorageDetailsForAccountPassword', function() {
    const accountPassword = createMultiStorePasswordEntry(
        {url: 'goo.gl', username: 'bart', accountId: 42});
    const accountPasswordDialog =
        elementFactory.createPasswordEditDialog(accountPassword);

    // By default no message is displayed.
    assertTrue(accountPasswordDialog.$.storageDetails.hidden);

    // Display the message.
    accountPasswordDialog.isAccountStoreUser = true;
    flush();
    assertFalse(accountPasswordDialog.$.storageDetails.hidden);
    assertEquals(
        accountPasswordDialog.i18n('passwordStoredInAccount'),
        accountPasswordDialog.$.storageDetails.innerText);
  });

  // Test verifies that the edit dialog informs the password is stored on the
  // device.
  test('showsStorageDetailsForDevicePassword', function() {
    const devicePassword = createMultiStorePasswordEntry(
        {url: 'goo.gl', username: 'bart', deviceId: 42});
    const devicePasswordDialog =
        elementFactory.createPasswordEditDialog(devicePassword);

    // By default no message is displayed.
    assertTrue(devicePasswordDialog.$.storageDetails.hidden);

    // Display the message.
    devicePasswordDialog.isAccountStoreUser = true;
    flush();
    assertFalse(devicePasswordDialog.$.storageDetails.hidden);
    assertEquals(
        devicePasswordDialog.i18n('passwordStoredOnDevice'),
        devicePasswordDialog.$.storageDetails.innerText);
  });

  // Test verifies that the edit dialog informs the password is stored both on
  // the device and in the account.
  test('showsStorageDetailsForBothLocations', function() {
    const accountAndDevicePassword = createMultiStorePasswordEntry(
        {url: 'goo.gl', username: 'bart', deviceId: 42, accountId: 43});
    const accountAndDevicePasswordDialog =
        elementFactory.createPasswordEditDialog(accountAndDevicePassword);

    // By default no message is displayed.
    assertTrue(accountAndDevicePasswordDialog.$.storageDetails.hidden);

    // Display the message.
    accountAndDevicePasswordDialog.isAccountStoreUser = true;
    flush();
    assertFalse(accountAndDevicePasswordDialog.$.storageDetails.hidden);
    assertEquals(
        accountAndDevicePasswordDialog.i18n(
            'passwordStoredInAccountAndOnDevice'),
        accountAndDevicePasswordDialog.$.storageDetails.innerText);
  });

  test('hasCorrectInitialStateWhenAddPassword', function() {
    const addDialog = elementFactory.createPasswordEditDialog();
    assertAddDialogParts(addDialog);
    assertEquals(true, addDialog.$.websiteInput.autofocus);
    assertEquals('', addDialog.$.websiteInput.value);
    assertEquals('', addDialog.$.usernameInput.value);
    assertEquals('', addDialog.$.passwordInput.value);
    assertEquals('password', addDialog.$.passwordInput.type);
    assertEquals(
        addDialog.i18n('addPasswordFootnote'),
        addDialog.$.footnote.innerText.trim());
  });

  test('showsStorePickerForAccountStoreUserWhenAddPassword', function() {
    const addDialog = elementFactory.createPasswordEditDialog();
    addDialog.accountEmail = 'username@gmail.com';
    const picker = addDialog.$.storePicker;
    assertFalse(isElementVisible(picker));
    assertEquals(false, picker.autofocus);
    assertEquals(true, addDialog.$.websiteInput.autofocus);

    addDialog.isAccountStoreUser = true;
    flush();
    assertTrue(isElementVisible(picker));
    assertEquals(true, picker.autofocus);
    assertEquals(false, addDialog.$.websiteInput.autofocus);
    assertEquals(
        addDialog.i18n('addPasswordStoreOptionAccount', addDialog.accountEmail),
        picker.options[0].textContent.trim());
    assertEquals(
        addDialog.i18n('addPasswordStoreOptionDevice'),
        picker.options[1].textContent.trim());
  });

  test('checksRequiredFieldsWhenAddPassword', async function() {
    const addDialog = elementFactory.createPasswordEditDialog();
    await updateWebsiteInput(addDialog, passwordManager, 'url');
    addDialog.$.passwordInput.value = 'password';
    assertFalse(addDialog.$.websiteInput.invalid);
    assertFalse(addDialog.$.passwordInput.invalid);
    assertFalse(addDialog.$.actionButton.disabled);

    // Website is required.
    await updateWebsiteInput(addDialog, passwordManager, '');
    assertTrue(addDialog.$.websiteInput.invalid);
    assertTrue(addDialog.$.actionButton.disabled);

    await updateWebsiteInput(addDialog, passwordManager, 'url');
    assertFalse(addDialog.$.websiteInput.invalid);
    assertFalse(addDialog.$.actionButton.disabled);

    // Password is required.
    addDialog.$.passwordInput.value = '';
    assertTrue(addDialog.$.passwordInput.invalid);
    assertTrue(addDialog.$.actionButton.disabled);
  });

  test('validatesWebsiteWhenAddPassword', async function() {
    const addDialog = elementFactory.createPasswordEditDialog();
    await updateWebsiteInput(addDialog, passwordManager, 'valid url', true);
    addDialog.$.passwordInput.value = 'password';
    assertFalse(addDialog.$.websiteInput.invalid);
    assertFalse(addDialog.$.actionButton.disabled);

    await updateWebsiteInput(addDialog, passwordManager, 'invalid url', false);
    assertTrue(addDialog.$.websiteInput.invalid);
    assertTrue(addDialog.$.actionButton.disabled);
  });

  test(
      'selectsDeviceInStorePickerWhenAccountStoreIsNotDefault',
      async function() {
        passwordManager.setIsAccountStoreDefault(false);
        const addDialog = elementFactory.createPasswordEditDialog(
            null, [], /*isAccountStoreUser=*/ true);
        await passwordManager.whenCalled('isAccountStoreDefault');

        assertEquals(
            addDialog.storeOptionDeviceValue, addDialog.$.storePicker.value);
      });

  test(
      'selectsAccountInStorePickerWhenAccountStoreIsDefault', async function() {
        passwordManager.setIsAccountStoreDefault(true);
        const addDialog = elementFactory.createPasswordEditDialog(
            null, [], /*isAccountStoreUser=*/ true);
        await passwordManager.whenCalled('isAccountStoreDefault');

        assertEquals(
            addDialog.storeOptionAccountValue, addDialog.$.storePicker.value);
      });

  test('addsPasswordWhenNotAccountStoreUser', function() {
    const addDialog = elementFactory.createPasswordEditDialog();
    return addPasswordTestHelper(
        addDialog, passwordManager, /*expectedUseAccountStore=*/ false);
  });

  test('addsPasswordWhenAccountStoreUserAndAccountSelected', async function() {
    const addDialog = elementFactory.createPasswordEditDialog(
        null, [], /*isAccountStoreUser=*/ true);
    await passwordManager.whenCalled('isAccountStoreDefault');
    addDialog.$.storePicker.value = addDialog.storeOptionAccountValue;
    return addPasswordTestHelper(
        addDialog, passwordManager, /*expectedUseAccountStore=*/ true);
  });

  test('addsPasswordWhenAccountStoreUserAndDeviceSelected', async function() {
    const addDialog = elementFactory.createPasswordEditDialog(
        null, [], /*isAccountStoreUser=*/ true);
    await passwordManager.whenCalled('isAccountStoreDefault');
    addDialog.$.storePicker.value = addDialog.storeOptionDeviceValue;
    return addPasswordTestHelper(
        addDialog, passwordManager, /*expectedUseAccountStore=*/ false);
  });

  test('validatesUsernameWhenWebsiteOriginChanges', async function() {
    const passwords = [createMultiStorePasswordEntry(
        {url: 'website.com', username: 'username', accountId: 0})];
    const addDialog = elementFactory.createPasswordEditDialog(null, passwords);

    addDialog.$.usernameInput.value = 'username';
    assertFalse(addDialog.$.usernameInput.invalid);

    await updateWebsiteInput(addDialog, passwordManager, 'website.com');
    assertTrue(addDialog.$.usernameInput.invalid);

    await updateWebsiteInput(
        addDialog, passwordManager, 'different-website.com');
    assertFalse(addDialog.$.usernameInput.invalid);
  });

  test('validatesUsernameWhenUsernameChanges', async function() {
    const passwords = [createMultiStorePasswordEntry(
        {url: 'website.com', username: 'username', accountId: 0})];
    const addDialog = elementFactory.createPasswordEditDialog(null, passwords);

    await updateWebsiteInput(addDialog, passwordManager, 'website.com');
    assertFalse(addDialog.$.usernameInput.invalid);

    addDialog.$.usernameInput.value = 'username';
    assertTrue(addDialog.$.usernameInput.invalid);

    addDialog.$.usernameInput.value = 'different username';
    assertFalse(addDialog.$.usernameInput.invalid);
  });

  test('addPasswordFailsWhenUsernameReusedForAnyStore', async function() {
    const passwords = [
      createMultiStorePasswordEntry(
          {url: 'website.com', username: 'username1', accountId: 0}),
      createMultiStorePasswordEntry(
          {url: 'website.com', username: 'username2', deviceId: 0})
    ];
    const addDialog = elementFactory.createPasswordEditDialog(null, passwords);
    await updateWebsiteInput(addDialog, passwordManager, 'website.com');

    addDialog.$.usernameInput.value = 'username1';
    assertTrue(addDialog.$.usernameInput.invalid);

    addDialog.$.usernameInput.value = 'username2';
    assertTrue(addDialog.$.usernameInput.invalid);
  });

  test('validatesWebsiteHasTopLevelDomainOnFocusLoss', async function() {
    const addDialog = elementFactory.createPasswordEditDialog();
    addDialog.$.passwordInput.value = 'password';

    // TLD error doesn't appear if another website error is shown.
    await updateWebsiteInput(
        addDialog, passwordManager, 'invalid-without-TLD',
        /* isValid= */ false);
    addDialog.$.websiteInput.dispatchEvent(new CustomEvent('blur'));
    assertTrue(addDialog.$.websiteInput.invalid);
    assertEquals(
        addDialog.$.websiteInput.errorMessage,
        addDialog.i18n('notValidWebAddress'));
    assertTrue(addDialog.$.actionButton.disabled);

    // TLD error appears if no other website error.
    await updateWebsiteInput(
        addDialog, passwordManager, 'valid-without-TLD', /* isValid= */ true);
    addDialog.$.websiteInput.dispatchEvent(new CustomEvent('blur'));
    assertTrue(addDialog.$.websiteInput.invalid);
    assertEquals(
        addDialog.$.websiteInput.errorMessage,
        addDialog.i18n('missingTLD', 'valid-without-TLD.com'));
    assertTrue(addDialog.$.actionButton.disabled);

    // TLD error disappears on website input change.
    await updateWebsiteInput(
        addDialog, passwordManager, 'changed-without-TLD', /* isValid= */ true);
    assertFalse(addDialog.$.websiteInput.invalid);
    assertFalse(addDialog.$.actionButton.disabled);

    // TLD error doesn't appear if TLD is present.
    await updateWebsiteInput(
        addDialog, passwordManager, 'valid-with-TLD.com', /* isValid= */ true);
    addDialog.$.websiteInput.dispatchEvent(new CustomEvent('blur'));
    assertFalse(addDialog.$.websiteInput.invalid);
    assertFalse(addDialog.$.actionButton.disabled);
  });

  test(
      'requestsPlaintextPasswordAndSwitchesToEditModeOnViewPasswordClick',
      async function() {
        const existingEntry = createMultiStorePasswordEntry(
            {url: 'website.com', username: 'username', accountId: 0});
        const addDialog =
            elementFactory.createPasswordEditDialog(null, [existingEntry]);
        assertFalse(isElementVisible(addDialog.$.viewExistingPasswordLink));

        await updateWebsiteInput(
            addDialog, passwordManager, existingEntry.urls.shown);
        addDialog.$.usernameInput.value = existingEntry.username;
        assertTrue(isElementVisible(addDialog.$.viewExistingPasswordLink));

        existingEntry.password = 'plaintext password';
        passwordManager.setPlaintextPassword(existingEntry.password);
        addDialog.$.viewExistingPasswordLink.click();
        const {id, reason} =
            await passwordManager.whenCalled('requestPlaintextPassword');
        assertEquals(existingEntry.getAnyId(), id);
        assertEquals(chrome.passwordsPrivate.PlaintextReason.EDIT, reason);
        await flushTasks();

        assertEditDialogParts(addDialog);
        assertEquals(existingEntry.urls.link, addDialog.$.websiteInput.value);
        assertEquals(existingEntry.username, addDialog.$.usernameInput.value);
        assertEquals(existingEntry.password, addDialog.$.passwordInput.value);
      });

  // On ChromeOS/Lacros the behavior is different (on failure we request token
  // and retry).
  if (!isChromeOS && !isLacros) {
    test(
        'notSwitchToEditModeOnViewPasswordClickWhenRequestPlaintextPasswordFailed',
        async function() {
          const existingEntry = createMultiStorePasswordEntry(
              {url: 'website.com', username: 'username', accountId: 0});
          const addDialog =
              elementFactory.createPasswordEditDialog(null, [existingEntry]);
          assertFalse(isElementVisible(addDialog.$.viewExistingPasswordLink));

          await updateWebsiteInput(
              addDialog, passwordManager, existingEntry.urls.shown);
          addDialog.$.usernameInput.value = existingEntry.username;
          assertTrue(isElementVisible(addDialog.$.viewExistingPasswordLink));

          // By default requestPlaintextPassword fails if value not set.
          addDialog.$.viewExistingPasswordLink.click();
          const {id, reason} =
              await passwordManager.whenCalled('requestPlaintextPassword');
          assertEquals(existingEntry.getAnyId(), id);
          assertEquals(chrome.passwordsPrivate.PlaintextReason.EDIT, reason);
          await flushTasks();

          assertAddDialogParts(addDialog);
        });
  }
});
