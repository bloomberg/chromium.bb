// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Tests that breadcrumbs work.
 */

'use strict';

(() => {
  testcase.breadcrumbsNavigate = async () => {
    const files = [ENTRIES.hello, ENTRIES.photos];
    const appId = await setupAndWaitUntilReady(RootPath.DOWNLOADS, files, []);

    // Navigate to Downloads/photos.
    await remoteCall.navigateWithDirectoryTree(
        appId, '/Downloads/photos', 'My files');

    // Use the breadcrumbs to navigate back to Downloads.
    await remoteCall.waitAndClickElement(
        appId, '#location-breadcrumbs .breadcrumb-path:nth-of-type(2)');

    // Wait for the contents of Downloads to load again.
    await remoteCall.waitForFiles(appId, TestEntryInfo.getExpectedRows(files));

    // A user action should have been recorded for the breadcrumbs.
    chrome.test.assertEq(
        1, await getUserActionCount('FileBrowser.ClickBreadcrumbs'));
  };

  /**
   * Test that clicking on the current directory in the Breadcrumbs doesn't
   * leave the focus in the breadcrumbs. crbug.com/944022
   */
  testcase.breadcrumbsLeafNoFocus = async () => {
    const appId =
        await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.photos], []);

    // Navigate to Downloads/photos.
    await remoteCall.navigateWithDirectoryTree(
        appId, '/Downloads/photos', 'My files');

    // Focus and click on "photos" in the breadcrumbs.
    const leafBreadCrumb =
        '#location-breadcrumbs .breadcrumb-path.breadcrumb-last';
    chrome.test.assertTrue(
        !!await remoteCall.callRemoteTestUtil('focus', appId, [leafBreadCrumb]),
        'focus failed: ' + leafBreadCrumb);
    await remoteCall.waitAndClickElement(appId, leafBreadCrumb);

    // Wait focus to not be on breadcrumb clicked.
    await remoteCall.waitForElementLost(appId, leafBreadCrumb + ':focus');

    // Focus should be on file list.
    await remoteCall.waitForElement(appId, '#file-list:focus');
  };

  /**
   * Tests that Downloads is translated in the breadcrumbs.
   */
  testcase.breadcrumbsDownloadsTranslation = async () => {
    // Switch UI to Portuguese (Portugal).
    await sendTestMessage({name: 'switchLanguage', language: 'pt-PT'});

    // Reload Files app to pick up the new language.
    await remoteCall.callRemoteTestUtil('reload', null, []);

    // Open Files app.
    const appId =
        await setupAndWaitUntilReady(RootPath.DOWNLOADS, [ENTRIES.photos], []);

    // Check the breadcrumbs for Downloads:
    // Os meu ficheiros => My files.
    // Transferências => Downloads (as in Transfers).
    const path =
        await remoteCall.callRemoteTestUtil('getBreadcrumbPath', appId, []);
    chrome.test.assertEq('/Os meus ficheiros/Transferências', path);

    // Expand Downloads folder.
    await expandTreeItem(
        appId, '#directory-tree [full-path-for-testing="/Downloads"]');

    // Navigate to Downloads/photos.
    await remoteCall.waitAndClickElement(
        appId, '[full-path-for-testing="/Downloads/photos"]');

    // Wait and check breadcrumb translation.
    await remoteCall.waitUntilCurrentDirectoryIsChanged(
        appId, '/Os meus ficheiros/Transferências/photos');
  };

  /**
   * Creates a folder test entry from a folder |path|.
   * @param {string} path The folder path.
   * @return {!TestEntryInfo}
   */
  function createTestFolder(path) {
    const name = path.split('/').pop();
    return new TestEntryInfo({
      targetPath: path,
      nameText: name,
      type: EntryType.DIRECTORY,
      lastModifiedTime: 'Jan 1, 1980, 11:59 PM',
      sizeText: '--',
      typeText: 'Folder',
    });
  }

  /**
   * Returns an array of nested folder test entries, where |depth| controls
   * the nesting. For example, a |depth| of 4 will return:
   *
   *   [0]: nested-folder0
   *   [1]: nested-folder0/nested-folder1
   *   [2]: nested-folder0/nested-folder1/nested-folder2
   *   [3]: nested-folder0/nested-folder1/nested-folder2/nested-folder3
   *
   * @param {number} depth The nesting depth.
   * @return {!Array<!TestEntryInfo>}
   */
  function createNestedTestFolders(depth) {
    const nestedFolderTestEntries = [];

    for (let path = 'nested-folder0', i = 0; i < depth; ++i) {
      nestedFolderTestEntries.push(createTestFolder(path));
      path += `/nested-folder${i + 1}`;
    }

    return nestedFolderTestEntries;
  }

  /**
   * Tests that breadcrumbs has to tooltip. crbug.com/951305
   */
  testcase.breadcrumbsTooltip = async () => {
    // Build an array of nested folder test entries.
    const nestedFolderTestEntries = createNestedTestFolders(8);

    // Open FilesApp on Downloads containing the test entries.
    const appId = await setupAndWaitUntilReady(
        RootPath.DOWNLOADS, nestedFolderTestEntries, []);

    // Navigate to deepest folder.
    const breadcrumb = '/My files/Downloads/' +
        nestedFolderTestEntries.map(e => e.nameText).join('/');
    await navigateWithDirectoryTree(appId, breadcrumb);

    // Get breadcrumb that is "collapsed" in the default window size.
    const collapsedBreadcrumb =
        await remoteCall.waitForElement(appId, '#breadcrumb-path-8');

    // Check: Should have aria-label so screenreader announces the folder name.
    chrome.test.assertEq(
        'nested-folder6', collapsedBreadcrumb.attributes['aria-label']);
    // Check: Should have collapsed attribute so it shows as "...".
    chrome.test.assertEq('', collapsedBreadcrumb.attributes['collapsed']);

    // Focus the search box.
    chrome.test.assertTrue(await remoteCall.callRemoteTestUtil(
        'fakeEvent', appId, ['#breadcrumb-path-8', 'focus']));

    // Check: The tooltip should be visible and its content is the folder name.
    const tooltipQueryVisible = 'files-tooltip[visible=true]';
    const tooltip = await remoteCall.waitForElement(appId, tooltipQueryVisible);
    const label =
        await remoteCall.waitForElement(appId, [tooltipQueryVisible, '#label']);
    chrome.test.assertEq('nested-folder6', label.text);
  };

  /**
   * Tests that the breadcrumbs correctly render a short (3 component) path.
   */
  testcase.breadcrumbsRenderShortPath = async () => {
    // Build an array of nested folder test entries.
    const nestedFolderTestEntries = createNestedTestFolders(1);

    // Open FilesApp on Downloads containing the test entries.
    const appId = await setupAndWaitUntilReady(
        RootPath.DOWNLOADS, nestedFolderTestEntries, []);

    // Navigate to deepest folder.
    const breadcrumb = '/My files/Downloads/' +
        nestedFolderTestEntries.map(e => e.nameText).join('/');
    await navigateWithDirectoryTree(appId, breadcrumb);

    // Check: the breadcrumb element should have a |path| attribute.
    const breadcrumbElement =
        await remoteCall.waitForElement(appId, ['bread-crumb']);
    const path = breadcrumb.slice(1);  // remove leading "/" char
    chrome.test.assertEq(path, breadcrumbElement.attributes.path);

    // Check: some of the main breadcrumb buttons should be visible.
    const buttons = ['bread-crumb', 'button:not([hidden])'];
    const elements = await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [buttons]);
    chrome.test.assertEq(3, elements.length);

    // Check: the main button text should be the path components.
    chrome.test.assertEq('My files', elements[0].text);
    chrome.test.assertEq('Downloads', elements[1].text);
    chrome.test.assertEq('nested-folder0', elements[2].text);

    // Check: the "last" main button should be disabled.
    chrome.test.assertEq(undefined, elements[0].attributes.disabled);
    chrome.test.assertEq(undefined, elements[1].attributes.disabled);
    chrome.test.assertEq('', elements[2].attributes.disabled);

    // Check: the breadcrumb elider button should be hidden.
    const eliderButtonHidden = ['bread-crumb', '[elider][hidden]'];
    await remoteCall.waitForElement(appId, eliderButtonHidden);
  };

  /**
   * Tests that short breadcrumbs paths (of 4 or fewer components) should not
   * be rendered elided. The elider button should be hidden.
   */
  testcase.breadcrumbsEliderButtonHidden = async () => {
    // Build an array of nested folder test entries.
    const nestedFolderTestEntries = createNestedTestFolders(2);

    // Open FilesApp on Downloads containing the test entries.
    const appId = await setupAndWaitUntilReady(
        RootPath.DOWNLOADS, nestedFolderTestEntries, []);

    // Navigate to deepest folder.
    const breadcrumb = '/My files/Downloads/' +
        nestedFolderTestEntries.map(e => e.nameText).join('/');
    await navigateWithDirectoryTree(appId, breadcrumb);

    // Check: the breadcrumb element should have a |path| attribute.
    const breadcrumbElement =
        await remoteCall.waitForElement(appId, ['bread-crumb']);
    const path = breadcrumb.slice(1);  // remove leading "/" char
    chrome.test.assertEq(path, breadcrumbElement.attributes.path);

    // Check: all of the main breadcrumb buttons should be visible.
    const buttons = ['bread-crumb', 'button:not([hidden])'];
    const elements = await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [buttons]);
    chrome.test.assertEq(4, elements.length);

    // Check: the main button text should be the path components.
    chrome.test.assertEq('My files', elements[0].text);
    chrome.test.assertEq('Downloads', elements[1].text);
    chrome.test.assertEq('nested-folder0', elements[2].text);
    chrome.test.assertEq('nested-folder1', elements[3].text);

    // Check: the "last" main button should be disabled.
    chrome.test.assertEq(undefined, elements[0].attributes.disabled);
    chrome.test.assertEq(undefined, elements[1].attributes.disabled);
    chrome.test.assertEq(undefined, elements[2].attributes.disabled);
    chrome.test.assertEq('', elements[3].attributes.disabled);

    // Check: the breadcrumb elider button should be hidden.
    const eliderButtonHidden = ['bread-crumb', '[elider][hidden]'];
    await remoteCall.waitForElement(appId, eliderButtonHidden);
  };

  /**
   * Tests that the breadcrumbs correctly render a long (5 component) path.
   */
  testcase.breadcrumbsRenderLongPath = async () => {
    // Build an array of nested folder test entries.
    const nestedFolderTestEntries = createNestedTestFolders(3);

    // Open FilesApp on Downloads containing the test entries.
    const appId = await setupAndWaitUntilReady(
        RootPath.DOWNLOADS, nestedFolderTestEntries, []);

    // Navigate to deepest folder.
    const breadcrumb = '/My files/Downloads/' +
        nestedFolderTestEntries.map(e => e.nameText).join('/');
    await navigateWithDirectoryTree(appId, breadcrumb);

    // Check: the breadcrumb element should have a |path| attribute.
    const breadcrumbElement =
        await remoteCall.waitForElement(appId, ['bread-crumb']);
    const path = breadcrumb.slice(1);  // remove leading "/" char
    chrome.test.assertEq(path, breadcrumbElement.attributes.path);

    // Check: some of the main breadcrumb buttons should be visible.
    const buttons = ['bread-crumb', 'button[id]:not([hidden])'];
    const elements = await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [buttons]);
    chrome.test.assertEq(3, elements.length);

    // Check: main button text should be the non-elided path components.
    chrome.test.assertEq('My files', elements[0].text);
    chrome.test.assertEq('nested-folder1', elements[1].text);
    chrome.test.assertEq('nested-folder2', elements[2].text);

    // Check: the "last" main button should be disabled.
    chrome.test.assertEq(undefined, elements[0].attributes.disabled);
    chrome.test.assertEq(undefined, elements[1].attributes.disabled);
    chrome.test.assertEq('', elements[2].attributes.disabled);

    // Check: the breadcrumb elider button should be shown.
    const eliderButton = ['bread-crumb', '[elider]:not([hidden])'];
    await remoteCall.waitForElement(appId, eliderButton);
  };

  /**
   * Tests that clicking a main breadcumb button makes the app navigate and
   * update the breadcrumb to that item.
   */
  testcase.breadcrumbsMainButtonClick = async () => {
    // Build an array of nested folder test entries.
    const nestedFolderTestEntries = createNestedTestFolders(2);

    // Open FilesApp on Downloads containing the test entries.
    const appId = await setupAndWaitUntilReady(
        RootPath.DOWNLOADS, nestedFolderTestEntries, []);

    // Navigate to deepest folder.
    const breadcrumb = '/My files/Downloads/' +
        nestedFolderTestEntries.map(e => e.nameText).join('/');
    await navigateWithDirectoryTree(appId, breadcrumb);

    // Check: the breadcrumb path attribute should be |breadcrumb|.
    const breadcrumbElement =
        await remoteCall.waitForElement(appId, ['bread-crumb']);
    const path = breadcrumb.slice(1);  // remove leading "/" char
    chrome.test.assertEq(path, breadcrumbElement.attributes.path);

    // Click the "second" main breadcrumb button (2nd path component).
    await remoteCall.waitAndClickElement(
        appId, ['bread-crumb', 'button[id="second"]']);

    // Check: the breadcrumb path should be updated due to navigation.
    await remoteCall.waitForElement(
        appId, ['bread-crumb[path="My files/Downloads"']);
  };

  /**
   * Tests that an Enter key on a main breadcumb button item makes the app
   * navigate and update the breadcrumb to that item.
   */
  testcase.breadcrumbsMainButtonEnterKey = async () => {
    // Build an array of nested folder test entries.
    const nestedFolderTestEntries = createNestedTestFolders(2);

    // Open FilesApp on Downloads containing the test entries.
    const appId = await setupAndWaitUntilReady(
        RootPath.DOWNLOADS, nestedFolderTestEntries, []);

    // Navigate to deepest folder.
    const breadcrumb = '/My files/Downloads/' +
        nestedFolderTestEntries.map(e => e.nameText).join('/');
    await navigateWithDirectoryTree(appId, breadcrumb);

    // Check: the breadcrumb path attribute should be |breadcrumb|.
    const breadcrumbElement =
        await remoteCall.waitForElement(appId, ['bread-crumb']);
    const path = breadcrumb.slice(1);  // remove leading "/" char
    chrome.test.assertEq(path, breadcrumbElement.attributes.path);

    // Send an Enter key to the "second" main breadcrumb button.
    const secondButton = ['bread-crumb', 'button[id="second"]'];
    const enterKey = [secondButton, 'Enter', false, false, false];
    chrome.test.assertTrue(
        await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, enterKey));

    // Check: the breadcrumb path should be updated due to navigation.
    await remoteCall.waitForElement(
        appId, ['bread-crumb[path="My files/Downloads"']);
  };

  /**
   * Tests that a breadcrumbs elider button click opens its drop down menu and
   * that clicking the button again, closes the drop down menu.
   */
  testcase.breadcrumbsEliderButtonClick = async () => {
    // Build an array of nested folder test entries.
    const nestedFolderTestEntries = createNestedTestFolders(3);

    // Open FilesApp on Downloads containing the test entries.
    const appId = await setupAndWaitUntilReady(
        RootPath.DOWNLOADS, nestedFolderTestEntries, []);

    // Navigate to deepest folder.
    const breadcrumb = '/My files/Downloads/' +
        nestedFolderTestEntries.map(e => e.nameText).join('/');
    await navigateWithDirectoryTree(appId, breadcrumb);

    // Click the breadcrumb elider button when it appears.
    const eliderButton = ['bread-crumb', '[elider]:not([hidden])'];
    await remoteCall.waitAndClickElement(appId, eliderButton);

    // Check: the elider button drop-down menu should open.
    const menu = ['bread-crumb', '#elider-menu', 'dialog[open]'];
    await remoteCall.waitForElement(appId, menu);

    // Check: the drop-down menu should contain 2 elided items.
    const menuItems = ['bread-crumb', '#elider-menu .dropdown-item'];
    const elements = await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [menuItems]);
    chrome.test.assertEq(2, elements.length);

    // Check: the menu item text should be the elided path components.
    chrome.test.assertEq('Downloads', elements[0].text);
    chrome.test.assertEq('nested-folder0', elements[1].text);

    // Check: the elider button should not have the focus.
    const eliderFocus = ['bread-crumb', '[elider]:not([hidden]):focus'];
    await remoteCall.waitForElementLost(appId, eliderFocus);

    // Click the elider button.
    await remoteCall.waitAndClickElement(appId, eliderButton);

    // Check: the elider button drop-down menu should close.
    await remoteCall.waitForElementLost(appId, menu);

    // Check: the focus should return to the elider button.
    await remoteCall.waitForElement(appId, eliderFocus);
  };

  /**
   * Tests that pressing Enter key on the breadcrumbs elider button opens its
   * drop down menu, then pressing Escape key closes the drop down menu.
   */
  testcase.breadcrumbsEliderButtonKeyboard = async () => {
    // Build an array of nested folder test entries.
    const nestedFolderTestEntries = createNestedTestFolders(4);

    // Open FilesApp on Downloads containing the test entries.
    const appId = await setupAndWaitUntilReady(
        RootPath.DOWNLOADS, nestedFolderTestEntries, []);

    // Navigate to deepest folder.
    const breadcrumb = '/My files/Downloads/' +
        nestedFolderTestEntries.map(e => e.nameText).join('/');
    await navigateWithDirectoryTree(appId, breadcrumb);

    // Wait for the breadcrumb elider button to appear.
    const eliderButton = ['bread-crumb', '[elider]:not([hidden])'];
    await remoteCall.waitForElement(appId, eliderButton);

    // Send an Enter key to the breadcrumb elider button.
    const enterKey = [eliderButton, 'Enter', false, false, false];
    chrome.test.assertTrue(
        await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, enterKey));

    // Check: the elider button drop-down menu should open.
    const menu = ['bread-crumb', '#elider-menu', 'dialog[open]'];
    await remoteCall.waitForElement(appId, menu);

    // Check: the drop-down menu should contain 3 elided items.
    const menuItems = ['bread-crumb', '#elider-menu .dropdown-item'];
    const elements = await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [menuItems]);
    chrome.test.assertEq(3, elements.length);

    // Check: the menu item text should be the elided path components.
    chrome.test.assertEq('Downloads', elements[0].text);
    chrome.test.assertEq('nested-folder0', elements[1].text);
    chrome.test.assertEq('nested-folder1', elements[2].text);

    // Check: the elider button should not have the focus.
    const eliderFocus = ['bread-crumb', '[elider]:not([hidden]):focus'];
    await remoteCall.waitForElementLost(appId, eliderFocus);

    // Send an Escape key to the drop-down menu.
    const escKey = [menu, 'Escape', false, false, false];
    chrome.test.assertTrue(
        await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, escKey));

    // Check: the elider button drop-down menu should close.
    await remoteCall.waitForElementLost(appId, menu);

    // Check: the focus should return to the elider button.
    await remoteCall.waitForElement(appId, eliderFocus);
  };

  /**
   * Tests that clicking outside the elider button drop down menu makes that
   * drop down menu close.
   */
  testcase.breadcrumbsEliderMenuClickOutside = async () => {
    // Build an array of nested folder test entries.
    const nestedFolderTestEntries = createNestedTestFolders(5);

    // Open FilesApp on Downloads containing the test entries.
    const appId = await setupAndWaitUntilReady(
        RootPath.DOWNLOADS, nestedFolderTestEntries, []);

    // Navigate to deepest folder.
    const breadcrumb = '/My files/Downloads/' +
        nestedFolderTestEntries.map(e => e.nameText).join('/');
    await navigateWithDirectoryTree(appId, breadcrumb);

    // Click the breadcrumb elider button when it appears.
    const eliderButton = ['bread-crumb', '[elider]:not([hidden])'];
    await remoteCall.waitAndClickElement(appId, eliderButton);

    // Check: the elider button drop-down menu should open.
    const menu = ['bread-crumb', '#elider-menu', 'dialog[open]'];
    await remoteCall.waitForElement(appId, menu);

    // Click somewhere outside the drop down menu.
    await remoteCall.simulateUiClick(appId, '#file-list');

    // Check: the elider button drop-down menu should close.
    await remoteCall.waitForElementLost(appId, menu);
  };

  /**
   * Tests that clicking an elider button drop down menu item makes the app
   * navigate and update the breadcrumb to that item.
   */
  testcase.breadcrumbsEliderMenuItemClick = async () => {
    // Build an array of nested folder test entries.
    const nestedFolderTestEntries = createNestedTestFolders(3);

    // Open FilesApp on Downloads containing the test entries.
    const appId = await setupAndWaitUntilReady(
        RootPath.DOWNLOADS, nestedFolderTestEntries, []);

    // Navigate to deepest folder.
    const breadcrumb = '/My files/Downloads/' +
        nestedFolderTestEntries.map(e => e.nameText).join('/');
    await navigateWithDirectoryTree(appId, breadcrumb);

    // Check: the breadcrumb path attribute should be |breadcrumb|.
    const breadcrumbElement =
        await remoteCall.waitForElement(appId, ['bread-crumb']);
    const path = breadcrumb.slice(1);  // remove leading "/" char
    chrome.test.assertEq(path, breadcrumbElement.attributes.path);

    // Click the breadcrumb elider button when it appears.
    const eliderButton = ['bread-crumb', '[elider]:not([hidden])'];
    await remoteCall.waitAndClickElement(appId, eliderButton);

    // Check: the elider button drop-down menu should open.
    const menu = ['bread-crumb', '#elider-menu', 'dialog[open]'];
    await remoteCall.waitForElement(appId, menu);

    // Check: the drop-down menu should contain 2 elided items.
    const menuItems = ['bread-crumb', '#elider-menu .dropdown-item'];
    const elements = await remoteCall.callRemoteTestUtil(
        'deepQueryAllElements', appId, [menuItems]);
    chrome.test.assertEq(2, elements.length);

    // Check: the menu item text should be the elided path components.
    chrome.test.assertEq('Downloads', elements[0].text);
    chrome.test.assertEq('nested-folder0', elements[1].text);

    // Click the first drop-down menu item.
    const item = ['bread-crumb', '#elider-menu button:first-child'];
    await remoteCall.waitAndClickElement(appId, item);

    // Check: the elider button drop-down menu should close.
    await remoteCall.waitForElementLost(appId, menu);

    // Check: the breadcrumb path should be updated due to navigation.
    await remoteCall.waitForElement(
        appId, ['bread-crumb[path="My files/Downloads"']);
  };

  /**
   * Tests that a <shift>-Tab key on the elider button drop down menu closes
   * the menu and focuses button#first to the left of the elider button.
   */
  testcase.breadcrumbsEliderMenuItemTabLeft = async () => {
    // Build an array of nested folder test entries.
    const nestedFolderTestEntries = createNestedTestFolders(3);

    // Open FilesApp on Downloads containing the test entries.
    const appId = await setupAndWaitUntilReady(
        RootPath.DOWNLOADS, nestedFolderTestEntries, []);

    // Navigate to deepest folder.
    const breadcrumb = '/My files/Downloads/' +
        nestedFolderTestEntries.map(e => e.nameText).join('/');
    await navigateWithDirectoryTree(appId, breadcrumb);

    // Click the breadcrumb elider button when it appears.
    const eliderButton = ['bread-crumb', '[elider]:not([hidden])'];
    await remoteCall.waitAndClickElement(appId, eliderButton);

    // Check: the elider button drop-down menu should open.
    const menu = ['bread-crumb', '#elider-menu', 'dialog[open]'];
    await remoteCall.waitForElement(appId, menu);

    // Send an ArrowDown key to the drop-down menu.
    const key = [menu, 'ArrowDown', false, false, false];
    chrome.test.assertTrue(
        await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key));

    // Check: the drop-down menu item should focus.
    const item = ['bread-crumb', '#elider-menu .dropdown-item:focus'];
    await remoteCall.waitForElement(appId, item);

    // Dispatch a <shift>-Tab key to the focused drop-down menu item.
    const result = await sendTestMessage(
        {name: 'dispatchTabKey', /* key modifier */ shift: true});
    chrome.test.assertEq(
        result, 'tabKeyDispatched', 'shift-Tab key dispatch failure');

    // Check: the elider button drop-down menu should close.
    await remoteCall.waitForElementLost(appId, menu);

    // Check: the "first" main button should be focused.
    await remoteCall.waitForElement(
        appId, ['bread-crumb', 'button[id="first"]:focus']);
  };

  /**
   * Tests that a Tab key on the elider button drop down menu closes the menu
   * and focuses button#third to the right of the elider button.
   */
  testcase.breadcrumbsEliderMenuItemTabRight = async () => {
    // Build an array of nested folder test entries.
    const nestedFolderTestEntries = createNestedTestFolders(3);

    // Open FilesApp on Downloads containing the test entries.
    const appId = await setupAndWaitUntilReady(
        RootPath.DOWNLOADS, nestedFolderTestEntries, []);

    // Navigate to deepest folder.
    const breadcrumb = '/My files/Downloads/' +
        nestedFolderTestEntries.map(e => e.nameText).join('/');
    await navigateWithDirectoryTree(appId, breadcrumb);

    // Click the breadcrumb elider button when it appears.
    const eliderButton = ['bread-crumb', '[elider]:not([hidden])'];
    await remoteCall.waitAndClickElement(appId, eliderButton);

    // Check: the elider button drop-down menu should open.
    const menu = ['bread-crumb', '#elider-menu', 'dialog[open]'];
    await remoteCall.waitForElement(appId, menu);

    // Send an ArrowDown key to the drop-down menu.
    const key = [menu, 'ArrowDown', false, false, false];
    chrome.test.assertTrue(
        await remoteCall.callRemoteTestUtil('fakeKeyDown', appId, key));

    // Check: the drop-down menu item should focus.
    const item = ['bread-crumb', '#elider-menu .dropdown-item:focus'];
    await remoteCall.waitForElement(appId, item);

    // Dispatch a Tab key to the focused drop-down menu item.
    const result = await sendTestMessage(
        {name: 'dispatchTabKey', /* key modifier */ shift: false});
    chrome.test.assertEq(
        result, 'tabKeyDispatched', 'Tab key dispatch failure');

    // Check: the elider button drop-down menu should close.
    await remoteCall.waitForElementLost(appId, menu);

    // Check: the "third" main button should be focused.
    await remoteCall.waitForElement(
        appId, ['bread-crumb', 'button[id="third"]:focus']);
  };
})();
