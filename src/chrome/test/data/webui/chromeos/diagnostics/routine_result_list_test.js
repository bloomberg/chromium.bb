// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/routine_result_list.js';

import {RoutineName, StandardRoutineResult} from 'chrome://diagnostics/diagnostics_types.js';
import {ExecutionProgress, ResultStatusItem} from 'chrome://diagnostics/routine_list_executor.js';

import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function routineResultListTestSuite() {
  /** @type {?RoutineResultListElement} */
  let routineResultListElement = null;

  setup(function() {
    document.body.innerHTML = '';
  });

  teardown(function() {
    if (routineResultListElement) {
      routineResultListElement.remove();
    }
    routineResultListElement = null;
  });

  /**
   * Initializes the routine-result-list and sets the list of routines.
   * @param {!Array<!RoutineName>} routines
   * @return {!Promise}
   */
  function initializeRoutineResultList(routines) {
    assertFalse(!!routineResultListElement);

    // Add the entry to the DOM.
    routineResultListElement = /** @type {!RoutineResultListElement} */ (
        document.createElement('routine-result-list'));
    assertTrue(!!routineResultListElement);
    document.body.appendChild(routineResultListElement);

    // Initialize the routines.
    routineResultListElement.initializeTestRun(routines);

    return flushTasks();
  }

  /**
   * Clear the routine-result-list.
   * @return {!Promise}
   */
  function clearRoutineResultList() {
    routineResultListElement.clearRoutines();
    return flushTasks();
  }

  /**
   * Returns an array of the entries in the list.
   * @return {!NodeList<!RoutineResultEntryElement>}
   */
  function getEntries() {
    return dx_utils.getResultEntries(routineResultListElement);
  }

  test('ElementRendered', () => {
    return initializeRoutineResultList([]).then(() => {
      // Verify the element rendered.
      let div = routineResultListElement.$$('#resultListContainer');
      assertTrue(!!div);
    });
  });

  test('HideElement', () => {
    return initializeRoutineResultList([])
        .then(() => {
          assertFalse(routineResultListElement.hidden);
          assertFalse(
              routineResultListElement.$$('#resultListContainer').hidden);
          routineResultListElement.hidden = true;
          return flushTasks();
        })
        .then(() => {
          assertTrue(routineResultListElement.hidden);
          assertTrue(
              routineResultListElement.$$('#resultListContainer').hidden);
        });
  });

  test('EmptyByDefault', () => {
    return initializeRoutineResultList([]).then(() => {
      assertEquals(0, getEntries().length);
    });
  });

  test('InitializedRoutines', () => {
    /** @type {!Array<!RoutineName>} */
    const routines = [
      RoutineName.kCpuCache,
      RoutineName.kFloatingPoint,
    ];

    return initializeRoutineResultList(routines).then(() => {
      assertEquals(routines.length, getEntries().length);
      getEntries().forEach((entry, index) => {
        // Routines are initialized in the unstarted state.
        let status = new ResultStatusItem(routines[index]);
        status.progress = ExecutionProgress.kNotStarted;
        assertDeepEquals(status, entry.item);
      });
    });
  });

  test('InitializeThenClearRoutines', () => {
    /** @type {!Array<!RoutineName>} */
    const routines = [
      RoutineName.kCpuCache,
      RoutineName.kFloatingPoint,
    ];

    return initializeRoutineResultList(routines)
        .then(() => {
          assertEquals(routines.length, getEntries().length);
          return clearRoutineResultList();
        })
        .then(() => {
          // List is empty after clearing.
          assertEquals(0, getEntries().length);
        });
  });

  test('VerifyStatusUpdates', () => {
    /** @type {!Array<!RoutineName>} */
    const routines = [
      RoutineName.kCpuCache,
      RoutineName.kFloatingPoint,
    ];

    return initializeRoutineResultList(routines).then(() => {
      // Verify the starting state.
      assertEquals(routines.length, getEntries().length);
      getEntries().forEach((entry, index) => {
        // Routines are initialized in the unstarted state.
        let status = new ResultStatusItem(routines[index]);
        status.progress = ExecutionProgress.kNotStarted;
        assertDeepEquals(status, entry.item);
      });

      let status = new ResultStatusItem(routines[0]);
      status.progress = ExecutionProgress.kRunning;
      routineResultListElement.onStatusUpdate(status);
      return flushTasks()
          .then(() => {
            // Verify first routine is running.
            assertEquals(
                ExecutionProgress.kRunning, getEntries()[0].item.progress);
            assertEquals(null, getEntries()[0].item.result);

            // Move the first routine to completed state.
            status = new ResultStatusItem(routines[0]);
            status.progress = ExecutionProgress.kCompleted;
            status.result = {simpleResult: StandardRoutineResult.kTestPassed};
            routineResultListElement.onStatusUpdate(status);

            return flushTasks();
          })
          .then(() => {
            // Verify the first routine is completed.
            assertEquals(
                ExecutionProgress.kCompleted, getEntries()[0].item.progress);
            assertNotEquals(null, getEntries()[0].item.result);
            assertEquals(
                StandardRoutineResult.kTestPassed,
                getEntries()[0].item.result.simpleResult);

            status = new ResultStatusItem(routines[1]);
            status.progress = ExecutionProgress.kRunning;
            routineResultListElement.onStatusUpdate(status);

            return flushTasks();
          })
          .then(() => {
            // Verify second routine is running.
            assertEquals(
                ExecutionProgress.kRunning, getEntries()[1].item.progress);
            assertEquals(null, getEntries()[1].item.result);

            // Move the second routine to completed state.
            status = new ResultStatusItem(routines[1]);
            status.progress = ExecutionProgress.kCompleted;
            status.result = {simpleResult: StandardRoutineResult.kTestPassed};
            routineResultListElement.onStatusUpdate(status);

            return flushTasks();
          })
          .then(() => {
            // Verify the second routine is completed.
            assertEquals(
                ExecutionProgress.kCompleted, getEntries()[1].item.progress);
            assertNotEquals(null, getEntries()[1].item.result);
            assertEquals(
                StandardRoutineResult.kTestPassed,
                getEntries()[0].item.result.simpleResult);

            return flushTasks();
          });
    });
  });
}
