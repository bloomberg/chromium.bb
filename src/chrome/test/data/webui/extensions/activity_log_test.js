// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for extensions-activity-log. */
suite('ExtensionsActivityLogTest', function() {
  /**
   * Backing extension id, same id as the one in
   * extension_test_util.createExtensionInfo
   * @type {string}
   */
  const EXTENSION_ID = 'a'.repeat(32);

  /**
   * Extension activityLog created before each test.
   * @type {extensions.ActivityLog}
   */
  let activityLog;
  let proxyDelegate;
  let testVisible;

  const testActivities = {
    activities: [
      {
        activityId: '299',
        activityType: 'api_call',
        apiCall: 'i18n.getUILanguage',
        args: 'null',
        count: 10,
        extensionId: EXTENSION_ID,
        time: 1541203132002.664
      },
      {
        activityId: '309',
        activityType: 'dom_access',
        apiCall: 'Storage.getItem',
        args: 'null',
        count: 9,
        extensionId: EXTENSION_ID,
        other: {domVerb: 'method'},
        pageTitle: 'Test Extension',
        pageUrl: `chrome-extension://${EXTENSION_ID}/index.html`,
        time: 1541203131994.837
      },
    ]
  };

  // Initialize an extension activity log before each test.
  setup(function() {
    PolymerTest.clearBody();

    activityLog = new extensions.ActivityLog();
    testVisible = extension_test_util.testVisible.bind(null, activityLog);

    activityLog.extensionId = EXTENSION_ID;
    proxyDelegate = new extensions.TestService();
    activityLog.delegate = proxyDelegate;
    proxyDelegate.testActivities = testActivities;
    document.body.appendChild(activityLog);

    // Wait until we have finished making the call to fetch the activity log.
    return proxyDelegate.whenCalled('getExtensionActivityLog');
  });

  teardown(function() {
    activityLog.remove();
  });

  test('activities are present for extension', function() {
    Polymer.dom.flush();

    testVisible('#no-activities', false);
    testVisible('#loading-activities', false);
    testVisible('#activity-list', true);
    expectEquals(
        activityLog.shadowRoot.querySelectorAll('activity-log-item').length, 2);
  });

  test('message shown when no activities present for extension', function() {
    // Spoof an API call and pretend that the extension has no activities.
    activityLog.activityData_ = {
      activities: [],
    };

    Polymer.dom.flush();

    testVisible('#no-activities', true);
    testVisible('#loading-activities', false);
    testVisible('#activity-list', false);
    expectEquals(
        activityLog.shadowRoot.querySelectorAll('activity-log-item').length, 0);
  });

  test('message shown when activities are being fetched', function() {
    // Pretend the activity log is still loading.
    activityLog.pageState_ = ActivityLogPageState.LOADING;

    Polymer.dom.flush();

    testVisible('#no-activities', false);
    testVisible('#loading-activities', true);
    testVisible('#activity-list', false);
  });

  test('clicking on back button navigates to the details page', function() {
    Polymer.dom.flush();

    let currentPage = null;
    extensions.navigation.addListener(newPage => {
      currentPage = newPage;
    });

    activityLog.$$('#close-button').click();
    expectDeepEquals(
        currentPage, {page: Page.DETAILS, extensionId: EXTENSION_ID});
  });
});
