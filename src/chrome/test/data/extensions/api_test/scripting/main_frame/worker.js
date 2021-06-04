// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const NEW_TITLE_FROM_FUNCTION = 'Hello, world!';
const NEW_TITLE_FROM_FILE = 'Goodnight';
const EXACTLY_ONE_FILE_ERROR = 'Error: Exactly one file must be specified.';

function injectedFunction() {
  // NOTE(devlin): We currently need to (re)hard-code this title, since the
  // injected function won't keep the execution context from the surrounding
  // script.
  document.title = 'Hello, world!';
  return document.title;
}

function injectedFunctionWithArgument(newTitle) {
  document.title = newTitle;
  return document.title;
}

function echoArguments() {
  const args = Array.from(arguments);
  return args;
}

async function getSingleTab(query) {
  const tabs = await new Promise(resolve => {
    chrome.tabs.query(query, resolve);
  });
  chrome.test.assertEq(1, tabs.length);
  return tabs[0];
}

chrome.test.runTests([
  async function changeTitleFromFunction() {
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    const results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
      },
      func: injectedFunction,
    });
    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq(NEW_TITLE_FROM_FUNCTION, results[0].result);
    tab = await getSingleTab(query);
    chrome.test.assertEq(NEW_TITLE_FROM_FUNCTION, tab.title);
    chrome.test.succeed();
  },

  async function changeTitleWithCurriedArguments() {
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    const customNewTitle = 'Custom Title';
    const results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
      },
      func: injectedFunctionWithArgument,
      args: [customNewTitle],
    });
    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq(customNewTitle, results[0].result);
    tab = await getSingleTab(query);
    chrome.test.assertEq(customNewTitle, tab.title);
    chrome.test.succeed();
  },

  async function echoArgsOfDifferentTypes() {
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    const args = [
        42,
        0.07,
        'foo',
        true,
        [1, 2, 3],
        { key: 'value' },
        null,
    ];
    const results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
      },
      func: echoArguments,
      args: args,
    });
    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq(args, results[0].result);
    chrome.test.succeed();
  },

  async function nullInArgsIsNotPreserved() {
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    const args = [
        { key: 'value', nullKey: null },
    ];
    const results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
      },
      func: echoArguments,
      args: args,
    });
    chrome.test.assertEq(1, results.length);
    // Currently, null values in objects are not preserved. We should fix this,
    // but the IDL extension schema currently does not support the preserveNull
    // attribute, and adding it in for arrays is non-trivial.
    chrome.test.assertEq([{ key: 'value' }], results[0].result);
    chrome.test.succeed();
  },

  async function changeTitleFromFile() {
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    const results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
      },
      files: ['script_file.js'],
    });
    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq(NEW_TITLE_FROM_FILE, results[0].result);
    tab = await getSingleTab(query);
    chrome.test.assertEq(NEW_TITLE_FROM_FILE, tab.title);
    chrome.test.succeed();
  },

  async function injectedFunctionReturnsNothing() {
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    const results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
      },
      // Note: This function has no return statement; in JS, this means
      // the return value will be undefined.
      func: () => {},
    });
    chrome.test.assertEq(1, results.length);
    // NOTE: Undefined results are mapped to null in our bindings layer,
    // because they converted from empty base::Values in the same way.
    // NOTE AS WELL: We use `val === null` (rather than
    // `assertEq(null, val)` because assertEq will classify null and undefined
    // as equal.
    chrome.test.assertTrue(results[0].result === null);
    chrome.test.succeed();
  },

  async function injectedFunctionReturnsNull() {
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    const results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
      },
      func: () => {
        return null;
      },
    });
    chrome.test.assertEq(1, results.length);
    // NOTE: We use `val === null` (rather than `assertEq(null, val)` because
    // assertEq will classify null and undefined as equal.
    chrome.test.assertTrue(results[0].result === null);
    chrome.test.succeed();
  },

  async function injectedFunctionHasError() {
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    const results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
      },
      // This will throw a runtime error, since foo, bar, and baz aren't
      // defined.
      func: () => {
        foo.bar = baz;
        return 3;
      },
    });

    // TODO(devlin): Currently, we don't pass the error from the injected
    // script back to the extension in any way. It'd be helpful to pass
    // this along to the extension.
    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq(null, results[0].result);
    chrome.test.succeed();
  },

  // The `func` property used to be called `function`. This should still work
  // for backwards compatibility.
  async function usingOldFunctionPropertyNameWorks() {
    const changeTitleAgain = function() {
      document.title = 'Some New Title';
      return document.title;
    };
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    const results = await chrome.scripting.executeScript({
      target: {
        tabId: tab.id,
      },
      function: changeTitleAgain,
    });
    const newTitle = 'Some New Title';
    chrome.test.assertEq(1, results.length);
    chrome.test.assertEq(newTitle, results[0].result);
    tab = await getSingleTab(query);
    chrome.test.assertEq(newTitle, tab.title);
    chrome.test.succeed();
  },

  async function onlyOneOfFunctionAndFunc() {
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    await chrome.test.assertPromiseRejects(
        chrome.scripting.executeScript({
          target: {
            tabId: tab.id,
          },
          func: injectedFunction,
          function: injectedFunction,
        }),
        `Error: Both 'func' and 'function' were specified. ` +
        `Only 'func' should be used.`);
    chrome.test.succeed();
  },

  async function noSuchTab() {
    const nonExistentTabId = 99999;
    await chrome.test.assertPromiseRejects(
        chrome.scripting.executeScript({
          target: {
            tabId: nonExistentTabId,
          },
          func: injectedFunction,
        }),
        `Error: No tab with id: ${nonExistentTabId}`);
    chrome.test.succeed();
  },

  async function noSuchFile() {
    const noSuchFile = 'no_such_file.js';
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    await chrome.test.assertPromiseRejects(
        chrome.scripting.executeScript({
          target: {
            tabId: tab.id,
          },
          files: [noSuchFile],
        }),
        `Error: Could not load file: '${noSuchFile}'.`);
    chrome.test.succeed();
  },

  async function noFilesSpecified() {
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    await chrome.test.assertPromiseRejects(
        chrome.scripting.executeScript({
          target: {
            tabId: tab.id,
          },
          files: [],
        }),
        EXACTLY_ONE_FILE_ERROR);
    chrome.test.succeed();
  },

  async function multipleFilesSpecified() {
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    await chrome.test.assertPromiseRejects(
        chrome.scripting.executeScript({
          target: {
            tabId: tab.id,
          },
          files: ['script_file.js', 'script_file2.js'],
        }),
        EXACTLY_ONE_FILE_ERROR);
    chrome.test.succeed();
  },

  async function disallowedPermission() {
    const query = {url: 'http://chromium.org/*'};
    let tab = await getSingleTab(query);
    const expectedTitle = 'Title Of Awesomeness';
    chrome.test.assertEq(expectedTitle, tab.title);
    await chrome.test.assertPromiseRejects(
        chrome.scripting.executeScript({
          target: {
            tabId: tab.id,
          },
          func: injectedFunction,
        }),
        `Error: Cannot access contents of url "${tab.url}". ` +
            'Extension manifest must request permission ' +
            'to access this host.');
    tab = await getSingleTab(query);
    chrome.test.assertEq(expectedTitle, tab.title);
    chrome.test.succeed();
  },

  async function unserializableCurriedArguments() {
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    const expectedError =
        'Error in invocation of scripting.executeScript(' +
        'scripting.ScriptInjection injection, optional function callback): ' +
        'Error at parameter \'injection\': Error at property \'args\': ' +
        'Error at index 0: Value is unserializable.';
    chrome.test.assertThrows(
        chrome.scripting.executeScript,
        [{
          target: {
            tabId: tab.id,
          },
          func: echoArguments,
          args: [function() {}],
        }],
        expectedError);
    chrome.test.succeed();
  },

  async function argsPassedWithFiles() {
    const query = {url: 'http://example.com/*'};
    let tab = await getSingleTab(query);
    const expectedError =
    await chrome.test.assertPromiseRejects(
        chrome.scripting.executeScript({
          target: {
            tabId: tab.id,
          },
          files: ['script_file.js'],
          args: ['foo'],
        }),
        `Error: 'args' may not be used with file injections.`);
    chrome.test.succeed();
  }
]);
