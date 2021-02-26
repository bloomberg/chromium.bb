// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://tab-strip/tab.js';
import {TabGroupElement} from 'chrome://tab-strip/tab_group.js';

import {TabStripEmbedderProxyImpl} from 'chrome://tab-strip/tab_strip_embedder_proxy.js';

import {assertEquals} from '../chai_assert.js';

import {TestTabStripEmbedderProxy} from './test_tab_strip_embedder_proxy.js';

suite('TabGroup', () => {
  const groupId = 'my-group-id';

  /** @type {!TabGroupElement} */
  let tabGroupElement;

  /** @type {!TestTabStripEmbedderProxy} */
  let testTabStripEmbedderProxy;

  setup(() => {
    testTabStripEmbedderProxy = new TestTabStripEmbedderProxy();
    TabStripEmbedderProxyImpl.instance_ = testTabStripEmbedderProxy;

    document.body.innerHTML = '';
    tabGroupElement = /** @type {!TabGroupElement} */ (
        document.createElement('tabstrip-tab-group'));
    tabGroupElement.dataset.groupId = groupId;
    tabGroupElement.appendChild(document.createElement('tabstrip-tab'));
    document.body.appendChild(tabGroupElement);
  });

  test('UpdatesVisuals', () => {
    const visuals = {
      color: '255, 0, 0',
      textColor: '0, 0, 255',
      title: 'My new title',
    };
    tabGroupElement.updateVisuals(visuals);
    assertEquals(
        visuals.title,
        tabGroupElement.shadowRoot.querySelector('#title').innerText);
    assertEquals(
        visuals.color,
        tabGroupElement.style.getPropertyValue(
            '--tabstrip-tab-group-color-rgb'));
    assertEquals(
        visuals.textColor,
        tabGroupElement.style.getPropertyValue(
            '--tabstrip-tab-group-text-color-rgb'));
  });

  test('DraggableChipStaysInPlace', () => {
    const originalChipRect = tabGroupElement.$('#chip').getBoundingClientRect();
    tabGroupElement.setDragging(true);
    const newChipRect = tabGroupElement.$('#chip').getBoundingClientRect();
    assertEquals(originalChipRect.left, newChipRect.left);
    assertEquals(originalChipRect.top, newChipRect.top);
    assertEquals(originalChipRect.right, newChipRect.right);
    assertEquals(originalChipRect.bottom, newChipRect.bottom);
  });

  test('DraggableChipStaysInPlaceInRTL', () => {
    document.documentElement.dir = 'rtl';
    const originalChipRect = tabGroupElement.$('#chip').getBoundingClientRect();
    tabGroupElement.setDragging(true);
    const newChipRect = tabGroupElement.$('#chip').getBoundingClientRect();
    assertEquals(originalChipRect.left, newChipRect.left);
    assertEquals(originalChipRect.top, newChipRect.top);
    assertEquals(originalChipRect.right, newChipRect.right);
    assertEquals(originalChipRect.bottom, newChipRect.bottom);
  });

  test('ChipOpensEditDialog', async () => {
    const chip = tabGroupElement.$('#chip');
    const chipRect = chip.getBoundingClientRect();
    chip.click();
    const [calledGroupId, locationX, locationY, width, height] =
        await testTabStripEmbedderProxy.whenCalled('showEditDialogForGroup');
    assertEquals(groupId, calledGroupId);
    assertEquals(chipRect.left, locationX);
    assertEquals(chipRect.top, locationY);
    assertEquals(chipRect.width, width);
    assertEquals(chipRect.height, height);
  });
});
