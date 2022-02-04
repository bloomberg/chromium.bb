// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {click, getBrowserAndPages, typeText, waitFor, waitForNone} from '../../shared/helper.js';
import {beforeEach, describe, it} from '../../shared/mocha-extensions.js';
import {CONSOLE_TAB_SELECTOR, CONSOLE_TOOLTIP_SELECTOR, focusConsolePrompt} from '../helpers/console-helpers.js';
import {openSourcesPanel} from '../helpers/sources-helpers.js';

describe('The Console Tab', async () => {
  beforeEach(async () => {
    const {frontend} = getBrowserAndPages();

    await click(CONSOLE_TAB_SELECTOR);
    await focusConsolePrompt();

    await typeText('let object = {a:1, b:2};');
    await frontend.keyboard.press('Enter');

    // Wait for the console to be usable again.
    await frontend.waitForFunction(() => {
      return document.querySelectorAll('.console-user-command-result').length === 1;
    });
  });

  afterEach(async () => {
    // Make sure we don't close DevTools while there is an outstanding
    // Runtime.evaluate CDP request, which causes an error. crbug.com/1134579.
    await openSourcesPanel();
  });

  // See the comments in console-repl-mode_test to see why this is necessary.
  async function objectAutocompleteTest(textAfterObject: string) {
    const {frontend} = getBrowserAndPages();

    const appearPromise = waitFor(CONSOLE_TOOLTIP_SELECTOR);
    await typeText('object');
    await appearPromise;

    const disappearPromise = waitForNone(CONSOLE_TOOLTIP_SELECTOR);
    await frontend.keyboard.press('Escape');
    await disappearPromise;

    const appearPromise2 = waitFor(CONSOLE_TOOLTIP_SELECTOR);
    await typeText(textAfterObject);
    await appearPromise2;

    // The first auto-suggest result is evaluated and generates a preview, which
    // we wait for so that we don't end the test/navigate with an open
    // Runtime.evaluate CDP request, which causes an error. crbug.com/1134579.
    await waitFor('.console-eager-inner-preview > span');
  }

  it('triggers autocompletion for `object.`', async () => {
    await objectAutocompleteTest('.');
  });

  it('triggers autocompletion for `object?.`', async () => {
    await objectAutocompleteTest('?.');
  });

  it('triggers autocompletion for `object[`', async () => {
    await objectAutocompleteTest('[');
  });
});
