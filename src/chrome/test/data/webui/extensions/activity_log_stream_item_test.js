// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for activity-log-stream-item. */
suite('ExtensionsActivityLogStreamItemTest', function() {
  /**
   * Extension activityLogStreamItem created before each test.
   * @type {extensions.ActivityLogStreamItem}
   */
  let activityLogStreamItem;
  let testVisible;

  /**
   * StreamItem data for the activityLogStreamItem
   * @type {extensions.StreamItem}
   */
  let testStreamItem;

  // Initialize an activity log stream item before each test.
  setup(function() {
    PolymerTest.clearBody();
    testStreamItem = {
      id: 'testAPI.testMethod1550101623113',
      name: 'testAPI.testMethod',
      timestamp: 1550101623113,
      activityType: chrome.activityLogPrivate.ExtensionActivityType.API_CALL,
      pageUrl: '',
      args: null,
    };

    activityLogStreamItem = new extensions.ActivityLogStreamItem();
    activityLogStreamItem.data = testStreamItem;
    testVisible =
        extension_test_util.testVisible.bind(null, activityLogStreamItem);

    document.body.appendChild(activityLogStreamItem);
  });

  test('item not expandable if it has no page URL or args', function() {
    Polymer.dom.flush();

    testVisible('#activity-item-main-row', true);
    testVisible('cr-expand-button', false);
  });

  test('page URL and args visible when item is expanded', function() {
    testStreamItem = {
      id: 'testAPI.testMethod1550101623113',
      name: 'testAPI.testMethod',
      timestamp: 1550101623113,
      activityType: chrome.activityLogPrivate.ExtensionActivityType.API_CALL,
      pageUrl: 'example.url',
      args: JSON.stringify([null]),
    };

    activityLogStreamItem.set('data', testStreamItem);

    Polymer.dom.flush();
    testVisible('cr-expand-button', true);
    activityLogStreamItem.$$('cr-expand-button').click();

    Polymer.dom.flush();
    testVisible('.page-url-link', true);
    testVisible('.data-args', true);
  });

  teardown(function() {
    activityLogStreamItem.remove();
  });
});
