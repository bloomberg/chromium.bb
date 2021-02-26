// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/memory_card.js';

import {MemoryUsage} from 'chrome://diagnostics/diagnostics_types.js';
import {fakeMemoryUsage} from 'chrome://diagnostics/fake_data.js';
import {FakeSystemDataProvider} from 'chrome://diagnostics/fake_system_data_provider.js';
import {setSystemDataProviderForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function memoryCardTestSuite() {
  /** @type {?MemoryCardElement} */
  let memoryElement = null;

  /** @type {?FakeSystemDataProvider} */
  let provider = null;

  suiteSetup(() => {
    provider = new FakeSystemDataProvider();
    setSystemDataProviderForTesting(provider);
  });

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    if (memoryElement) {
      memoryElement.remove();
    }
    memoryElement = null;
    provider.reset();
  });

  /**
   * @param {!Array<MemoryUsage>} memoryUsage
   * @return {!Promise}
   */
  function initializeMemoryCard(memoryUsage) {
    assertFalse(!!memoryElement);

    // Initialize the fake data.
    provider.setFakeMemoryUsage(memoryUsage);

    // Add the memory card to the DOM.
    memoryElement = /** @type {!MemoryCardElement} */ (
        document.createElement('memory-card'));
    assertTrue(!!memoryElement);
    document.body.appendChild(memoryElement);

    return flushTasks();
  }

  /**
   * Returns the routine-section from the card.
   * @return {!RoutineSectionElement}
   */
  function getRoutineSection() {
    const routineSection = /** @type {!RoutineSectionElement} */ (
        memoryElement.$$('routine-section'));
    assertTrue(!!routineSection);
    return routineSection;
  }

  /**
   * Returns the Run Tests button from inside the routine-section.
   * @return {!CrButtonElement}
   */
  function getRunTestsButton() {
    const button = dx_utils.getRunTestsButtonFromSection(getRoutineSection());
    assertTrue(!!button);
    return button;
  }

  /**
   * Returns whether the run tests button is disabled.
   * @return {boolean}
   */
  function isRunTestsButtonDisabled() {
    return getRunTestsButton().disabled;
  }

  test('MemoryCardPopulated', () => {
    return initializeMemoryCard(fakeMemoryUsage).then(() => {
      const dataPoints = dx_utils.getDataPointElements(memoryElement);
      assertEquals(fakeMemoryUsage[0].totalMemoryKib, dataPoints[0].value);
      assertEquals(fakeMemoryUsage[0].availableMemoryKib, dataPoints[1].value);

      const barChart = dx_utils.getPercentBarChartElement(memoryElement);
      const memInUse = fakeMemoryUsage[0].totalMemoryKib -
          fakeMemoryUsage[0].availableMemoryKib;
      assertEquals(fakeMemoryUsage[0].totalMemoryKib, barChart.max);
      assertEquals(memInUse, barChart.value);

      // Verify the routine section is in the card.
      assertTrue(!!getRoutineSection());
      assertTrue(!!getRunTestsButton());
      assertFalse(isRunTestsButtonDisabled());
    });
  });
}
