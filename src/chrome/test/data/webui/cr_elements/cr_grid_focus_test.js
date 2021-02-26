// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrGridElement} from 'chrome://resources/cr_elements/cr_grid/cr_grid.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {keyDownOn} from 'chrome://resources/polymer/v3_0/iron-test-helpers/mock-interactions.js';

import {assertEquals} from '../chai_assert.js';
import {eventToPromise} from '../test_util.m.js';

suite('CrElementsGridFocusTest', () => {
  /**
   * @param {number} size
   * @return {!CrGridElement}
   */
  function createGrid(size) {
    const grid =
        /** @type {!CrGridElement} */ (document.createElement('cr-grid'));
    for (let i = 0; i < size; i++) {
      const div = document.createElement('div');
      div.tabIndex = '0';
      div.id = i;
      grid.appendChild(div);
    }
    grid.columns = 6;
    document.body.appendChild(grid);
    return grid;
  }

  /**
   * Asserts that an element is focused.
   * @param {!Element} element The element to test.
   */
  function assertFocus(element) {
    assertEquals(element, getDeepActiveElement());
  }

  /**
   * @param {!Element} element
   * @param {string} key
   */
  function keydown(element, key) {
    keyDownOn(element, 0, [], key);
  }

  setup(() => {
    document.body.innerHTML = '';
  });

  test('right focuses right item', () => {
    // Arrange.
    const grid = createGrid(12);

    // Act.
    keydown(grid.children[0], 'ArrowRight');

    // Assert.
    assertFocus(grid.children[1]);
  });

  test('right wrap around focuses first item', () => {
    // Arrange.
    const grid = createGrid(12);

    // Act.
    keydown(grid.children[grid.children.length - 1], 'ArrowRight');

    // Assert.
    assertFocus(grid.children[0]);
  });

  test('left focuses left item', () => {
    // Arrange.
    const grid = createGrid(12);

    // Act.
    keydown(grid.children[1], 'ArrowLeft');

    // Assert.
    assertFocus(grid.children[0]);
  });

  test('left wrap around focuses last item', () => {
    // Arrange.
    const grid = createGrid(12);

    // Act.
    keydown(grid.children[0], 'ArrowLeft');

    // Assert.
    assertFocus(grid.children[grid.children.length - 1]);
  });

  test('right focuses left item in RTL', () => {
    // Arrange.
    const grid = createGrid(12);
    grid.dir = 'rtl';

    // Act.
    keydown(grid.children[1], 'ArrowRight');

    // Assert.
    assertFocus(grid.children[0]);
  });

  test('right wrap around focuses last item in RTL', () => {
    // Arrange.
    const grid = createGrid(12);
    grid.dir = 'rtl';

    // Act.
    keydown(grid.children[0], 'ArrowRight');

    // Assert.
    assertFocus(grid.children[grid.children.length - 1]);
  });

  test('left focuses right item in RTL', () => {
    // Arrange.
    const grid = createGrid(12);
    grid.dir = 'rtl';

    // Act.
    keydown(grid.children[0], 'ArrowLeft');

    // Assert.
    assertFocus(grid.children[1]);
  });

  test('left wrap around focuses first item in RTL', () => {
    // Arrange.
    const grid = createGrid(12);
    grid.dir = 'rtl';

    // Act.
    keydown(grid.children[grid.children.length - 1], 'ArrowLeft');

    // Assert.
    assertFocus(grid.children[0]);
  });


  const test_params = [
    {
      type: 'full',
      size: 10,
      columns: 5,
    },
    {
      type: 'non-full',
      size: 8,
      columns: 5,
    },
  ];

  test_params.forEach(function(param) {
    test(`down focuses below item for ${param.type} grid`, () => {
      // Arrange.
      const grid = createGrid(param.size);
      grid.columns = param.columns;

      // Act.
      keydown(grid.children[0], 'ArrowDown');

      // Assert.
      assertFocus(grid.children[param.columns]);
    });

    test(`last column down focuses below item for ${param.type} grid`, () => {
      // Arrange.
      const grid = createGrid(param.size);
      grid.columns = param.columns;

      // Act.
      keydown(grid.children[param.columns - 1], 'ArrowDown');

      // Assert.
      const focusedIndex =
          (param.size % param.columns === 0 ? param.size : param.columns) - 1;
      assertFocus(grid.children[focusedIndex]);
    });

    test(`up focuses above item for ${param.type} grid`, () => {
      // Arrange.
      const grid = createGrid(param.size);
      grid.columns = param.columns;

      // Act.
      keydown(grid.children[param.columns], 'ArrowUp');

      // Assert.
      assertFocus(grid.children[0]);
    });

    test(`last column up focuses above item for ${param.type} grid`, () => {
      // Arrange.
      const grid = createGrid(param.size);
      grid.columns = param.columns;

      // Act.
      keydown(grid.children[param.columns - 1], 'ArrowUp');

      // Assert.
      const focusedIndex =
          (param.size % param.columns === 0 ? param.size : param.columns) - 1;
      assertFocus(grid.children[focusedIndex]);
    });

    test(`down wrap around focuses top item for ${param.type} grid`, () => {
      // Arrange.
      const grid = createGrid(param.size);
      grid.columns = param.columns;

      // Act.
      keydown(grid.children[param.columns], 'ArrowDown');

      // Assert.
      assertFocus(grid.children[0]);
    });

    test(`up wrap around focuses bottom item for ${param.type} grid`, () => {
      // Arrange.
      const grid = createGrid(param.size);
      grid.columns = param.columns;

      // Act.
      keydown(grid.children[0], 'ArrowDown');

      // Assert.
      assertFocus(grid.children[param.columns]);
    });
  });

  test('enter clicks focused item', async () => {
    // Arrange.
    const grid = createGrid(1);
    grid.children[0].focus();
    const itemClicked = eventToPromise('click', grid.children[0]);

    // Act.
    keydown(grid.children[0], 'Enter');

    // Assert.
    await itemClicked;
  });

  test('space clicks focused item', async () => {
    // Arrange.
    const grid = createGrid(1);
    grid.children[0].focus();
    const itemClicked = eventToPromise('click', grid.children[0]);

    // Act.
    keydown(grid.children[0], ' ');

    // Assert.
    await itemClicked;
  });
});
