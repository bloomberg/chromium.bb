// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, expect} from 'chai';
import type {BrowserAndPages} from '../../conductor/puppeteer-state.js';

import {click, getBrowserAndPages, pressKey, step, waitFor, waitForAria, waitForElementWithTextContent, waitForFunction} from '../../shared/helper.js';
import {describe, it} from '../../shared/mocha-extensions.js';
import {getAllRequestNames, navigateToNetworkTab, selectRequestByName, setCacheDisabled, setPersistLog, waitForSelectedRequestChange, waitForSomeRequestsToAppear} from '../helpers/network-helpers.js';

async function getRequestRowInfo(frontend: BrowserAndPages['frontend'], name: string) {
  const statusColumn = await frontend.evaluate(() => {
    return Array.from(document.querySelectorAll('.status-column')).map(node => node.textContent);
  });
  const timeColumn = await frontend.evaluate(() => {
    return Array.from(document.querySelectorAll('.time-column')).map(node => node.textContent);
  });
  const typeColumn = await frontend.evaluate(() => {
    return Array.from(document.querySelectorAll('.type-column')).map(node => node.textContent);
  });
  const nameColumn = await frontend.evaluate(() => {
    return Array.from(document.querySelectorAll('.name-column')).map(node => node.textContent);
  });
  const index = nameColumn.findIndex(x => x === name);
  return {status: statusColumn[index], time: timeColumn[index], type: typeColumn[index]};
}

describe('The Network Tab', async function() {
  if (this.timeout() !== 0.0) {
    // These tests take some time on slow windows machines.
    this.timeout(10000);
  }
  const formatByteSize = (value: number) => {
    return `${value}\xA0B`;
  };

  beforeEach(async () => {
    await navigateToNetworkTab('empty.html');
    await setCacheDisabled(true);
    await setPersistLog(false);
  });

  it('can click on checkbox label to toggle checkbox', async () => {
    await navigateToNetworkTab('resources-from-cache.html');

    // Click the label next to the checkbox input element
    await click('[aria-label="Disable cache"] + label');

    const checkbox = await waitFor('[aria-label="Disable cache"]');
    const checked = await checkbox.evaluate(box => (box as HTMLInputElement).checked);

    assert.strictEqual(checked, false, 'The disable cache checkbox should be unchecked');
  });

  it('shows Last-Modified', async () => {
    const {target, frontend} = getBrowserAndPages();
    await navigateToNetworkTab('last-modified.html');

    // Reload to populate network request table
    await target.reload({waitUntil: 'networkidle0'});

    await step('Wait for the column to show up and populate its values', async () => {
      await waitForSomeRequestsToAppear(2);
    });

    await step('Open the contextmenu for all network column', async () => {
      await click('.name-column', {clickOptions: {button: 'right'}});
    });

    await step('Click "Response Headers" submenu', async () => {
      const responseHeaders = await waitForAria('Response Headers');
      await click(responseHeaders);
    });

    await step('Enable the Last-Modified column in the network datagrid', async () => {
      const lastModified = await waitForAria('Last-Modified, unchecked');
      await click(lastModified);
    });

    await step('Wait for the "Last-Modified" column to have the expected values', async () => {
      const expectedValues = JSON.stringify(['Last-Modified', '', 'Sun, 26 Sep 2010 22:04:35 GMT']);
      await waitForFunction(async () => {
        const lastModifiedColumnValues = await frontend.$$eval(
            'pierce/.last-modified-column',
            cells => cells.map(element => element.textContent),
        );
        return JSON.stringify(lastModifiedColumnValues) === expectedValues;
      });
    });
  });

  it('the HTML response including cyrillic characters with utf-8 encoding', async () => {
    const {target} = getBrowserAndPages();
    await navigateToNetworkTab('utf-8.rawresponse');

    // Reload to populate network request table
    await target.reload({waitUntil: 'networkidle0'});

    // Wait for the column to show up and populate its values
    await waitForSomeRequestsToAppear(1);

    // Open the HTML file that was loaded
    await click('td.name-column');
    // Wait for the detailed network information pane to show up
    await waitFor('[aria-label="Response"]');
    // Open the raw response HTML
    await click('[aria-label="Response"]');
    // Wait for the raw response editor to show up
    const codeMirrorEditor = await waitFor('[aria-label="Code editor"]');

    const htmlRawResponse = await codeMirrorEditor.evaluate(editor => editor.textContent);

    assert.strictEqual(
        htmlRawResponse,
        '<html><body>The following word is written using cyrillic letters and should look like "SUCCESS": SU\u0421\u0421\u0415SS.</body></html>');
  });

  it('the correct MIME type when resources came from HTTP cache', async () => {
    const {target, frontend} = getBrowserAndPages();

    await navigateToNetworkTab('resources-from-cache.html');

    // Reload the page without a cache, to force a fresh load of the network resources
    await setCacheDisabled(true);
    await target.reload({waitUntil: 'networkidle0'});

    // Wait for the column to show up and populate its values
    await waitForSomeRequestsToAppear(2);

    // Get the size of the first two network request responses (excluding header and favicon.ico).
    const getNetworkRequestSize = () => frontend.evaluate(() => {
      return Array.from(document.querySelectorAll('.size-column')).slice(1, 3).map(node => node.textContent);
    });
    const getNetworkRequestMimeTypes = () => frontend.evaluate(() => {
      return Array.from(document.querySelectorAll('.type-column')).slice(1, 3).map(node => node.textContent);
    });

    assert.deepEqual(await getNetworkRequestSize(), [
      `${formatByteSize(404)}${formatByteSize(219)}`,
      `${formatByteSize(376)}${formatByteSize(28)}`,
    ]);
    assert.deepEqual(await getNetworkRequestMimeTypes(), [
      'document',
      'script',
    ]);

    // Allow resources from the cache again and reload the page to load from cache
    await setCacheDisabled(false);
    await target.reload({waitUntil: 'networkidle0'});
    // Wait for the column to show up and populate its values
    await waitForSomeRequestsToAppear(2);

    assert.deepEqual(await getNetworkRequestSize(), [
      `${formatByteSize(404)}${formatByteSize(219)}`,
      `(memory cache)${formatByteSize(28)}`,
    ]);

    assert.deepEqual(await getNetworkRequestMimeTypes(), [
      'document',
      'script',
    ]);
  });

  it('shows the correct initiator address space', async () => {
    const {target, frontend} = getBrowserAndPages();

    await navigateToNetworkTab('fetch.html');

    // Reload to populate network request table
    await target.reload({waitUntil: 'networkidle0'});

    await waitForSomeRequestsToAppear(2);

    await step('Open the contextmenu for all network column', async () => {
      await click('.name-column', {clickOptions: {button: 'right'}});
    });

    await step('Enable the Initiator Address Space column in the network datagrid', async () => {
      const initiatorAddressSpace = await waitForAria('Initiator Address Space, unchecked');
      await click(initiatorAddressSpace);
    });

    await step('Wait for the Initiator Address Space column to have the expected values', async () => {
      const expectedValues = JSON.stringify(['Initiator Address Space', '', 'Local']);
      await waitForFunction(async () => {
        const initiatorAddressSpaceValues = await frontend.$$eval(
            'pierce/.initiator-address-space-column',
            cells => cells.map(element => element.textContent),
        );
        return JSON.stringify(initiatorAddressSpaceValues) === expectedValues;
      });
    });
  });

  it('shows the correct remote address space', async () => {
    const {target, frontend} = getBrowserAndPages();

    await navigateToNetworkTab('fetch.html');

    // Reload to populate network request table
    await target.reload({waitUntil: 'networkidle0'});

    await waitForSomeRequestsToAppear(2);

    await step('Open the contextmenu for all network column', async () => {
      await click('.name-column', {clickOptions: {button: 'right'}});
    });

    await step('Enable the Remote Address Space column in the network datagrid', async () => {
      const remoteAddressSpace = await waitForAria('Remote Address Space, unchecked');
      await click(remoteAddressSpace);
    });

    await step('Wait for the Remote Address Space column to have the expected values', async () => {
      const expectedValues = JSON.stringify(['Remote Address Space', 'Local', 'Local']);
      await waitForFunction(async () => {
        const remoteAddressSpaceValues = await frontend.$$eval(
            'pierce/.remoteaddress-space-column',
            cells => cells.map(element => element.textContent),
        );
        return JSON.stringify(remoteAddressSpaceValues) === expectedValues;
      });
    });
  });

  it('indicates resources from the web bundle in the size column', async () => {
    const {target, frontend} = getBrowserAndPages();

    await navigateToNetworkTab('resources-from-webbundle.html');

    await target.reload({waitUntil: 'networkidle0'});

    await waitForSomeRequestsToAppear(3);
    await waitForElementWithTextContent(`(Web Bundle)${formatByteSize(27)}`);

    const getNetworkRequestSize = () => frontend.evaluate(() => {
      return Array.from(document.querySelectorAll('.size-column')).slice(2, 4).map(node => node.textContent);
    });

    assert.sameMembers(await getNetworkRequestSize(), [
      `${formatByteSize(653)}${formatByteSize(0)}`,
      `(Web Bundle)${formatByteSize(27)}`,
    ]);
  });

  it('shows web bundle metadata error in the status column', async () => {
    const {target, frontend} = getBrowserAndPages();

    await navigateToNetworkTab('resources-from-webbundle-with-bad-metadata.html');

    await target.reload({waitUntil: 'networkidle0'});

    await waitForSomeRequestsToAppear(3);
    await waitForElementWithTextContent('Web Bundle error');

    const getNetworkRequestStatus = () => frontend.evaluate(() => {
      return Array.from(document.querySelectorAll('.status-column')).slice(2, 4).map(node => node.textContent);
    });

    assert.sameMembers(await getNetworkRequestStatus(), ['Web Bundle error', '(failed)net::ERR_INVALID_WEB_BUNDLE']);
  });

  it('shows web bundle inner request error in the status column', async () => {
    const {target, frontend} = getBrowserAndPages();

    await navigateToNetworkTab('resources-from-webbundle-with-bad-inner-request.html');

    await target.reload({waitUntil: 'networkidle0'});

    await waitForSomeRequestsToAppear(3);
    await waitForElementWithTextContent('Web Bundle error');

    const getNetworkRequestSize = () => frontend.evaluate(() => {
      return Array.from(document.querySelectorAll('.status-column')).slice(2, 4).map(node => node.textContent);
    });

    assert.sameMembers(await getNetworkRequestSize(), ['200OK', 'Web Bundle error']);
  });

  it('shows web bundle icons', async () => {
    const {target, frontend} = getBrowserAndPages();

    await navigateToNetworkTab('resources-from-webbundle.html');

    await setCacheDisabled(true);
    await target.reload({waitUntil: 'networkidle0'});

    await waitForSomeRequestsToAppear(3);
    await waitFor('.name-column > [role="link"] > .icon');

    const getNetworkRequestIcons = () => frontend.evaluate(() => {
      return Array.from(document.querySelectorAll('.name-column > .icon'))
          .slice(1, 4)
          .map(node => (node as HTMLImageElement).alt);
    });
    assert.sameMembers(await getNetworkRequestIcons(), [
      'Script',
      'WebBundle',
    ]);
    const getFromWebBundleIcons = () => frontend.evaluate(() => {
      return Array.from(document.querySelectorAll('.name-column > [role="link"] > .icon'))
          .map(node => (node as HTMLImageElement).alt);
    });
    assert.sameMembers(await getFromWebBundleIcons(), [
      'Served from Web Bundle',
    ]);
  });

  it('shows preserved pending requests as unknown', async () => {
    const {target, frontend} = getBrowserAndPages();

    await navigateToNetworkTab('send_beacon_on_unload.html');

    await setCacheDisabled(true);
    await target.reload({waitUntil: 'networkidle0'});

    await waitForSomeRequestsToAppear(1);

    await setPersistLog(true);

    await navigateToNetworkTab('fetch.html');
    await waitForSomeRequestsToAppear(1);

    // We need to wait for the network log to update.
    await waitForFunction(async () => {
      const {status, time} = await getRequestRowInfo(frontend, 'sendBeacon');
      // Depending on timing of the reporting, the status infomation (404) might reach DevTools in time.
      return (status === '(unknown)' || status === '404') && time === '(unknown)';
    });
  });

  it('repeats xhr request on "r" shortcut when the request is focused', async () => {
    const {target} = getBrowserAndPages();

    await navigateToNetworkTab('xhr.html');
    await target.reload({waitUntil: 'networkidle0'});
    await waitForSomeRequestsToAppear(2);

    await selectRequestByName('image.svg');
    await waitForSelectedRequestChange(null);
    await pressKey('r');
    await waitForSomeRequestsToAppear(3);

    const updatedRequestNames = await getAllRequestNames();
    assert.deepStrictEqual(updatedRequestNames, ['xhr.html', 'image.svg', 'image.svg']);
  });

  it('shows the request panel when clicked during a websocket message (https://crbug.com/1222382)', async () => {
    await navigateToNetworkTab('websocket.html?infiniteMessages=true');

    await waitForSomeRequestsToAppear(2);

    // WebSocket messages get sent every 100 milliseconds, so holding the mouse
    // down for 300 milliseconds should suffice.
    await selectRequestByName('localhost', {delay: 300});

    await waitFor('.network-item-view');
  });

  // This is currently skipped while we fix the alignment of requestId+networkId in the CDP
  // events that apply to the main service worker request
  it.skip('[crbug.com/1304795] shows the main service worker request as complete', async () => {
    await navigateToNetworkTab('service-worker.html');
    const {target, frontend} = getBrowserAndPages();
    await target.waitForXPath('//div[@id="content" and text()="pong"]');
    const html = await getRequestRowInfo(frontend, 'service-worker.html/test/e2e/resources/network');
    expect(html).to.contain({
      status: '200OK',
      type: 'document',
    });
    const sw = await getRequestRowInfo(frontend, '⚙ service-worker.js/test/e2e/resources/network');
    expect(sw).to.contain({
      status: '200OK',
      type: 'script',
    });
  });
});
