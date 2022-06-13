// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {CrTabsElement} from 'chrome://resources/cr_elements/cr_tabs/cr_tabs.js';

import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {assertEquals, assertNotEquals, assertTrue} from '../chai_assert.js';
import {eventToPromise, flushTasks} from '../test_util.js';
// clang-format on

suite('cr_tabs_test', function() {
  /** @type {!CrTabsElement} */
  let tabs;

  setup(() => {
    document.body.innerHTML = '';
    tabs = /** @type {!CrTabsElement} */ (document.createElement('cr-tabs'));
    tabs.tabNames = ['tab1', 'tab2', 'tab3'];
    tabs.tabIcons = ['chrome://icon1.png'];
    document.body.appendChild(tabs);
    return flushTasks();
  });

  /**
   * @param {number} index
   * @return {!HTMLElement}
   */
  function getTabElement(index) {
    return /** @type {!HTMLElement} */ (
        tabs.shadowRoot.querySelector(`.tab:nth-of-type(${index + 1})`));
  }

  /**
   * @param {Function} uiChange
   * @param {number} initialSelection
   * @param {number} expectedSelection
   */
  async function checkUiChange(uiChange, initialSelection, expectedSelection) {
    tabs.selected = initialSelection;
    if (initialSelection === expectedSelection) {
      uiChange();
    } else {
      const wait = eventToPromise('selected-changed', tabs);
      uiChange();
      await wait;
    }
    assertEquals(expectedSelection, tabs.selected);
    const tabElement = getTabElement(expectedSelection);
    assertTrue(!!tabElement);
    assertTrue(tabElement.classList.contains('selected'));
    assertEquals('0', tabElement.getAttribute('tabindex'));
    const notSelected = tabs.shadowRoot.querySelectorAll('.tab:not(.selected)');
    assertEquals(2, notSelected.length);
    notSelected.forEach(tab => {
      assertEquals('-1', tab.getAttribute('tabindex'));
    });
  }

  /**
   * @param {string} key
   * @param {number} initialSelection
   * @param {number} expectedSelection
   */
  async function checkKey(key, initialSelection, expectedSelection) {
    await checkUiChange(
        () => keyDownOn(tabs, 0, [], key), initialSelection, expectedSelection);
  }

  /**
   * @param {number} initialSelection
   * @param {number} expectedSelection
   */
  async function checkClickTab(initialSelection, expectedSelection) {
    await checkUiChange(
        () => getTabElement(expectedSelection).click(), initialSelection,
        expectedSelection);
  }

  test('check CSS classes, aria-selected and tabindex for a tab', () => {
    const tab = getTabElement(0);
    assertEquals(1, tab.classList.length);
    assertEquals('false', tab.getAttribute('aria-selected'));
    assertEquals('-1', tab.getAttribute('tabindex'));
    tabs.selected = 0;
    assertEquals(2, tab.classList.length);
    assertTrue(tab.classList.contains('selected'));
    assertEquals('true', tab.getAttribute('aria-selected'));
    assertEquals('0', tab.getAttribute('tabindex'));
    tabs.selected = 1;
    assertEquals(1, tab.classList.length);
    assertEquals('false', tab.getAttribute('aria-selected'));
    assertEquals('-1', tab.getAttribute('tabindex'));
  });

  test('tab icons are optional', () => {
    const tab0 = getTabElement(0);
    const tabIcon0 = tab0.querySelector('.tab-icon');
    assertNotEquals('none', getComputedStyle(tabIcon0).display);

    const tab1 = getTabElement(1);
    const tabIcon1 = tab1.querySelector('.tab-icon');
    assertEquals('none', getComputedStyle(tabIcon1).display);
  });

  test('right/left pressed, selection changes and event fires', async () => {
    await checkKey('ArrowRight', 0, 1);
    await checkKey('ArrowRight', 1, 2);
    // Check that the selection wraps.
    await checkKey('ArrowRight', 2, 0);

    await checkKey('ArrowLeft', 2, 1);
    await checkKey('ArrowLeft', 1, 0);
    // Check that the selection wraps.
    await checkKey('ArrowLeft', 0, 2);

    await checkKey('Home', 0, 0);
    await checkKey('Home', 1, 0);
    await checkKey('Home', 2, 0);
    await checkKey('End', 0, 2);
    await checkKey('End', 1, 2);
    await checkKey('End', 2, 2);
  });

  test('clicking on tabs, selection changes and event fires', async () => {
    await checkClickTab(0, 0);
    await checkClickTab(1, 0);
    await checkClickTab(2, 0);
    await checkClickTab(0, 1);
    await checkClickTab(1, 1);
    await checkClickTab(2, 1);
    await checkClickTab(0, 2);
    await checkClickTab(1, 2);
    await checkClickTab(2, 2);
  });
});
