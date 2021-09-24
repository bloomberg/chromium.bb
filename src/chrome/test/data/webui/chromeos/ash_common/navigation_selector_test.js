// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {NavigationSelectorElement, SelectorItem} from 'chrome://resources/ash/common/navigation_selector.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {waitAfterNextRender} from '../../test_util.js';

export function navigationSelectorTestSuite() {
  /** @type {?NavigationSelectorElement} */
  let navigationElement = null;

  setup(() => {
    navigationElement =
        /** @type {!NavigationSelectorElement} */ (
            document.createElement('navigation-selector'));
    document.body.appendChild(navigationElement);
  });

  teardown(() => {
    navigationElement.remove();
    navigationElement = null;
  });

  /**
   * @param {string} name
   * @param {string} pageIs
   * @param {string} icon
   * @return {!SelectorItem}
   */
  function createSelectorItem(name, pageIs, icon) {
    let item = /** @type{SelectorItem} */ (
        {'name': name, 'pageIs': pageIs, 'icon': icon});
    return item;
  }

  test('navigationSelectorLoadEntries', async () => {
    const item1 = createSelectorItem('test1', 'test-page1', '');
    const item2 = createSelectorItem('test2', 'test-page2', '');

    const entries =
        /** @type{!Array<!SelectorItem>} */ ([item1, item2]);
    navigationElement.selectorItems = entries;

    await waitAfterNextRender(navigationElement);

    const navigationElements =
        navigationElement.shadowRoot.querySelectorAll('.navigation-item');
    assertEquals(2, navigationElements.length);
    assertEquals('test1', navigationElements[0].textContent.trim());
    assertEquals('test2', navigationElements[1].textContent.trim());
  });
}
