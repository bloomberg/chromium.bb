// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This just tests the interface. It does not test for specific results, only
// that callbacks are correctly invoked, expected parameters are correct,
// and failures are detected.


const COMPROMISE_TIME = 158322960000;

var availableTests = [
  function changeSavedPassword() {
    var numCalls = 0;
    var callback = function(savedPasswordsList) {
      numCalls++;
      if (numCalls == 1) {
        chrome.passwordsPrivate.changeSavedPassword(0, 'new_user');
      } else if (numCalls == 2) {
        chrome.test.assertEq('new_user', savedPasswordsList[0].username);
        chrome.passwordsPrivate.changeSavedPassword(
            0, 'another_user', 'new_pass');
      } else if (numCalls == 3) {
        chrome.test.assertEq('another_user', savedPasswordsList[0].username);
        chrome.test.succeed();
      } else {
        chrome.test.fail();
      }
    };

    chrome.passwordsPrivate.onSavedPasswordsListChanged.addListener(callback);
    chrome.passwordsPrivate.getSavedPasswordList(callback);
  },

  function removeAndUndoRemoveSavedPassword() {
    var numCalls = 0;
    var numSavedPasswords;
    var callback = function(savedPasswordsList) {
      numCalls++;

      if (numCalls == 1) {
        numSavedPasswords = savedPasswordsList.length;
        chrome.passwordsPrivate.removeSavedPassword(savedPasswordsList[0].id);
      } else if (numCalls == 2) {
        chrome.test.assertEq(savedPasswordsList.length, numSavedPasswords - 1);
        chrome.passwordsPrivate.undoRemoveSavedPasswordOrException();
      } else if (numCalls == 3) {
        chrome.test.assertEq(savedPasswordsList.length, numSavedPasswords);
        chrome.test.succeed();
      } else {
        chrome.test.fail();
      }
    };

    chrome.passwordsPrivate.onSavedPasswordsListChanged.addListener(callback);
    chrome.passwordsPrivate.getSavedPasswordList(callback);
  },

  function removeAndUndoRemovePasswordException() {
    var numCalls = 0;
    var numPasswordExceptions;
    var callback = function(passwordExceptionsList) {
      numCalls++;

      if (numCalls == 1) {
        numPasswordExceptions = passwordExceptionsList.length;
        chrome.passwordsPrivate.removePasswordException(
            passwordExceptionsList[0].id);
      } else if (numCalls == 2) {
        chrome.test.assertEq(
            passwordExceptionsList.length, numPasswordExceptions - 1);
        chrome.passwordsPrivate.undoRemoveSavedPasswordOrException();
      } else if (numCalls == 3) {
        chrome.test.assertEq(
            passwordExceptionsList.length, numPasswordExceptions);
        chrome.test.succeed();
      } else {
        chrome.test.fail();
      }
    };

    chrome.passwordsPrivate.onPasswordExceptionsListChanged.addListener(
        callback);
    chrome.passwordsPrivate.getPasswordExceptionList(callback);
  },

  function requestPlaintextPassword() {
    chrome.passwordsPrivate.requestPlaintextPassword(
        0, chrome.passwordsPrivate.PlaintextReason.VIEW, password => {
          // Ensure that the callback is invoked without an error state and the
          // expected plaintext password.
          chrome.test.assertNoLastError();
          chrome.test.assertEq('plaintext', password);
          chrome.test.succeed();
        });
  },

  function requestPlaintextPasswordFails() {
    chrome.passwordsPrivate.requestPlaintextPassword(
        123, chrome.passwordsPrivate.PlaintextReason.VIEW, password => {
          // Ensure that the callback is invoked with an error state and the
          // message contains the right id.
          chrome.test.assertLastError(
              'Could not obtain plaintext password. Either the user is not ' +
              'authenticated or no password with id = 123 could be found.');
          chrome.test.succeed();
        });
  },

  function getSavedPasswordList() {
    var callback = function(list) {
      chrome.test.assertTrue(!!list);
      chrome.test.assertTrue(list.length > 0);

      var idSet = new Set();
      for (var i = 0; i < list.length; ++i) {
        var entry = list[i];
        chrome.test.assertTrue(!!entry);
        chrome.test.assertTrue(!!entry.urls.origin);
        chrome.test.assertTrue(!!entry.urls.shown);
        chrome.test.assertTrue(!!entry.urls.link);
        idSet.add(entry.id);
      }

      // Ensure that all entry ids are unique.
      chrome.test.assertEq(list.length, idSet.size);
      // Ensure that the callback is invoked.
      chrome.test.succeed();
    };

    chrome.passwordsPrivate.getSavedPasswordList(callback);
  },

  function getPasswordExceptionList() {
    var callback = function(list) {
      chrome.test.assertTrue(!!list);
      chrome.test.assertTrue(list.length > 0);

      var idSet = new Set();
      for (var i = 0; i < list.length; ++i) {
        var exception = list[i];
        chrome.test.assertTrue(!!exception.urls.origin);
        chrome.test.assertTrue(!!exception.urls.shown);
        chrome.test.assertTrue(!!exception.urls.link);
        idSet.add(exception.id);
      }

      // Ensure that all exception ids are unique.
      chrome.test.assertEq(list.length, idSet.size);
      // Ensure that the callback is invoked.
      chrome.test.succeed();
    };

    chrome.passwordsPrivate.getPasswordExceptionList(callback);
  },

  function importPasswords() {
    chrome.passwordsPrivate.importPasswords();
    chrome.test.succeed();
  },

  function exportPasswords() {
    let callback = function() {
      chrome.test.assertNoLastError();

      // Ensure that the callback is invoked.
      chrome.test.succeed();
    };

    chrome.passwordsPrivate.exportPasswords(callback);
  },

  function cancelExportPasswords() {
    chrome.passwordsPrivate.cancelExportPasswords();
    chrome.test.succeed();
  },

  function requestExportProgressStatus() {
    let callback = function(status) {
      chrome.test.assertEq(
          chrome.passwordsPrivate.ExportProgressStatus.IN_PROGRESS, status);

      // Ensure that the callback is invoked.
      chrome.test.succeed();
    };

    chrome.passwordsPrivate.requestExportProgressStatus(callback);
  },

  function isNotOptedInForAccountStorage() {
    var callback = function(optedIn) {
      chrome.test.assertEq(optedIn, false);
      // Ensure that the callback is invoked.
      chrome.test.succeed();
    };

    chrome.passwordsPrivate.isOptedInForAccountStorage(callback);
  },

  function isOptedInForAccountStorage() {
    var callback = function(optedIn) {
      chrome.test.assertEq(optedIn, true);
      // Ensure that the callback is invoked.
      chrome.test.succeed();
    };

    chrome.passwordsPrivate.isOptedInForAccountStorage(callback);
  },

  function optInForAccountStorage() {
    chrome.passwordsPrivate.optInForAccountStorage(true);
    chrome.passwordsPrivate.isOptedInForAccountStorage(function(optedIn) {
      chrome.test.assertEq(optedIn, true);
      chrome.test.succeed();
    });
  },

  function optOutForAccountStorage() {
    chrome.passwordsPrivate.optInForAccountStorage(false);
    chrome.passwordsPrivate.isOptedInForAccountStorage(function(optedIn) {
      chrome.test.assertEq(optedIn, false);
      chrome.test.succeed();
    });
  },

  function getCompromisedCredentials() {
    chrome.passwordsPrivate.getCompromisedCredentials(
        compromisedCredentials => {
          chrome.test.assertEq(1, compromisedCredentials.length);

          var compromisedCredential = compromisedCredentials[0];
          chrome.test.assertEq(
              'example.com', compromisedCredential.formattedOrigin);
          chrome.test.assertEq(
              'https://example.com', compromisedCredential.detailedOrigin);
          chrome.test.assertFalse(compromisedCredential.isAndroidCredential);
          chrome.test.assertEq(
              'https://example.com/change-password',
              compromisedCredential.changePasswordUrl);
          chrome.test.assertEq('alice', compromisedCredential.username);
          const compromiseTime = new Date(compromisedCredential.compromiseTime);
          chrome.test.assertEq(
              'Tue, 03 Mar 2020 12:00:00 GMT', compromiseTime.toUTCString());
          chrome.test.assertEq(
              '3 days ago', compromisedCredential.elapsedTimeSinceCompromise);
          chrome.test.assertEq('LEAKED', compromisedCredential.compromiseType);
          chrome.test.succeed();
        });
  },

  function getPlaintextCompromisedPassword() {
    var compromisedCredential = {
      id: 0,
      formattedOrigin: 'example.com',
      detailedOrigin: 'https://example.com',
      isAndroidCredential: false,
      signonRealm: 'https://example.com',
      username: 'alice',
      compromiseTime: COMPROMISE_TIME,
      elapsedTimeSinceCompromise: '3 days ago',
      compromiseType: 'LEAKED',
    };

    chrome.passwordsPrivate.getPlaintextCompromisedPassword(
        compromisedCredential, chrome.passwordsPrivate.PlaintextReason.VIEW,
        credentialWithPassword => {
          chrome.test.assertEq('plaintext', credentialWithPassword.password);
          chrome.test.succeed();
        });
  },

  function getPlaintextCompromisedPasswordFails() {
    var compromisedCredential = {
      id: 0,
      formattedOrigin: 'example.com',
      detailedOrigin: 'https://example.com',
      isAndroidCredential: false,
      signonRealm: 'https://example.com',
      username: 'alice',
      compromiseTime: COMPROMISE_TIME,
      elapsedTimeSinceCompromise: '3 days ago',
      compromiseType: 'LEAKED',
    };

    chrome.passwordsPrivate.getPlaintextCompromisedPassword(
        compromisedCredential, chrome.passwordsPrivate.PlaintextReason.VIEW,
        credentialWithPassword => {
          chrome.test.assertLastError(
              'Could not obtain plaintext compromised password. Either the ' +
              'user is not authenticated or no matching password could be ' +
              'found.');
          chrome.test.succeed();
        });
  },

  function changeCompromisedCredentialFails() {
    chrome.passwordsPrivate.changeCompromisedCredential(
        {
          id: 0,
          formattedOrigin: 'example.com',
          detailedOrigin: 'https://example.com',
          isAndroidCredential: false,
          signonRealm: 'https://example.com',
          username: 'alice',
          compromiseTime: COMPROMISE_TIME,
          elapsedTimeSinceCompromise: '3 days ago',
          compromiseType: 'LEAKED',
        },
        'new_pass', () => {
          chrome.test.assertLastError(
              'Could not change the compromised credential. Either the user ' +
              'is not authenticated or no matching password could be found.');
          chrome.test.succeed();
        });
  },

  function changeCompromisedCredentialSucceeds() {
    chrome.passwordsPrivate.changeCompromisedCredential(
        {
          id: 0,
          formattedOrigin: 'example.com',
          detailedOrigin: 'https://example.com',
          isAndroidCredential: false,
          signonRealm: 'https://example.com',
          username: 'alice',
          compromiseTime: COMPROMISE_TIME,
          elapsedTimeSinceCompromise: '3 days ago',
          compromiseType: 'LEAKED',
        },
        'new_pass', () => {
          chrome.test.assertNoLastError();
          chrome.test.succeed();
        });
  },

  function removeCompromisedCredentialFails() {
    chrome.passwordsPrivate.removeCompromisedCredential(
        {
          id: 0,
          formattedOrigin: 'example.com',
          detailedOrigin: 'https://example.com',
          isAndroidCredential: false,
          signonRealm: 'https://example.com',
          username: 'alice',
          compromiseTime: COMPROMISE_TIME,
          elapsedTimeSinceCompromise: '3 days ago',
          compromiseType: 'LEAKED',
        },
        () => {
          chrome.test.assertLastError(
              'Could not remove the compromised credential. Probably no ' +
              'matching password could be found.');
          // Ensure that the callback is invoked.
          chrome.test.succeed();
        });
  },

  function removeCompromisedCredentialSucceeds() {
    chrome.passwordsPrivate.removeCompromisedCredential(
        {
          id: 0,
          formattedOrigin: 'example.com',
          detailedOrigin: 'https://example.com',
          isAndroidCredential: false,
          signonRealm: 'https://example.com',
          username: 'alice',
          compromiseTime: COMPROMISE_TIME,
          elapsedTimeSinceCompromise: '3 days ago',
          compromiseType: 'LEAKED',
        },
        () => {
          chrome.test.assertNoLastError();
          // Ensure that the callback is invoked.
          chrome.test.succeed();
        });
  },

  function startPasswordCheck() {
    chrome.passwordsPrivate.startPasswordCheck(() => {
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },

  function startPasswordCheckFailed() {
    chrome.passwordsPrivate.startPasswordCheck(() => {
      chrome.test.assertLastError('Starting password check failed.');
      chrome.test.succeed();
    });
  },

  function stopPasswordCheck() {
    chrome.passwordsPrivate.stopPasswordCheck(() => {
      chrome.test.assertNoLastError();
      chrome.test.succeed();
    });
  },

  function getPasswordCheckStatus() {
    chrome.passwordsPrivate.getPasswordCheckStatus(status => {
      chrome.test.assertEq('RUNNING', status.state);
      chrome.test.assertEq(5, status.alreadyProcessed);
      chrome.test.assertEq(10, status.remainingInQueue);
      chrome.test.assertEq('5 mins ago', status.elapsedTimeSinceLastCheck);
      chrome.test.succeed();
    });
  },
];

var testToRun = window.location.search.substring(1);
chrome.test.runTests(availableTests.filter(function(op) {
  return op.name == testToRun;
}));
