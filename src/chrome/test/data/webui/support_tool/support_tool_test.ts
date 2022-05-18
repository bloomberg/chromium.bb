// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Suite of tests for the Support Tool UI elements. It will be
 * executed by support_tool_browsertest.js.
 */

import 'chrome://support-tool/support_tool.js';
import 'chrome://support-tool/url_generator.js';

import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import {webUIListenerCallback} from 'chrome://resources/js/cr.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {BrowserProxy, BrowserProxyImpl, DataCollectorItem, IssueDetails, PIIDataItem, UrlGenerationResult} from 'chrome://support-tool/browser_proxy.js';
import {DataExportResult, SupportToolElement, SupportToolPageIndex} from 'chrome://support-tool/support_tool.js';
import {UrlGeneratorElement} from 'chrome://support-tool/url_generator.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';
import {waitAfterNextRender} from 'chrome://webui-test/test_util.js';

const EMAIL_ADDRESSES: string[] =
    ['testemail1@test.com', 'testemail2@test.com'];

const DATA_COLLECTORS: DataCollectorItem[] = [
  {name: 'data collector 1', isIncluded: false, protoEnum: 1},
  {name: 'data collector 2', isIncluded: true, protoEnum: 2},
  {name: 'data collector 3', isIncluded: false, protoEnum: 3},
];

const ALL_DATA_COLLECTORS: DataCollectorItem[] = [
  {name: 'data collector 1', isIncluded: false, protoEnum: 1},
  {name: 'data collector 2', isIncluded: false, protoEnum: 2},
  {name: 'data collector 3', isIncluded: false, protoEnum: 3},
  {name: 'data collector 4', isIncluded: false, protoEnum: 4},
  {name: 'data collector 5', isIncluded: false, protoEnum: 5},
];

const PII_ITEMS: PIIDataItem[] = [
  {
    piiTypeDescription: 'IP Address',
    piiType: 0,
    detectedData: '255.255.155.2, 255.255.155.255, 172.11.5.5',
    count: 3,
    keep: false,
    expandDetails: true
  },
  {
    piiTypeDescription: 'Hash',
    piiType: 1,
    detectedData: '27540283740a0897ab7c8de0f809add2bacde78f',
    count: 1,
    keep: false,
    expandDetails: true
  },
  {
    piiTypeDescription: 'URL',
    piiType: 2,
    detectedData:
        'chrome://resources/f?user=bar, chrome-extension://nkoccljplnhpfnfiajclkommnmllphnl/foobar.js?bar=x, http://tets.com',
    count: 3,
    keep: false,
    expandDetails: true
  }
];

/**
 * A test version of SupportToolBrowserProxy.
 * Provides helper methods for allowing tests to know when a method was called,
 * as well as specifying mock responses.
 */
class TestSupportToolBrowserProxy extends TestBrowserProxy implements
    BrowserProxy {
  private urlGenerationResult_:
      UrlGenerationResult = {success: false, url: '', errorMessage: ''};

  constructor() {
    super([
      'getEmailAddresses',
      'getDataCollectors',
      'startDataCollection',
      'cancelDataCollection',
      'startDataExport',
      'showExportedDataInFolder',
      'getAllDataCollectors',
      'generateCustomizedURL',
    ]);
  }

  getEmailAddresses() {
    this.methodCalled('getEmailAddresses');
    return Promise.resolve(EMAIL_ADDRESSES);
  }

  getDataCollectors() {
    this.methodCalled('getDataCollectors');
    return Promise.resolve(DATA_COLLECTORS);
  }

  getAllDataCollectors() {
    this.methodCalled('getAllDataCollectors');
    return Promise.resolve(ALL_DATA_COLLECTORS);
  }

  startDataCollection(
      issueDetails: IssueDetails, selectedDataCollectors: DataCollectorItem[]) {
    this.methodCalled(
        'startDataCollection', issueDetails, selectedDataCollectors);
    // Return result with success for testing.
    const result = {success: true, errorMessage: ''};
    return Promise.resolve(result);
  }

  cancelDataCollection() {
    this.methodCalled('cancelDataCollection');
  }

  startDataExport(piiDataItems: PIIDataItem[]) {
    this.methodCalled('startDataExport', [piiDataItems]);
  }

  showExportedDataInFolder() {
    this.methodCalled('showExportedDataInFolder');
  }

  setUrlGenerationResult(result: UrlGenerationResult) {
    this.urlGenerationResult_ = result;
  }

  // Returns this.urlGenerationResult as response. Please call
  // this.setUrlGenerationResult() before using this function in tests.
  generateCustomizedURL(caseId: string, dataCollectors: DataCollectorItem[]) {
    this.methodCalled('generateCustomizedURL', caseId, dataCollectors);
    return Promise.resolve(this.urlGenerationResult_);
  }
}


suite('SupportToolTest', function() {
  let supportTool: SupportToolElement;
  let browserProxy: TestSupportToolBrowserProxy;

  const strings = {
    caseId: 'testcaseid',
  };

  setup(async function() {
    loadTimeData.overrideValues(strings);
    document.body.innerHTML = '';
    browserProxy = new TestSupportToolBrowserProxy();
    BrowserProxyImpl.setInstance(browserProxy);
    supportTool = document.createElement('support-tool');
    document.body.appendChild(supportTool);
    await waitAfterNextRender(supportTool);
  });

  test('support tool pages navigation', () => {
    const ironPages = supportTool.shadowRoot!.querySelector('iron-pages');
    // The selected page index must be 0, which means initial page is
    // IssueDetails.
    assertEquals(ironPages!.selected, SupportToolPageIndex.ISSUE_DETAILS);
    // Only continue button container must be visible in initial page.
    assertFalse(
        supportTool.shadowRoot!.getElementById(
                                   'continueButtonContainer')!.hidden);
    // Click on continue button to move onto data collector selection page.
    supportTool.shadowRoot!.getElementById('continueButton')!.click();
    assertEquals(
        ironPages!.selected, SupportToolPageIndex.DATA_COLLECTOR_SELECTION);
    // Click on continue button to start data collection.
    supportTool.shadowRoot!.getElementById('continueButton')!.click();
    browserProxy.whenCalled('startDataCollection').then(function([
      issueDetails, selectedDataCollectors
    ]) {
      assertEquals(issueDetails.caseId, 'testcaseid');
      assertEquals(selectedDataCollectors, DATA_COLLECTORS);
    });
  });

  test('issue details page', () => {
    // Check the contents of data collectors page.
    const issueDetails = supportTool.$.issueDetails;
    assertEquals(
        issueDetails.shadowRoot!.querySelector('cr-input')!.value,
        'testcaseid');
    const emailOptions = issueDetails.shadowRoot!.querySelectorAll('option')!;
    // IssueDetailsElement adds empty string to the email addresses options as a
    // default value.
    assertEquals(EMAIL_ADDRESSES.length + 1, emailOptions.length);
  });

  test('data collector selection page', () => {
    // Check the contents of data collectors page.
    const ironListItems =
        supportTool.$.dataCollectors.shadowRoot!.querySelector(
                                                    'iron-list')!.items!;
    assertEquals(ironListItems.length, DATA_COLLECTORS.length);
    for (let i = 0; i < ironListItems.length; i++) {
      const listItem = ironListItems[i];
      assertEquals(listItem.name, DATA_COLLECTORS[i]!.name);
      assertEquals(listItem.isIncluded, DATA_COLLECTORS[i]!.isIncluded);
      assertEquals(listItem.protoEnum, DATA_COLLECTORS[i]!.protoEnum);
    }
  });

  test('spinner page', () => {
    // Check the contents of spinner page.
    const spinner = supportTool.$.spinnerPage;
    spinner.shadowRoot!.getElementById('cancelButton')!.click();
    browserProxy.whenCalled('cancelDataCollection').then(function() {
      webUIListenerCallback('data-collection-cancelled');
      flush();
      // Make sure the issue details page is displayed after cancelling data
      // collection.
      assertEquals(
          supportTool.shadowRoot!.querySelector('iron-pages')!.selected,
          SupportToolPageIndex.ISSUE_DETAILS);
    });
    assertEquals(browserProxy.getCallCount('cancelDataCollection'), 1);
  });

  test('PII selection page', async () => {
    // Go to the data collector selection page and start data collection by
    // clicking continue button twice so that the PII selection page gets
    // filled.
    supportTool.shadowRoot!.getElementById('continueButton')!.click();
    assertEquals(
        supportTool.shadowRoot!.querySelector('iron-pages')!.selected,
        SupportToolPageIndex.DATA_COLLECTOR_SELECTION);
    supportTool.shadowRoot!.getElementById('continueButton')!.click();
    // Check the contents of PII selection page.
    const piiSelection = supportTool.$.piiSelection;
    browserProxy.whenCalled('startDataCollection').then(function() {
      webUIListenerCallback('data-collection-completed', PII_ITEMS);
      flush();
      const items =
          piiSelection.shadowRoot!.querySelector('dom-repeat')!.items!;
      assertEquals(items, PII_ITEMS);
    });
    assertEquals(browserProxy.getCallCount('startDataCollection'), 1);
    piiSelection.shadowRoot!.getElementById('exportButton')!.click();
    await browserProxy.whenCalled('startDataExport');
    webUIListenerCallback('support-data-export-started');
    flush();
    assertEquals(
        supportTool.shadowRoot!.querySelector('iron-pages')!.selected,
        SupportToolPageIndex.EXPORT_SPINNER);
    const exportResult: DataExportResult = {
      success: true,
      path: '/usr/testuser/downloads/fake_support_packet_path.zip',
      error: ''
    };
    webUIListenerCallback('data-export-completed', exportResult);
    flush();
    assertEquals(
        supportTool.shadowRoot!.querySelector('iron-pages')!.selected,
        SupportToolPageIndex.DATA_EXPORT_DONE);
  });
});

suite('UrlGeneratorTest', function() {
  let urlGenerator: UrlGeneratorElement;
  let browserProxy: TestSupportToolBrowserProxy;

  setup(async function() {
    document.body.innerHTML = '';
    browserProxy = new TestSupportToolBrowserProxy();
    BrowserProxyImpl.setInstance(browserProxy);
    urlGenerator = document.createElement('url-generator');
    document.body.appendChild(urlGenerator);
    await waitAfterNextRender(urlGenerator);
  });

  test('url generation success', async () => {
    const caseIdInput = urlGenerator.shadowRoot!.getElementById(
                            'caseIdInput')! as CrInputElement;
    caseIdInput.value = 'test123';
    const dataCollectors =
        urlGenerator.shadowRoot!.querySelector('iron-list')!.items!;
    // Select the first one of data collectors.
    dataCollectors[0]!.selected = true;
    // Set the expected result of URL generation to successful.
    const expectedResult: UrlGenerationResult = {
      success: true,
      url: 'chrome://support-tool/?case_id=test123&module=jekhh',
      errorMessage: ''
    };
    browserProxy.setUrlGenerationResult(expectedResult);
    // Click the button to generate URL.
    urlGenerator.shadowRoot!.getElementById('generateButton')!.click();
    await browserProxy.whenCalled('generateCustomizedURL');
    // Check the URL value shown to user if it's as expected.
    const generatedURL = urlGenerator.shadowRoot!.getElementById(
                             'generatedURL')! as CrInputElement;
    assertEquals(generatedURL.value, expectedResult.url);
    // The input fields should be disabled when there's a generated URL shown to
    // user.
    assertTrue(caseIdInput.disabled);
    // Click the button to go back to URL generation.
    urlGenerator.shadowRoot!.getElementById('backButton')!.click();
    // The input fields should be enabled again when user clicked back button.
    assertFalse(caseIdInput.disabled);
    // Check the URL value shown to user is empty after going back.
    assertEquals(generatedURL.value, '');
  });

  test('url generation fail', async () => {
    // Set the expected result of URL generation to error.
    const expectedResult: UrlGenerationResult = {
      success: false,
      url: '',
      errorMessage: 'Test error message'
    };
    browserProxy.setUrlGenerationResult(expectedResult);
    // Click the button to generate URL.
    urlGenerator.shadowRoot!.getElementById('generateButton')!.click();
    await browserProxy.whenCalled('generateCustomizedURL');
    // Check that there's an error message shown to user.
    assertTrue(urlGenerator.$.errorMessageToast.open);
  });
});
