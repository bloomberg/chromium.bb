// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Runs tests for the OS settings search box. */

suite('OSSettingsSearchBox', () => {
  /** @const {number} */
  const DEFAULT_RELEVANCE_SCORE = 0.5;

  /** @const {!Array<mojoBase.mojom.String16>} */
  const DEFAULT_PAGE_HIERARCHY = [];

  /** @type {?OsToolbar} */
  let toolbar;

  /** @type {?OsSettingsSearchBox} */
  let searchBox;

  /** @type {?CrSearchFieldElement} */
  let field;

  /** @type {?IronDropdownElement} */
  let dropDown;

  /** @type {?IronListElement} */
  let resultList;

  /** @type {*} */
  let settingsSearchHandler;

  /** @type {?chromeos.settings.mojom.UserActionRecorderInterface} */
  let userActionRecorder;

  /** @type {?HTMLElement} */
  let noResultsSection;

  /** @param {string} term */
  async function simulateSearch(term) {
    field.$.searchInput.value = term;
    field.onSearchTermInput();
    field.onSearchTermSearch();
    await settingsSearchHandler.search;
    Polymer.dom.flush();
  }

  async function waitForListUpdate() {
    // Wait for iron-list to complete resizing.
    await test_util.eventToPromise('iron-resize', resultList);
    Polymer.dom.flush();
  }

  /**
   * @param {string} resultText Exact string of the result to be displayed.
   * @param {string} path Url path with optional params.
   * @param {?chromeos.settings.mojom.SearchResultIcon} icon Result icon enum.
   * @return {!chromeos.settings.mojom.SearchResult} A search result.
   */
  function fakeResult(resultText, urlPathWithParameters, icon) {
    return /** @type {!mojom.SearchResult} */ ({
      resultText: {
        data: Array.from(resultText, c => c.charCodeAt()),
      },
      urlPathWithParameters: urlPathWithParameters,
      icon: icon ? icon : chromeos.settings.mojom.SearchResultIcon.MIN_VALUE,
      relevanceScore: DEFAULT_RELEVANCE_SCORE,
      settingsPageHierarchy: DEFAULT_PAGE_HIERARCHY,
    });
  }

  setup(function() {
    toolbar = document.querySelector('os-settings-ui').$$('os-toolbar');
    assertTrue(!!toolbar);
    searchBox = toolbar.$$('os-settings-search-box');
    assertTrue(!!searchBox);
    field = searchBox.$$('cr-toolbar-search-field');
    assertTrue(!!field);
    dropDown = searchBox.$$('iron-dropdown');
    assertTrue(!!dropDown);
    resultList = searchBox.$$('iron-list');
    assertTrue(!!resultList);
    noResultsSection = searchBox.$$('#noSearchResultsContainer');
    assertTrue(!!noResultsSection);

    settingsSearchHandler = new settings.FakeSettingsSearchHandler();
    settings.setSearchHandlerForTesting(settingsSearchHandler);

    userActionRecorder = new settings.FakeUserActionRecorder();
    settings.setUserActionRecorderForTesting(userActionRecorder);
    settings.Router.getInstance().navigateTo(settings.routes.BASIC);
  });

  teardown(async () => {
    // Clear search field for next test.
    await simulateSearch('');
    settings.setUserActionRecorderForTesting(null);
    settings.setSearchHandlerForTesting(null);
  });

  test('User action search event', async () => {
    settingsSearchHandler.setFakeResults([]);

    assertEquals(userActionRecorder.searchCount, 0);
    await simulateSearch('query');
    assertEquals(userActionRecorder.searchCount, 1);
  });

  test('Dropdown opens correctly when results are fetched', async () => {
    // Show no results in dropdown if no results are returned.
    settingsSearchHandler.setFakeResults([]);
    assertFalse(dropDown.opened);
    await simulateSearch('query 1');
    assertTrue(dropDown.opened);
    assertEquals(searchBox.searchResults_.length, 0);
    assertFalse(noResultsSection.hidden);

    assertEquals(userActionRecorder.searchCount, 1);

    // Show result list if results are returned, and hide no results div.
    settingsSearchHandler.setFakeResults([fakeResult('result')]);
    await simulateSearch('query 2');
    assertNotEquals(searchBox.searchResults_.length, 0);
    assertTrue(noResultsSection.hidden);
  });

  test('Restore previous existing search results', async () => {
    settingsSearchHandler.setFakeResults([fakeResult('result 1')]);
    await simulateSearch('query');
    assertTrue(dropDown.opened);
    const resultRow = resultList.items[0];

    // Child blur elements except field should not trigger closing of dropdown.
    resultList.blur();
    assertTrue(dropDown.opened);
    dropDown.blur();
    assertTrue(dropDown.opened);

    // User clicks outside the search box, closing the dropdown.
    searchBox.blur();
    assertFalse(dropDown.opened);

    // User clicks on input, restoring old results and opening dropdown.
    field.$.searchInput.focus();
    assertEquals('query', field.$.searchInput.value);
    assertTrue(dropDown.opened);

    // The same result row exists.
    assertEquals(resultRow, resultList.items[0]);

    // Search field is blurred, closing the dropdown.
    field.$.searchInput.blur();
    assertFalse(dropDown.opened);

    // User clicks on input, restoring old results and opening dropdown.
    field.$.searchInput.focus();
    assertEquals('query', field.$.searchInput.value);
    assertTrue(dropDown.opened);

    // The same result row exists.
    assertEquals(resultRow, resultList.items[0]);
  });

  test('Search result rows are selected correctly', async () => {
    settingsSearchHandler.setFakeResults([fakeResult('a'), fakeResult('b')]);
    await simulateSearch('query');
    await waitForListUpdate();

    assertTrue(dropDown.opened);
    assertEquals(resultList.items.length, 2);

    // The first row should be selected when results are fetched.
    assertEquals(resultList.selectedItem, resultList.items[0]);

    // Test ArrowUp and ArrowDown interaction with selecting.
    const arrowUpEvent = new KeyboardEvent(
        'keydown', {cancelable: true, key: 'ArrowUp', keyCode: 38});
    const arrowDownEvent = new KeyboardEvent(
        'keydown', {cancelable: true, key: 'ArrowDown', keyCode: 40});

    // ArrowDown event should select next row.
    searchBox.dispatchEvent(arrowDownEvent);
    assertEquals(resultList.selectedItem, resultList.items[1]);

    // If last row selected, ArrowDown brings select back to first row.
    searchBox.dispatchEvent(arrowDownEvent);
    assertEquals(resultList.selectedItem, resultList.items[0]);

    // If first row selected, ArrowUp brings select back to last row.
    searchBox.dispatchEvent(arrowUpEvent);
    assertEquals(resultList.selectedItem, resultList.items[1]);

    // ArrowUp should bring select previous row.
    searchBox.dispatchEvent(arrowUpEvent);
    assertEquals(resultList.selectedItem, resultList.items[0]);

    // Test that ArrowLeft and ArrowRight do nothing.
    const arrowLeftEvent = new KeyboardEvent(
        'keydown', {cancelable: true, key: 'ArrowLeft', keyCode: 37});
    const arrowRightEvent = new KeyboardEvent(
        'keydown', {cancelable: true, key: 'ArrowRight', keyCode: 39});

    // No change on ArrowLeft
    searchBox.dispatchEvent(arrowLeftEvent);
    assertEquals(resultList.selectedItem, resultList.items[0]);

    // No change on ArrowRight
    searchBox.dispatchEvent(arrowRightEvent);
    assertEquals(resultList.selectedItem, resultList.items[0]);
  });

  test('Keydown Enter on search box can cause route change', async () => {
    settingsSearchHandler.setFakeResults(
        [fakeResult('WiFi Settings', 'networks?type=WiFi')]);
    await simulateSearch('query');
    await waitForListUpdate();

    const enterEvent = new KeyboardEvent(
        'keydown', {cancelable: true, key: 'Enter', keyCode: 13});

    // Keydown with Enter key on the searchBox causes navigation to selected
    // row's route.
    searchBox.dispatchEvent(enterEvent);
    Polymer.dom.flush();
    assertFalse(dropDown.opened);
    const router = settings.Router.getInstance();
    assertEquals(router.getCurrentRoute().path, '/networks');
    assertEquals(router.getQueryParameters().get('type'), 'WiFi');
  });

  test('Keypress Enter on row causes route change', async () => {
    settingsSearchHandler.setFakeResults(
        [fakeResult('WiFi Settings', 'networks?type=WiFi')]);
    await simulateSearch('query');
    await waitForListUpdate();

    const selectedOsRow = searchBox.getSelectedOsSearchResultRow_();
    assertTrue(!!selectedOsRow);

    // Keypress with Enter key on any row specifically causes navigation to
    // selected row's route. This can't happen unless the row is focused.
    const enterEvent = new KeyboardEvent(
        'keypress', {cancelable: true, key: 'Enter', keyCode: 13});
    selectedOsRow.$.searchResultContainer.dispatchEvent(enterEvent);
    assertFalse(dropDown.opened);
    const router = settings.Router.getInstance();
    assertEquals(router.getCurrentRoute().path, '/networks');
    assertEquals(router.getQueryParameters().get('type'), 'WiFi');
  });

  test('Route change when result row is clicked', async () => {
    settingsSearchHandler.setFakeResults(
        [fakeResult('WiFi Settings', 'networks?type=WiFi')]);
    await simulateSearch('query');
    await waitForListUpdate();

    const searchResultRow = searchBox.getSelectedOsSearchResultRow_();

    // Clicking on the searchResultContainer of the row correctly changes the
    // route and dropdown to close.
    searchResultRow.$.searchResultContainer.click();

    assertFalse(dropDown.opened);
    const router = settings.Router.getInstance();
    assertEquals(router.getCurrentRoute().path, '/networks');
    assertEquals(router.getQueryParameters().get('type'), 'WiFi');
  });

  test('Selecting result a second time does not deselect it.', async () => {
    settingsSearchHandler.setFakeResults(
        [fakeResult('WiFi Settings', 'networks?type=WiFi')]);
    await simulateSearch('query');
    await waitForListUpdate();

    // Clicking a selected item does not deselect it.
    const searchResultRow = searchBox.getSelectedOsSearchResultRow_();
    searchResultRow.$.searchResultContainer.click();
    assertEquals(resultList.selectedItem, resultList.items[0]);
    assertFalse(dropDown.opened);

    // Open search drop down again.
    field.$.searchInput.focus();
    assertTrue(dropDown.opened);

    // Clicking again does not deslect the row.
    searchResultRow.$.searchResultContainer.click();
    assertEquals(resultList.selectedItem, resultList.items[0]);
  });

  test('Tokenize and match result text to query text', async () => {
    settingsSearchHandler.setFakeResults([fakeResult('Search and Assistant')]);
    await simulateSearch(`Assistant Search`);
    await waitForListUpdate();
    assertEquals(
        searchBox.getSelectedOsSearchResultRow_().$.resultText.innerHTML,
        `<b>Search</b> and <b>Assistant</b>`);
  });

  test('Bold result text to matching query', async () => {
    settingsSearchHandler.setFakeResults([fakeResult('Search and Assistant')]);
    await simulateSearch(`a`);
    await waitForListUpdate();
    assertEquals(
        searchBox.getSelectedOsSearchResultRow_().$.resultText.innerHTML,
        `Se<b>a</b>rch <b>a</b>nd <b>A</b>ssist<b>a</b>nt`);
  });

  test('Bold result including ignored characters', async () => {
    settingsSearchHandler.setFakeResults([fakeResult('Turn on Wi-Fi')]);
    await simulateSearch(`wif`);
    await waitForListUpdate();
    assertEquals(
        searchBox.getSelectedOsSearchResultRow_().$.resultText.innerHTML,
        `Turn on <b>Wi-F</b>i`);
  });

  test('Test query longer than result blocks', async () => {
    settingsSearchHandler.setFakeResults([fakeResult('Turn on Wi-Fi')]);
    await simulateSearch(`onwifi`);
    await waitForListUpdate();
    assertEquals(
        searchBox.getSelectedOsSearchResultRow_().$.resultText.innerHTML,
        `Turn <b>on</b> <b>Wi-Fi</b>`);
  });

  test('Test bolding of accented characters', async () => {
    settingsSearchHandler.setFakeResults([fakeResult('Crème Brûlée')]);
    await simulateSearch(`E U`);
    await waitForListUpdate();
    assertEquals(
        searchBox.getSelectedOsSearchResultRow_().$.resultText.innerHTML,
        `Cr<b>è</b>me Br<b>û</b>l<b>é</b>e`);
  });

  test('Test no spaces nor characters that have upper/lower case', async () => {
    settingsSearchHandler.setFakeResults([fakeResult('キーボード設定---')]);
    await simulateSearch(`キー設`);
    await waitForListUpdate();
    assertEquals(
        searchBox.getSelectedOsSearchResultRow_().$.resultText.innerHTML,
        `<b>キ</b><b>ー</b>ボ<b>ー</b>ド<b>設</b>定---`);
  });

  test('Test blankspace types in result maintained', async () => {
    const resultText = 'Turn\xa0on  \xa0Wi-Fi ';

    settingsSearchHandler.setFakeResults([fakeResult(resultText)]);
    await simulateSearch(`wif`);
    await waitForListUpdate();
    assertEquals(
        searchBox.getSelectedOsSearchResultRow_().$.resultText.innerHTML,
        `Turn&nbsp;on  &nbsp;<b>Wi-F</b>i `);
  });
});
