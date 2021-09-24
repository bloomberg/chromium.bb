// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chai';

import {click, getBrowserAndPages, getTestServerPort, waitFor} from '../../shared/helper.js';
import {describe, it} from '../../shared/mocha-extensions.js';
import {getTrimmedTextContent, navigateToApplicationTab} from '../helpers/application-helpers.js';

const MANIFEST_SELECTOR = '[aria-label="Manifest"]';
const APP_ID_SELECTOR = '[aria-label="App Id"]';
const FIELD_NAMES_SELECTOR = '.report-field-name';
const FIELD_VALUES_SELECTOR = '.report-field-value';

// These tests are skipped because they do not pass in headless mode.
// The 'getAppId()' CDP method depends on the code for installable
// PWAs, which is only available in '/chrome', and therefore does not
// return a value in headless chromium.
// The tests are provided for local debugging only. They require debug
// mode and setting a chrome feature flag:
// npm run debug-e2etest -- -- --chrome-features=WebAppEnableManifestId
describe.skip('The Manifest Page', async () => {
  it('shows app id', async () => {
    const {target} = getBrowserAndPages();
    await navigateToApplicationTab(target, 'app-manifest-id');
    await click(MANIFEST_SELECTOR);
    await waitFor(APP_ID_SELECTOR);

    const fieldNames = await getTrimmedTextContent(FIELD_NAMES_SELECTOR);
    const fieldValues = await getTrimmedTextContent(FIELD_VALUES_SELECTOR);
    assert.strictEqual(fieldNames[3], 'App Id');
    assert.strictEqual(fieldValues[3], `https://localhost:${getTestServerPort()}/some_id`);
  });

  it('shows start id as app id', async () => {
    const {target} = getBrowserAndPages();
    await navigateToApplicationTab(target, 'app-manifest-no-id');
    await click(MANIFEST_SELECTOR);
    await waitFor(APP_ID_SELECTOR);

    const fieldNames = await getTrimmedTextContent(FIELD_NAMES_SELECTOR);
    const fieldValues = await getTrimmedTextContent(FIELD_VALUES_SELECTOR);
    assert.strictEqual(fieldNames[3], 'App Id');
    assert.strictEqual(
        fieldValues[3], `https://localhost:${getTestServerPort()}/test/e2e/resources/application/some_start_url`);
  });
});
