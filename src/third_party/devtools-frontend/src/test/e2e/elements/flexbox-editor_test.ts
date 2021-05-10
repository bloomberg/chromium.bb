// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chai';

import {$$, enableExperiment, getBrowserAndPages, goToResource, waitFor} from '../../shared/helper.js';
import {describe, it} from '../../shared/mocha-extensions.js';
import {getCSSPropertyInRule, waitForContentOfSelectedElementsNode, waitForCSSPropertyValue} from '../helpers/elements-helpers.js';

describe('Flexbox Editor', async function() {
  beforeEach(async function() {
    const {frontend} = getBrowserAndPages();
    await enableExperiment('cssFlexboxFeatures');
    await goToResource('elements/flexbox-editor.html');
    await waitForContentOfSelectedElementsNode('<body>\u200B');
    await frontend.keyboard.press('ArrowDown');
    await waitForCSSPropertyValue('#target', 'display', 'flex');
  });

  async function clickFlexboxEditorButton() {
    const flexboxEditorButtons = await $$('[title="Open Flexbox editor"]');
    assert.deepEqual(flexboxEditorButtons.length, 1);
    const flexboxEditorButton = flexboxEditorButtons[0];
    flexboxEditorButton.click();
    await waitFor('devtools-flexbox-editor');
  }

  async function clickFlexEditButton(selector: string) {
    const buttons = await $$(selector);
    assert.strictEqual(buttons.length, 1);
    const button = buttons[0];
    button.click();
  }

  it('can be opened and flexbox styles can be edited', async () => {
    await clickFlexboxEditorButton();

    // Clicking once sets the value.
    await clickFlexEditButton('[title="Add flex-direction: column"]');
    await waitForCSSPropertyValue('#target', 'flex-direction', 'column');

    // Clicking again removes the value.
    await clickFlexEditButton('[title="Remove flex-direction: column"]');
    // Wait for the button's title to be updated so that we know the change
    // was made.
    await waitFor('[title="Add flex-direction: column"]');
    const property = await getCSSPropertyInRule('#target', 'flex-direction');
    assert.isUndefined(property);
  });
});
