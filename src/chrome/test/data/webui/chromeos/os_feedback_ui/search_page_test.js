// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeSearchResponse} from 'chrome://os-feedback/fake_data.js';
import {FakeHelpContentProvider} from 'chrome://os-feedback/fake_help_content_provider.js';
import {HelpContentElement} from 'chrome://os-feedback/help_content.js';
import {setHelpContentProviderForTesting} from 'chrome://os-feedback/mojo_interface_provider.js';
import {OS_FEEDBACK_UNTRUSTED_ORIGIN, SearchPageElement} from 'chrome://os-feedback/search_page.js';

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
import {eventToPromise, flushTasks} from '../../test_util.js';

export function searchPageTestSuite() {
  /** @type {?SearchPageElement} */
  let page = null;

  /** @type {?FakeHelpContentProvider} */
  let provider = null;

  setup(() => {
    document.body.innerHTML = '';
    // Create provider.
    provider = new FakeHelpContentProvider();
    // Setup search response.
    provider.setFakeSearchResponse(fakeSearchResponse);
    // Set the fake provider.
    setHelpContentProviderForTesting(provider);
  });

  teardown(() => {
    page.remove();
    page = null;
    provider = null;
  });

  function initializePage() {
    assertFalse(!!page);
    page =
        /** @type {!SearchPageElement} */ (
            document.createElement('search-page'));
    assertTrue(!!page);
    document.body.appendChild(page);
    return flushTasks();
  }

  /** Test that expected html elements are in the page after loaded. */
  test('SearchPageLoaded', async () => {
    await initializePage();
    // Verify the title is in the page.
    const title = page.shadowRoot.querySelector('#title');
    assertTrue(!!title);
    assertEquals('Send feedback', title.textContent);

    // Verify the help content is not in the page. For security reason, help
    // contents fetched online can't be displayed in trusted context.
    const helpContent = page.shadowRoot.querySelector('help-content');
    assertFalse(!!helpContent);

    // Verify the iframe is in the page.
    const untrustedFrame = page.shadowRoot.querySelector('iframe');
    assertTrue(!!untrustedFrame);
    assertEquals(
        'chrome-untrusted://os-feedback/untrusted_index.html',
        untrustedFrame.src);

    // Verify the continue button is in the page.
    const btnContinue = page.shadowRoot.querySelector('#btnContinue');
    assertTrue(!!btnContinue);
  });

  /**
   * Test that the text area accepts input and may fire search query to retrieve
   * help contents.
   * - Case 1: When number of characters newly entered is less than 3, search is
   *   not triggered.
   * - Case 2: When number of characters newly entered is 3 or more, search is
   *   triggered and help contents are populated.
   */
  test('HelpContentPopulated', async () => {
    /** {?Element} */
    let textAreaElement = null;

    await initializePage();
    textAreaElement = page.shadowRoot.querySelector('#descriptionText');
    assertTrue(!!textAreaElement);
    // Verify the textarea is empty.
    assertEquals('', textAreaElement.value);

    // Enter three chars.
    textAreaElement.value = 'abc';
    // Setting the value of the textarea in code does not trigger the
    // input event. So we trigger it here.
    textAreaElement.dispatchEvent(new Event('input'));

    await flushTasks();
    // Verify that getHelpContent() has been called with query 'abc'.
    assertEquals('abc', provider.lastQuery);

    // Enter 2 more characters. This should NOT trigger another search.
    textAreaElement.value = 'abc12';
    textAreaElement.dispatchEvent(new Event('input'));

    await flushTasks();
    // Verify that getHelpContent() has NOT been called with query
    // 'abc12'.
    assertNotEquals('abc12', provider.lastQuery);

    // Enter one more characters. This should trigger another search.
    textAreaElement.value = 'abc123';
    textAreaElement.dispatchEvent(new Event('input'));

    await flushTasks();
    // Verify that getHelpContent() has been called with query 'abc123'.
    assertEquals('abc123', provider.lastQuery);
  });

  /**
   * Test that the search page can send help content to embedded untrusted page
   * via postMessage.
   */
  test('CanCommunicateWithUntrustedPage', async () => {
    /** Whether untrusted page has received new help contents. */
    let helpContentReceived = false;
    /** Number of help contents received by untrusted page. */
    let helpContentCountReceived = 0;

    await initializePage();

    const iframe = /** @type {!HTMLIFrameElement} */ (
        page.shadowRoot.querySelector('iframe'));
    assertTrue(!!iframe);
    // Wait for the iframe completes loading.
    await eventToPromise('load', iframe);

    window.addEventListener('message', (event) => {
      if (OS_FEEDBACK_UNTRUSTED_ORIGIN === event.origin &&
          'help-content-received-for-testing' === event.data.id) {
        helpContentReceived = true;
        helpContentCountReceived = event.data.count;
      }
    });

    const data = {
      response: fakeSearchResponse,
    };
    iframe.contentWindow.postMessage(data, OS_FEEDBACK_UNTRUSTED_ORIGIN);

    // Wait for the "help-content-received" message has been received.
    await eventToPromise('message', window);
    // Verify that help contents have been received.
    assertTrue(helpContentReceived);
    // Verify that 5 help contents have been received.
    assertEquals(5, helpContentCountReceived);
  });
}
