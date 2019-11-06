// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for activity-log-item. */
suite('ExtensionsActivityLogStreamTest', function() {
  /**
   * Backing extension id, same id as the one in
   * extension_test_util.createExtensionInfo
   * @type {string}
   */
  const EXTENSION_ID = 'a'.repeat(32);

  const activity1 = {
    extensionId: EXTENSION_ID,
    activityType: chrome.activityLogPrivate.ExtensionActivityType.API_CALL,
    time: 1550101623113,
    args: JSON.stringify([null]),
    apiCall: 'testAPI.testMethod',
  };

  const activity2 = {
    extensionId: EXTENSION_ID,
    activityType: chrome.activityLogPrivate.ExtensionActivityType.DOM_EVENT,
    time: 1550101623116,
    args: JSON.stringify(['testArg']),
    apiCall: 'testAPI.DOMMethod',
    pageUrl: 'testUrl',
  };

  const contentScriptActivity = {
    extensionId: EXTENSION_ID,
    activityType:
        chrome.activityLogPrivate.ExtensionActivityType.CONTENT_SCRIPT,
    time: 1550101623115,
    args: JSON.stringify(['script1.js', 'script2.js']),
    apiCall: '',
  };

  /**
   * Extension activityLogStream created before each test.
   * @type {extensions.ActivityLogStream}
   */
  let activityLogStream;
  let proxyDelegate;
  let testVisible;

  // Initialize an extension activity log item before each test.
  setup(function() {
    PolymerTest.clearBody();
    proxyDelegate = new extensions.TestService();

    activityLogStream = new extensions.ActivityLogStream();

    activityLogStream.extensionId = EXTENSION_ID;
    activityLogStream.delegate = proxyDelegate;
    testVisible = extension_test_util.testVisible.bind(null, activityLogStream);

    document.body.appendChild(activityLogStream);
  });

  teardown(function() {
    activityLogStream.remove();
  });

  // Returns a list of visible stream items. The not([hidden]) selector is
  // needed for iron-list as it reuses components but hides them when not in
  // use.
  function getStreamItems() {
    return activityLogStream.shadowRoot.querySelectorAll(
        'activity-log-stream-item:not([hidden])');
  }

  test('button toggles stream on/off', function() {
    Polymer.dom.flush();

    // Stream should be on when element is first attached to the DOM.
    testVisible('.activity-subpage-header', true);
    testVisible('#empty-stream-message', true);
    testVisible('#stream-started-message', true);

    activityLogStream.$$('#toggle-stream-button').click();
    testVisible('#stream-stopped-message', true);
  });

  test(
      'new activity events are only shown while the stream is started',
      function() {
        Polymer.dom.flush();
        proxyDelegate.getOnExtensionActivity().callListeners(activity1);

        Polymer.dom.flush();
        // One event coming in. Since the stream is on, we should be able to see
        // it.
        let streamItems = getStreamItems();
        expectEquals(1, streamItems.length);

        // Pause the stream.
        activityLogStream.$$('#toggle-stream-button').click();
        proxyDelegate.getOnExtensionActivity().callListeners(
            contentScriptActivity);

        Polymer.dom.flush();
        // One event was fired but the stream was paused, we should still see
        // only one item.
        streamItems = getStreamItems();
        expectEquals(1, streamItems.length);

        // Resume the stream.
        activityLogStream.$$('#toggle-stream-button').click();
        proxyDelegate.getOnExtensionActivity().callListeners(activity2);

        Polymer.dom.flush();
        streamItems = getStreamItems();
        expectEquals(2, streamItems.length);

        expectEquals(
            streamItems[0].$$('#activity-name').innerText,
            'testAPI.testMethod');
        expectEquals(
            streamItems[1].$$('#activity-name').innerText, 'testAPI.DOMMethod');
      });

  test('activities shown match search query', function() {
    Polymer.dom.flush();
    testVisible('#empty-stream-message', true);

    proxyDelegate.getOnExtensionActivity().callListeners(activity1);
    proxyDelegate.getOnExtensionActivity().callListeners(activity2);

    Polymer.dom.flush();
    expectEquals(2, getStreamItems().length);

    const search = activityLogStream.$$('cr-search-field');
    assertTrue(!!search);

    // Search for the apiCall of |activity1|.
    search.setValue('testMethod');
    Polymer.dom.flush();

    const filteredStreamItems = getStreamItems();
    expectEquals(1, getStreamItems().length);
    expectEquals(
        filteredStreamItems[0].$$('#activity-name').innerText,
        'testAPI.testMethod');

    // search again, expect none
    search.setValue('not expecting any activities to match');
    Polymer.dom.flush();

    expectEquals(0, getStreamItems().length);
    testVisible('#empty-stream-message', false);
    testVisible('#empty-search-message', true);

    // Another activity comes in while the stream is listening but search
    // returns no results.
    proxyDelegate.getOnExtensionActivity().callListeners(contentScriptActivity);

    search.$$('#clearSearch').click();
    Polymer.dom.flush();

    // We expect 4 activities to appear as |contentScriptActivity| (which is
    // split into 2 items) should be processed and stored in the stream
    // regardless of the search input.
    expectEquals(4, getStreamItems().length);
  });

  test('content script events are split by content script names', function() {
    proxyDelegate.getOnExtensionActivity().callListeners(contentScriptActivity);

    Polymer.dom.flush();
    let streamItems = getStreamItems();
    expectEquals(2, streamItems.length);

    // We should see two items: one for every script called.
    expectEquals(streamItems[0].$$('#activity-name').innerText, 'script1.js');
    expectEquals(streamItems[1].$$('#activity-name').innerText, 'script2.js');
  });

  test('clicking on clear button clears the activity log stream', function() {
    proxyDelegate.getOnExtensionActivity().callListeners(activity1);

    Polymer.dom.flush();
    expectEquals(1, getStreamItems().length);
    testVisible('.activity-table-headings', true);
    activityLogStream.$$('.clear-activities-button').click();

    Polymer.dom.flush();
    expectEquals(0, getStreamItems().length);
    testVisible('.activity-table-headings', false);
  });
});
