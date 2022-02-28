// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {SelectorItem} from 'chrome://resources/ash/common/navigation_selector.js';
import {NavigationViewPanelElement} from 'chrome://resources/ash/common/navigation_view_panel.js';
import {CrDrawerElement} from 'chrome://resources/cr_elements/cr_drawer/cr_drawer.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {eventToPromise, flushTasks} from '../../test_util.js';

export function navigationViewPanelTestSuite() {
  /** @type {?NavigationViewPanelElement} */
  let viewElement = null;

  /** @type {number} */
  let eventCount = 0;
  /** @type {!Object} */
  let eventDetail = {};
  /** @type {number} */
  let numPageChangedCount = 0;

  setup(() => {
    viewElement =
        /** @type {!NavigationViewPanelElement} */ (
            document.createElement('navigation-view-panel'));
    document.body.appendChild(viewElement);
  });

  teardown(() => {
    viewElement.remove();
    viewElement = null;
    eventCount = 0;
    eventDetail = {};
    numPageChangedCount = 0;
  });

  /**
   * @param {!Object} e
   */
  function handleEvent(e) {
    eventDetail = e;
    eventCount++;
  }

  /**
   * @param {!CustomEvent} e
   */
  function onNavigationPageChanged(e) {
    numPageChangedCount++;
  }

  /**
   * @return {!NodeList<!HTMLElement>}
   */
  function getNavElements() {
    const sideNav = viewElement.shadowRoot.querySelector('navigation-selector');
    assert(!!sideNav);
    const navElements = sideNav.shadowRoot.querySelectorAll('.navigation-item');
    assert(!!navElements);
    return navElements;
  }

  /**
   * Adds a section to the navigation element.
   * @param {string} name
   * @param {string} pageType
   * @param {string} icon
   * @param {?string} id
   * @param {?Object} initialData
   * @return {!Promise}
   */
  function addNavigationSection(
      name, pageType, icon = '', id = null, initialData = null) {
    viewElement.addSelector(name, pageType, icon, id, initialData);
    return flushTasks();
  }

  /**
   * Adds sections to the navigation element.
   * @param {!Array<!SelectorItem>} sections
   * @return {!Promise}
   */
  function addNavigationSections(sections) {
    viewElement.addSelectors(sections);
    return flushTasks();
  }

  function getDrawer() {
    return /** @type {!CrDrawerElement} */ (
        viewElement.shadowRoot.querySelector('#drawer'));
  }

  /**
   * @return {!Promise}
   */
  function clickDrawerIcon() {
    viewElement.shadowRoot.querySelector('#iconButton').click();
    return flushTasks();
  }

  test('twoEntries', async () => {
    const dummyPage1 = 'dummy-page1';
    const dummyPage2 = 'dummy-page2';
    await addNavigationSection('dummyPage1', dummyPage1);
    await addNavigationSection('dummyPage2', dummyPage2);

    // Click the first selector item. Expect that the dummyPage1 to be created
    // and not hidden.
    const navElements = getNavElements();
    navElements[0].click();
    await flushTasks();
    const dummyElement1 =
        viewElement.shadowRoot.querySelector(`#${dummyPage1}`);
    assertFalse(dummyElement1.hidden);
    dummyElement1['onNavigationPageChanged'] = onNavigationPageChanged;

    // Click the second selector item. Expect that the dummyPage2 to be created
    // and not hidden. dummyPage1 should be hidden now.
    navElements[1].click();
    await flushTasks();
    const dummyElement2 =
        viewElement.shadowRoot.querySelector(`#${dummyPage2}`);
    dummyElement2['onNavigationPageChanged'] = onNavigationPageChanged;
    assertFalse(dummyElement2.hidden);
    assertTrue(dummyElement1.hidden);
    // Only one page has implemented "onNavigationPageChanged" by the second
    // navigation click, expect only one client to be notified.
    assertEquals(1, numPageChangedCount);

    // Click the first selector item. Expect that dummyPage2 is now hidden and
    // dummyPage1 is not hidden.
    navElements[0].click();
    await flushTasks();
    assertTrue(dummyElement2.hidden);
    assertFalse(dummyElement1.hidden);
    // Now that both dummy pages have implemented "onNavigationPageChanged",
    // the navigation click will trigger both page's methods.
    assertEquals(3, numPageChangedCount);
  });

  test('notifyEvent', async () => {
    const dummyPage1 = 'dummy-page1';
    await addNavigationSection('dummyPage1', dummyPage1);

    // Create the element.
    const navElements = getNavElements();
    navElements[0].click();
    await flushTasks();
    const dummyElement = viewElement.shadowRoot.querySelector(`#${dummyPage1}`);

    const functionName = 'onEventReceived';
    const expectedDetail = 'test';
    // Set the function handler for the element.
    dummyElement[functionName] = handleEvent;
    // Trigger notifyEvent and expect |dummyElement| to capture the event.
    viewElement.notifyEvent(functionName, {detail: expectedDetail});

    assertEquals(1, eventCount);
    assertEquals(expectedDetail, eventDetail.detail);
  });

  test('defaultPage', async () => {
    const dummyPage1 = 'dummy-page1';
    const dummyPage2 = 'dummy-page2';

    await addNavigationSections([
      viewElement.createSelectorItem('dummyPage1', dummyPage1),
      viewElement.createSelectorItem('dummyPage2', dummyPage2),
    ]);

    assertFalse(viewElement.shadowRoot.querySelector(`#${dummyPage1}`).hidden);
    assertFalse(!!viewElement.shadowRoot.querySelector(`#${dummyPage2}`));
  });

  test('defaultPageSetUsingQueryParam', async () => {
    const queryParams = new URLSearchParams(window.location.search);
    queryParams.set('page2', '');
    // Replace current querystring with the new one.
    window.history.replaceState(null, '', '?' + queryParams.toString());

    await addNavigationSections([
      viewElement.createSelectorItem('dummyPage1', 'dummy-page1', '', 'page1'),
      viewElement.createSelectorItem('dummyPage1', 'dummy-page2', '', 'page2')
    ]);


    assertFalse(viewElement.shadowRoot.querySelector('#page2').hidden);
    assertFalse(!!viewElement.shadowRoot.querySelector('#page1'));
  });

  test('samePageTypeDifferentId', async () => {
    const pageType = 'myPageType';
    const id1 = 'id1';
    const id2 = 'id2';

    // Add two pages of the the same type with different ids.
    await addNavigationSections([
      viewElement.createSelectorItem('Page 1', pageType, /*icon=*/ '', 'id1'),
      viewElement.createSelectorItem('Page 2', pageType, /*icon=*/ '', 'id2'),
    ]);

    // First page should be created by default.
    assertTrue(!!viewElement.shadowRoot.querySelector(`#${id1}`));
    assertFalse(viewElement.shadowRoot.querySelector(`#${id1}`).hidden);
    assertFalse(!!viewElement.shadowRoot.querySelector(`#${id2}`));

    // Nav to the second page and it should be created and the first page
    // should be hidden.
    const navElements = getNavElements();
    navElements[1].click();
    await flushTasks();

    assertTrue(viewElement.shadowRoot.querySelector(`#${id1}`).hidden);
    assertTrue(!!viewElement.shadowRoot.querySelector(`#${id2}`));
    assertFalse(viewElement.shadowRoot.querySelector(`#${id2}`).hidden);
  });

  test('toolBarVisible', async () => {
    const dummyPage1 = 'dummy-page1';
    const expectedTitle = 'title';
    viewElement.title = expectedTitle;
    viewElement.showToolBar = true;

    await addNavigationSections([
      viewElement.createSelectorItem('dummyPage1', dummyPage1),
    ]);

    assertFalse(viewElement.shadowRoot.querySelector(`#${dummyPage1}`).hidden);
    // The title is only visible if the toolbar is stamped.
    const pageToolbar = viewElement.shadowRoot.querySelector('page-toolbar');
    const toolbarTitle = pageToolbar.$.title.textContent.trim();
    assertEquals(expectedTitle, toolbarTitle);
  });

  test('ToggleDrawer', async () => {
    const dummyPage1 = 'dummy-page1';
    const expectedTitle = 'title';
    viewElement.title = expectedTitle;
    viewElement.showToolBar = true;

    await addNavigationSections([
      viewElement.createSelectorItem('dummyPage1', dummyPage1),
    ]);

    const drawer = getDrawer();
    drawer.openDrawer();
    await eventToPromise('cr-drawer-opened', drawer);
    assertTrue(drawer.open);

    // Clicking the drawer icon closes the drawer.
    await clickDrawerIcon();
    await eventToPromise('close', drawer);
    assertFalse(drawer.open);
    assertTrue(drawer.wasCanceled());
  });
}
