// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://scanning/page_size_select.js';

import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {getPageSizeString} from 'chrome://scanning/scanning_app_util.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

import {assertOrderedAlphabetically, changeSelect} from './scanning_app_test_utils.js';

const PageSize = {
  A4: chromeos.scanning.mojom.PageSize.kIsoA4,
  Letter: chromeos.scanning.mojom.PageSize.kNaLetter,
  Max: chromeos.scanning.mojom.PageSize.kMax,
};

export function pageSizeSelectTest() {
  /** @type {?PageSizeSelectElement} */
  let pageSizeSelect = null;

  setup(() => {
    pageSizeSelect = /** @type {!PageSizeSelectElement} */ (
        document.createElement('page-size-select'));
    assertTrue(!!pageSizeSelect);
    document.body.appendChild(pageSizeSelect);
  });

  teardown(() => {
    pageSizeSelect.remove();
    pageSizeSelect = null;
  });

  test('initializePageSizeSelect', () => {
    // Before options are added, the dropdown should be disabled and empty.
    const select =
        /** @type {!HTMLSelectElement} */ (pageSizeSelect.$$('select'));
    assertTrue(!!select);
    assertTrue(select.disabled);
    assertEquals(0, select.length);

    const firstPageSize = PageSize.A4;
    const secondPageSize = PageSize.Max;
    pageSizeSelect.pageSizes = [firstPageSize, secondPageSize];
    flush();

    // Verify that adding more than one page size results in the dropdown
    // becoming enabled with the correct options.
    assertFalse(select.disabled);
    assertEquals(2, select.length);
    assertEquals(
        getPageSizeString(firstPageSize), select.options[0].textContent.trim());
    assertEquals(
        getPageSizeString(secondPageSize),
        select.options[1].textContent.trim());
    assertEquals(firstPageSize.toString(), select.value);

    // Selecting a different option should update the selected value.
    return changeSelect(
               select, secondPageSize.toString(), /* selectedIndex */ null)
        .then(() => {
          assertEquals(
              secondPageSize.toString(), pageSizeSelect.selectedPageSize);
        });
  });

  test('pageSizeSelectDisabled', () => {
    const select = pageSizeSelect.$$('select');
    assertTrue(!!select);

    let pageSizeArr = [PageSize.Letter];
    pageSizeSelect.pageSizes = pageSizeArr;
    flush();

    // Verify the dropdown is disabled when there's only one option.
    assertEquals(1, select.length);
    assertTrue(select.disabled);

    pageSizeArr = pageSizeArr.concat([PageSize.A4]);
    pageSizeSelect.pageSizes = pageSizeArr;
    flush();

    // Verify the dropdown is enabled when there's more than one option.
    assertEquals(2, select.length);
    assertFalse(select.disabled);
  });

  test('pageSizesSortedCorrectly', () => {
    pageSizeSelect.pageSizes = [PageSize.Letter, PageSize.Max, PageSize.A4];
    flush();

    // Verify the page sizes are sorted alphabetically except for the fit to
    // scan area option, which should always be last. Verify that Letter is
    // selected by default.
    assertOrderedAlphabetically(
        pageSizeSelect.pageSizes.slice(0, pageSizeSelect.pageSizes.length - 1),
        (pageSize) => getPageSizeString(pageSize));
    assertEquals(
        PageSize.Max,
        pageSizeSelect.pageSizes[pageSizeSelect.pageSizes.length - 1]);
    assertEquals(PageSize.Letter.toString(), pageSizeSelect.selectedPageSize);
  });

  test('firstPageSizeUsedWhenDefaultNotAvailable', () => {
    pageSizeSelect.pageSizes = [PageSize.Max, PageSize.A4];
    flush();

    // Verify the first page size in the sorted page sizes array is selected by
    // default when Letter is not an available option.
    assertEquals(PageSize.A4.toString(), pageSizeSelect.selectedPageSize);
  });
}
