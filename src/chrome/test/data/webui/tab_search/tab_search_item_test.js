// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Tab, TabData, TabGroup, TabGroupColor, TabItemType, TabSearchItem} from 'chrome://tab-search.top-chrome/tab_search.js';

import {assertDeepEquals, assertEquals, assertNotEquals} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

import {sampleToken} from './tab_search_test_data.js';
import {typed} from './tab_search_test_helper.js';

suite('TabSearchItemTest', () => {
  /** @type {!TabSearchItem} */
  let tabSearchItem;

  /** @param {!TabData} data */
  async function setupTest(data) {
    tabSearchItem = /** @type {!TabSearchItem} */ (
        document.createElement('tab-search-item'));
    tabSearchItem.data = typed(data, TabData);
    document.body.innerHTML = '';
    document.body.appendChild(tabSearchItem);
    await flushTasks();
  }

  /**
   * @param {string} text
   * @param {?Array<{start:number, length:number}>} fieldHighlightRanges
   * @param {!Array<string>} expected
   */
  async function assertTabSearchItemHighlights(
      text, fieldHighlightRanges, expected) {
    const data = /** @type {!TabData} */ ({
      hostname: text,
      tab: {
        active: true,
        index: 0,
        isDefaultFavicon: true,
        lastActiveTimeTicks: {internalValue: BigInt(0)},
        pinned: false,
        showIcon: true,
        tabId: 0,
        url: {url: 'https://example.com'},
        title: text,
      },
      highlightRanges: {
        'tab.title': fieldHighlightRanges,
        hostname: fieldHighlightRanges,
      },
    });
    await setupTest(data);

    assertHighlight(
        /** @type {!HTMLElement} */ (tabSearchItem.$['primaryText']), expected);
    assertHighlight(
        /** @type {!HTMLElement} */ (tabSearchItem.$['secondaryText']),
        expected);
  }

  /**
   * @param {!HTMLElement} node
   * @param {!Array<string>} expected
   */
  function assertHighlight(node, expected) {
    assertDeepEquals(
        expected,
        [].slice.call(node.querySelectorAll('.search-highlight-hit'))
            .map(e => e ? e.textContent : ''));
  }

  test('Highlight', async () => {
    const text = 'Make work better';
    await assertTabSearchItemHighlights(text, null, []);
    await assertTabSearchItemHighlights(
        text, [{start: 0, length: text.length}], ['Make work better']);
    await assertTabSearchItemHighlights(
        text, [{start: 0, length: 4}], ['Make']);
    await assertTabSearchItemHighlights(
        text, [{start: 0, length: 4}, {start: 10, length: 6}],
        ['Make', 'better']);
    await assertTabSearchItemHighlights(
        text, [{start: 5, length: 4}], ['work']);
  });

  test('CloseButtonPresence', async () => {
    const tab = /** @type {!Tab} */ ({
      active: true,
      index: 0,
      isDefaultFavicon: true,
      lastActiveTimeTicks: {internalValue: BigInt(0)},
      pinned: false,
      showIcon: true,
      tabId: 0,
      url: {url: 'https://example.com'},
      title: 'Example.com site',
    });

    await setupTest(/** @type {!TabData} */ ({
      hostname: 'example',
      tab,
      type: TabItemType.OPEN_TAB,
      highlightRanges: {},
    }));

    let tabSearchItemCloseButton = /** @type {!HTMLElement} */ (
        tabSearchItem.shadowRoot.querySelector('cr-icon-button'));
    assertNotEquals(null, tabSearchItemCloseButton);

    await setupTest(/** @type {!TabData} */ (
        {hostname: 'example', tab, type: TabItemType.RECENTLY_CLOSED_TAB}));

    tabSearchItemCloseButton = /** @type {!HTMLElement} */ (
        tabSearchItem.shadowRoot.querySelector('cr-icon-button'));
    assertEquals(null, tabSearchItemCloseButton);
  });

  test('GroupDetailsPresence', async () => {
    const token = sampleToken(1, 1);
    const tab = /** @type {!Tab} */ ({
      active: true,
      index: 0,
      isDefaultFavicon: true,
      lastActiveTimeTicks: {internalValue: BigInt(0)},
      pinned: false,
      showIcon: true,
      tabId: 0,
      groupId: token,
      url: {url: 'https://example.com'},
      title: 'Example.com site',
    });

    const tabGroup = /** @type {!TabGroup} */ ({
      id: token,
      color: TabGroupColor.kBlue,
      title: 'Examples',
    });

    await setupTest(/** @type {!TabData} */ ({
      hostname: 'example',
      tab,
      type: TabItemType.OPEN_TAB,
      tabGroup,
      highlightRanges: {},
    }));

    const groupDotElement = tabSearchItem.shadowRoot.querySelector('#groupDot');
    assertNotEquals(null, groupDotElement);
    const groupDotComputedStyle = getComputedStyle(groupDotElement);
    assertEquals(
        groupDotComputedStyle.getPropertyValue('--tab-group-color-blue'),
        groupDotComputedStyle.getPropertyValue('--group-dot-color'));

    assertNotEquals(
        null, tabSearchItem.shadowRoot.querySelector('#groupTitle'));
  });
});
