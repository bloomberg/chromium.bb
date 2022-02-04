// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {CrSettingsPrefs, SettingsPrefsElement} from 'chrome://settings/settings.js';
import {assertEquals, assertNotEquals} from 'chrome://webui-test/chai_assert.js';

import {FakeSettingsPrivate} from './fake_settings_private.js';
import {prefsTestCases} from './prefs_test_cases.js';

// clang-format on

/** @fileoverview Suite of tests for settings-prefs. */
/**
 * Creates a deep copy of the object.
 */
function deepCopy(obj: Object): Object {
  return JSON.parse(JSON.stringify(obj));
}

suite('CrSettingsPrefs', function() {
  /**
   * Prefs instance created before each test.
   */
  let prefs: SettingsPrefsElement;

  let fakeApi: FakeSettingsPrivate;

  /**
   * @param {!Object} Pref store from <settings-prefs>.
   * @param {string} Pref key of the pref to return.
   */
  function getPrefFromKey(prefStore: any, key: string):
      chrome.settingsPrivate.PrefObject|undefined {
    const path = key.split('.');
    let pref = prefStore;
    for (const part of path) {
      pref = pref[part];
      if (!pref) {
        return undefined;
      }
    }
    return pref;
  }

  /**
   * Checks that the fake API pref store contains the expected values.
   * @param testCaseValueIndex The index of possible next values
   *     from the test case to check.
   */
  function assertFakeApiPrefsSet(testCaseValueIndex: number) {
    for (const testCase of prefsTestCases) {
      const expectedValue =
          JSON.stringify(testCase.nextValues[testCaseValueIndex]);
      const prefsObject =
          (fakeApi.prefs as
           {[key: string]:
                chrome.settingsPrivate.PrefObject})[testCase.pref.key]!;
      const actualValue = JSON.stringify(prefsObject.value);
      assertEquals(expectedValue, actualValue, testCase.pref.key);
    }
  }

  /**
   * Checks that the <settings-prefs> contains the expected values.
   * @param testCaseValueIndex The index of possible next values
   *     from the test case to check.
   */
  function assertPrefsSet(testCaseValueIndex: number) {
    for (const testCase of prefsTestCases) {
      const expectedValue =
          JSON.stringify(testCase.nextValues[testCaseValueIndex]);
      const actualValue =
          JSON.stringify(prefs.get('prefs.' + testCase.pref.key + '.value'));
      assertEquals(expectedValue, actualValue);
    }
  }

  // Initialize a <settings-prefs> before each test.
  setup(function() {
    document.body.innerHTML = '';

    // Override chrome.settingsPrivate with FakeSettingsPrivate.
    fakeApi = new FakeSettingsPrivate(prefsTestCases.map(function(testCase) {
      return testCase.pref;
    }));
    CrSettingsPrefs.deferInitialization = true;

    prefs = document.createElement('settings-prefs');
    document.body.appendChild(prefs);
    prefs.initialize(fakeApi as unknown as typeof chrome.settingsPrivate);

    // getAllPrefs is asynchronous, so return the prefs promise.
    return CrSettingsPrefs.initialized;
  });

  teardown(function() {
    CrSettingsPrefs.resetForTesting();
    CrSettingsPrefs.deferInitialization = false;
    prefs.resetForTesting();

    document.body.innerHTML = '';
  });

  test('receives and caches prefs', function testGetPrefs() {
    // Test that each pref has been successfully copied to the Polymer
    // |prefs| property.
    for (const key in fakeApi.prefs) {
      const expectedPref =
          (fakeApi.prefs as
           {[key: string]: chrome.settingsPrivate.PrefObject})[key];
      const actualPref = getPrefFromKey(prefs.prefs, key);
      assertNotEquals(undefined, actualPref);
      assertEquals(JSON.stringify(expectedPref), JSON.stringify(actualPref));
    }
  });

  test('forwards pref changes to API', function testSetPrefs() {
    // Test that settings-prefs uses the setPref API.
    for (const testCase of prefsTestCases) {
      prefs.set(
          'prefs.' + testCase.pref.key + '.value',
          deepCopy(testCase.nextValues[0]));
    }
    // Check that setPref has been called for the right values.
    assertFakeApiPrefsSet(0);

    // Test that when setPref fails, the pref is reverted locally.
    for (const testCase of prefsTestCases) {
      fakeApi.failNextSetPref();
      prefs.set(
          'prefs.' + testCase.pref.key + '.value',
          deepCopy(testCase.nextValues[1]));
    }

    assertPrefsSet(0);

    // Test that setPref is not called when the pref doesn't change.
    fakeApi.disallowSetPref();
    for (const testCase of prefsTestCases) {
      prefs.set(
          'prefs.' + testCase.pref.key + '.value',
          deepCopy(testCase.nextValues[0]));
    }
    assertFakeApiPrefsSet(0);
    fakeApi.allowSetPref();
  });

  test('responds to API changes', function testOnChange() {
    // Changes from the API should not result in those changes being sent
    // back to the API, as this could trigger a race condition.
    fakeApi.disallowSetPref();
    let prefChanges = [];
    for (const testCase of prefsTestCases) {
      prefChanges.push({key: testCase.pref.key, value: testCase.nextValues[0]});
    }

    // Send a set of changes.
    fakeApi.sendPrefChanges(prefChanges);
    assertPrefsSet(0);

    prefChanges = [];
    for (const testCase of prefsTestCases) {
      prefChanges.push({key: testCase.pref.key, value: testCase.nextValues[1]});
    }

    // Send a second set of changes.
    fakeApi.sendPrefChanges(prefChanges);
    assertPrefsSet(1);

    // Send the same set of changes again -- nothing should happen.
    fakeApi.sendPrefChanges(prefChanges);
    assertPrefsSet(1);
  });
});
