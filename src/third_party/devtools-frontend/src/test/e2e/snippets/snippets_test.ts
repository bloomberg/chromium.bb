// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chai';

import {getBrowserAndPages, timeout, typeText, waitFor} from '../../shared/helper.js';
import {describe, it} from '../../shared/mocha-extensions.js';
import {getCurrentConsoleMessages} from '../helpers/console-helpers.js';
import {getAvailableSnippets, openCommandMenu, showSnippetsAutocompletion} from '../helpers/quick_open-helpers.js';
import {addSelectedTextToWatches, createNewSnippet, evaluateSelectedTextInConsole, getWatchExpressionsValues, openSnippetsSubPane, openSourcesPanel, runSnippet} from '../helpers/sources-helpers.js';

describe.skip('[crbug.com/1198160]: Snippet creation', () => {
  it('can show newly created snippets show up in command menu', async () => {
    const {frontend} = getBrowserAndPages();

    await openSourcesPanel();
    await openSnippetsSubPane();
    await createNewSnippet('New snippet');

    await openCommandMenu();
    await showSnippetsAutocompletion();

    assert.deepEqual(await getAvailableSnippets(), [
      'New snippet\u200B',
    ]);

    await typeText('New ');
    assert.deepEqual(await getAvailableSnippets(), [
      'New snippet\u200B',
    ]);

    await typeText('w');
    assert.deepEqual(await getAvailableSnippets(), []);

    await frontend.keyboard.press('Backspace');
    assert.deepEqual(await getAvailableSnippets(), [
      'New snippet\u200B',
    ]);
  });
});

describe.skip('[crbug.com/1198160]: Expression evaluation', () => {
  const message = '"Hello"';

  beforeEach(async () => {
    const {frontend} = getBrowserAndPages();
    await openSourcesPanel();
    await openSnippetsSubPane();
    await createNewSnippet('New snippet');
    await typeText(`(x => {debugger})(${message});`);
    await runSnippet();

    const functionParameterElement = await waitFor('.cm-js-def');
    const parameterElementPosition = await functionParameterElement.evaluate(elem => {
      const {x, y, right} = elem.getBoundingClientRect();
      return {x, y, right};
    });
    await frontend.mouse.move(parameterElementPosition.x, parameterElementPosition.y);
    await frontend.mouse.down();
    await frontend.mouse.move(parameterElementPosition.right, parameterElementPosition.y);
    await frontend.mouse.up();
  });

  afterEach(async () => {
    const {frontend} = getBrowserAndPages();
    await frontend.reload();
  });

  it('evaluates a selected expression in the console', async () => {
    await evaluateSelectedTextInConsole();
    // Prevent flakyness by awaiting some time for the text to be evaluated
    await timeout(200);
    const messages = await getCurrentConsoleMessages();
    assert.deepEqual(messages, [
      message,
    ]);
  });

  it('adds an expression to watches', async () => {
    await addSelectedTextToWatches();
    const watchExpressions = await getWatchExpressionsValues();
    assert.deepEqual(watchExpressions, [
      message,
    ]);
  });
});
