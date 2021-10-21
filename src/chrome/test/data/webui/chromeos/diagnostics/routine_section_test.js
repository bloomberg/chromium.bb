// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://diagnostics/routine_section.js';

import {RoutineType, StandardRoutineResult} from 'chrome://diagnostics/diagnostics_types.js';
import {createRoutine} from 'chrome://diagnostics/diagnostics_utils.js';
import {fakePowerRoutineResults, fakeRoutineResults} from 'chrome://diagnostics/fake_data.js';
import {FakeSystemRoutineController} from 'chrome://diagnostics/fake_system_routine_controller.js';
import {setSystemRoutineControllerForTesting} from 'chrome://diagnostics/mojo_interface_provider.js';
import {RoutineGroup} from 'chrome://diagnostics/routine_group.js';
import {ExecutionProgress, TestSuiteStatus} from 'chrome://diagnostics/routine_list_executor.js';
import {getRoutineType} from 'chrome://diagnostics/routine_result_entry.js';
import {BadgeType} from 'chrome://diagnostics/text_badge.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.js';

import * as dx_utils from './diagnostics_test_utils.js';

export function routineSectionTestSuite() {
  /** @type {?RoutineSectionElement} */
  let routineSectionElement = null;

  /** @type {!FakeSystemRoutineController} */
  let routineController;

  /** @type {function(this:Performance): number} */
  const originalTime = performance.now;

  setup(function() {
    document.body.innerHTML = '';

    // Setup a fake routine controller so that nothing resolves unless
    // done explicitly.
    routineController = new FakeSystemRoutineController();
    routineController.setDelayTimeInMillisecondsForTesting(-1);

    // Enable all routines by default.
    routineController.setFakeSupportedRoutines(
        [...fakeRoutineResults.keys(), ...fakePowerRoutineResults.keys()]);

    setSystemRoutineControllerForTesting(routineController);
  });

  teardown(function() {
    if (routineSectionElement) {
      routineSectionElement.remove();
    }
    routineSectionElement = null;
  });

  /**
   * Initializes the element and sets the routines.
   * @param {!Array<!RoutineType|RoutineGroup>} routines
   * @param {number=} runtime in minutes.
   */
  function initializeRoutineSection(routines, runtime = 1) {
    assertFalse(!!routineSectionElement);

    // Add the entry to the DOM.
    routineSectionElement = /** @type {!RoutineSectionElement} */ (
        document.createElement('routine-section'));
    assertTrue(!!routineSectionElement);
    document.body.appendChild(routineSectionElement);

    // Assign the routines to the property.
    routineSectionElement.routines = routines;
    routineSectionElement.testSuiteStatus = TestSuiteStatus.kNotRunning;
    routineSectionElement.routineRuntime = runtime;

    if (!(routines[0] instanceof RoutineGroup) && routines.length === 1 && [
          RoutineType.kBatteryDischarge, RoutineType.kBatteryCharge
        ].includes(routines[0])) {
      routineSectionElement.isPowerRoutine = true;
    }

    return flushTasks();
  }

  /**
   * Returns the result list element.
   * @return {!RoutineResultListElement}
   */
  function getResultList() {
    const resultList = dx_utils.getResultList(routineSectionElement);
    assertTrue(!!resultList);
    return resultList;
  }

  /**
   * Returns the Run Tests button.
   * @return {!CrButtonElement}
   */
  function getRunTestsButton() {
    const button = dx_utils.getRunTestsButtonFromSection(routineSectionElement);
    assertTrue(!!button);
    return button;
  }

  /**
   * Returns the Stop tests button.
   * @return {!CrButtonElement}
   */
  function getStopTestsButton() {
    const button =
        dx_utils.getStopTestsButtonFromSection(routineSectionElement);
    assertTrue(!!button);
    return button;
  }

  /**
   * Returns the Show/Hide Test Report button.
   * @return {!CrButtonElement}
   */
  function getToggleTestReportButton() {
    const button =
        dx_utils.getToggleTestReportButtonFromSection(routineSectionElement);
    assertTrue(!!button);
    return button;
  }

  /**
   * Returns the status badge.
   * @return {!TextBadgeElement}
   */
  function getStatusBadge() {
    return /** @type {!TextBadgeElement} */ (
        routineSectionElement.$$('#testStatusBadge'));
  }

  /**
   * Returns the status text.
   * @return {!HTMLElement}
   */
  function getStatusTextElement() {
    const statusText =
        /** @type {!HTMLElement} */ (
            routineSectionElement.$$('#testStatusText'));
    assertTrue(!!statusText);
    return statusText;
  }

  /**
   * Returns whether the run tests button is disabled.
   * @return {boolean}
   */
  function isRunTestsButtonDisabled() {
    return getRunTestsButton().disabled;
  }

  /**
   * Clicks the run tests button.
   * @return {!Promise}
   */
  function clickRunTestsButton() {
    getRunTestsButton().click();
    return flushTasks();
  }

  /**
   * Clicks the stop tests button.
   * @return {!Promise}
   */
  function clickStopTestsButton() {
    getStopTestsButton().click();
    return flushTasks();
  }

  /**
   * Clicks the show/hide test report button.
   * @return {!Promise}
   */
  function clickToggleTestReportButton() {
    getToggleTestReportButton().click();
    return flushTasks();
  }

  /**
   * @suppress {visibility}
   * @return {string}
   */
  function getAnnouncedText() {
    assertTrue(!!routineSectionElement);

    return routineSectionElement.announcedText_;
  }

  /**
   * Returns an array of the entries in the list.
   * @return {!NodeList<!RoutineResultEntryElement>}
   */
  function getEntries() {
    return dx_utils.getResultEntries(getResultList());
  }

  /**
   * Returns whether the "result list" section is expanded or not.
   * @return {boolean}
   */
  function isIronCollapseOpen() {
    return routineSectionElement.$.collapse.opened;
  }

  /**
   * Get currentTestName_ private member for testing.
   * @suppress {visibility} // access private member
   * @return {string}
   */
  function getCurrentTestName() {
    assertTrue(!!routineSectionElement);
    return routineSectionElement.currentTestName_;
  }

  /**
   * @param {number} t Set current time to t.
   */
  function setMockTime(t) {
    performance.now = () => t;
  }

  /**
   * Restores mocked time to the original function.
   */
  function resetMockTime() {
    performance.now = originalTime;
  }

  /**
   * Updates time-to-finish status
   * @suppress {visibility} // access private member for test
   * @return {!Promise}
   */
  function triggerStatusUpdate() {
    routineSectionElement.setRunningStatusBadgeText_();
    return flushTasks();
  }

  /**
   * @param {boolean} isActive
   * @return {!Promise}
   */
  function setIsActive(isActive) {
    routineSectionElement.isActive = isActive;
    return flushTasks();
  }

  /**
   * @param {boolean} runTestsAutomatically
   * @return {!Promise}
   */
  function setRunTestsAutomatically(runTestsAutomatically) {
    routineSectionElement.runTestsAutomatically = runTestsAutomatically;
    return flushTasks();
  }

  /**
   * @param {!Array<!RoutineType>} routines
   * @return {!Promise}
   */
  function setRoutines(routines) {
    routineSectionElement.routines = routines;
    return flushTasks();
  }

  /**
   * @param {boolean} hideRoutineStatus
   * @return {!Promise}
   */
  function setHideRoutineStatus(hideRoutineStatus) {
    routineSectionElement.hideRoutineStatus = hideRoutineStatus;
    return flushTasks();
  }

  /**
   * Returns the learn more button.
   * @return {!CrButtonElement}
   */
  function getLearnMoreButton() {
    const learnMoreButton =
        /** @type {!CrButtonElement} */ (
            routineSectionElement.$$('#learnMoreButton'));
    assertTrue(!!learnMoreButton);
    return learnMoreButton;
  }

  test('ElementRenders', () => {
    return initializeRoutineSection([]).then(() => {
      // Verify the element rendered.
      assertTrue(!!routineSectionElement.$$('#routineSection'));
    });
  });

  test('ClickButtonShowsStopTest', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      RoutineType.kCpuCache,
      RoutineType.kCpuFloatingPoint,
    ];

    return initializeRoutineSection(routines)
        .then(() => {
          assertFalse(isRunTestsButtonDisabled());
          assertEquals(
              TestSuiteStatus.kNotRunning,
              routineSectionElement.testSuiteStatus);
          return clickRunTestsButton();
        })
        .then(() => {
          assertFalse(isVisible(getRunTestsButton()));
          assertTrue(isVisible(getStopTestsButton()));
          assertEquals(
              TestSuiteStatus.kRunning, routineSectionElement.testSuiteStatus);
          dx_utils.assertElementContainsText(
              getStopTestsButton(),
              loadTimeData.getString('stopTestButtonText'));
        });
  });

  test('ResultListToggleButton', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      RoutineType.kCpuCache,
      RoutineType.kCpuFloatingPoint,
    ];

    return initializeRoutineSection(routines)
        .then(() => {
          // Hidden by default.
          assertFalse(isIronCollapseOpen());
          assertFalse(isVisible(getToggleTestReportButton()));
          return clickRunTestsButton();
        })
        .then(() => {
          // Report and toggle button are visible.
          assertTrue(isIronCollapseOpen());
          assertTrue(isVisible(getToggleTestReportButton()));
          return clickToggleTestReportButton();
        })
        .then(() => {
          // Report is hidden when button is clicked again.
          assertFalse(isIronCollapseOpen());
          assertTrue(isVisible(getToggleTestReportButton()));
        });
  });

  test('PowerResultListToggleButton', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      RoutineType.kBatteryCharge,
    ];

    return initializeRoutineSection(routines)
        .then(() => {
          // Hidden by default.
          assertFalse(isIronCollapseOpen());
          assertFalse(isVisible(getToggleTestReportButton()));
          return clickRunTestsButton();
        })
        .then(() => {
          // Report is hidden by default and so is toggle button.
          assertFalse(isIronCollapseOpen());
          assertFalse(isVisible(getToggleTestReportButton()));
        });
  });

  test('ClickButtonInitializesResultList', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      RoutineType.kCpuCache,
      RoutineType.kCpuFloatingPoint,
    ];

    return initializeRoutineSection(routines)
        .then(() => {
          // No result entries initially.
          assertEquals(0, getEntries().length);
          return clickRunTestsButton();
        })
        .then(() => {
          const entries = getEntries();
          assertEquals(routines.length, entries.length);

          // First routine should be running.
          assertEquals(routines[0], entries[0].item.routine);
          assertEquals(ExecutionProgress.kRunning, entries[0].item.progress);

          // Second routine is not started.
          assertEquals(routines[1], entries[1].item.routine);
          assertEquals(ExecutionProgress.kNotStarted, entries[1].item.progress);

          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => {
          return flushTasks();
        })
        .then(() => {
          const entries = getEntries();
          assertEquals(routines.length, entries.length);

          // First routine should be completed.
          assertEquals(routines[0], entries[0].item.routine);
          assertEquals(ExecutionProgress.kCompleted, entries[0].item.progress);

          // Second routine should be running.
          assertEquals(routines[1], entries[1].item.routine);
          assertEquals(ExecutionProgress.kRunning, entries[1].item.progress);

          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => {
          return flushTasks();
        })
        .then(() => {
          const entries = getEntries();
          assertEquals(routines.length, entries.length);

          // First routine should be completed.
          assertEquals(routines[0], entries[0].item.routine);
          assertEquals(ExecutionProgress.kCompleted, entries[0].item.progress);

          // Second routine should be completed.
          assertEquals(routines[1], entries[1].item.routine);
          assertEquals(ExecutionProgress.kCompleted, entries[1].item.progress);
        });
  });

  test('ResultListFiltersBySupported', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      RoutineType.kCpuCache,
      RoutineType.kMemory,
    ];

    routineController.setFakeStandardRoutineResult(
        RoutineType.kMemory, StandardRoutineResult.kTestPassed);
    routineController.setFakeStandardRoutineResult(
        RoutineType.kCpuCache, StandardRoutineResult.kTestPassed);
    routineController.setFakeSupportedRoutines([RoutineType.kMemory]);

    return initializeRoutineSection(routines)
        .then(() => {
          return clickRunTestsButton();
        })
        .then(() => {
          const entries = getEntries();
          assertEquals(1, entries.length);
          assertEquals(RoutineType.kMemory, entries[0].item.routine);
          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => {
          return flushTasks();
        })
        .then(() => {
          const entries = getEntries();
          assertEquals(1, entries.length);
          assertEquals(RoutineType.kMemory, entries[0].item.routine);
        });
  });

  test('ResultListStatusSuccess', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      RoutineType.kMemory,
    ];

    routineController.setFakeStandardRoutineResult(
        RoutineType.kMemory, StandardRoutineResult.kTestPassed);

    return initializeRoutineSection(routines)
        .then(() => {
          // Hidden by default.
          assertFalse(isVisible(getStatusBadge()));
          assertFalse(isVisible(getStatusTextElement()));
          return clickRunTestsButton();
        })
        .then(() => {
          // Badge is visible with test running.
          assertFalse(getStatusBadge().hidden);
          assertEquals(getStatusBadge().badgeType, BadgeType.RUNNING);
          dx_utils.assertTextContains(
              getStatusBadge().value,
              loadTimeData.getString('routineRemainingMinFinal'));

          // Text is visible describing which test is being run.
          assertFalse(getStatusTextElement().hidden);
          dx_utils.assertElementContainsText(
              getStatusTextElement(),
              loadTimeData.getString('memoryRoutineText').toLowerCase());

          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => {
          return flushTasks();
        })
        .then(() => {
          // Badge is visible with success.
          assertFalse(getStatusBadge().hidden);
          assertEquals(getStatusBadge().badgeType, BadgeType.SUCCESS);
          assertEquals(getStatusBadge().value, 'PASSED');

          // Text is visible saying test succeeded.
          assertFalse(getStatusTextElement().hidden);
          dx_utils.assertElementContainsText(
              getStatusTextElement(), 'Test succeeded');
          dx_utils.assertElementContainsText(
              getStatusTextElement(), 'Learn more');
        });
  });

  test('ResultListStatusFail', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      RoutineType.kCpuFloatingPoint,
      RoutineType.kCpuCache,
    ];

    routineController.setFakeStandardRoutineResult(
        RoutineType.kCpuFloatingPoint, StandardRoutineResult.kTestFailed);
    routineController.setFakeStandardRoutineResult(
        RoutineType.kCpuCache, StandardRoutineResult.kTestPassed);

    return initializeRoutineSection(routines)
        .then(() => {
          // Hidden by default.
          assertTrue(getStatusBadge().hidden);
          assertTrue(getStatusTextElement().hidden);
          return clickRunTestsButton();
        })
        .then(() => {
          // Badge is visible with test running.
          assertFalse(getStatusBadge().hidden);
          assertEquals(getStatusBadge().badgeType, BadgeType.RUNNING);
          dx_utils.assertTextContains(
              getStatusBadge().value,
              loadTimeData.getString('routineRemainingMinFinal'));

          // Text is visible describing which test is being run.
          assertFalse(getStatusTextElement().hidden);
          dx_utils.assertElementContainsText(
              getStatusTextElement(),
              loadTimeData.getString('cpuFloatingPointAccuracyRoutineText')
                  .toLowerCase());

          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => {
          return flushTasks();
        })
        .then(() => {
          // Badge is still visible with "test running", even though first one
          // failed.
          assertFalse(getStatusBadge().hidden);
          assertEquals(getStatusBadge().badgeType, BadgeType.RUNNING);
          dx_utils.assertTextContains(
              getStatusBadge().value,
              loadTimeData.getString('routineRemainingMinFinal'));

          // Text is visible describing which test is being run.
          assertFalse(getStatusTextElement().hidden);
          dx_utils.assertElementContainsText(
              getStatusTextElement(),
              loadTimeData.getString('cpuCacheRoutineText').toLowerCase());

          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => {
          return flushTasks();
        })
        .then(() => {
          // Badge is visible with fail.
          assertFalse(getStatusBadge().hidden);
          assertEquals(getStatusBadge().badgeType, BadgeType.ERROR);
          assertEquals(getStatusBadge().value, 'FAILED');

          // Text is visible saying test failed.
          assertFalse(getStatusTextElement().hidden);
          dx_utils.assertElementContainsText(
              getStatusTextElement(), 'Test failed');
          dx_utils.assertElementContainsText(
              getStatusTextElement(), 'Learn more');
        });
  });

  test('CancelQueuedRoutinesWithRoutineCompleted', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      RoutineType.kCpuCache,
      RoutineType.kCpuStress,
    ];
    routineController.setFakeStandardRoutineResult(
        RoutineType.kCpuCache, StandardRoutineResult.kTestPassed);
    routineController.setFakeStandardRoutineResult(
        RoutineType.kCpuStress, StandardRoutineResult.kTestPassed);

    return initializeRoutineSection(routines)
        .then(() => clickRunTestsButton())
        .then(() => {
          const entries = getEntries();
          assertEquals(routines.length, entries.length);

          // First routine should be running.
          assertEquals(routines[0], entries[0].item.routine);
          assertEquals(ExecutionProgress.kRunning, entries[0].item.progress);

          // Second routine is not started.
          assertEquals(routines[1], entries[1].item.routine);
          assertEquals(ExecutionProgress.kNotStarted, entries[1].item.progress);
          // // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          const entries = getEntries();
          // First routine should be completed.
          assertEquals(ExecutionProgress.kCompleted, entries[0].item.progress);

          // Second routine should be running.
          assertEquals(ExecutionProgress.kRunning, entries[1].item.progress);
        })
        .then(() => clickStopTestsButton())
        .then(() => {
          const entries = getEntries();
          // First routine should still be completed.
          assertEquals(ExecutionProgress.kCompleted, entries[0].item.progress);
          // Second routine should be cancelled.
          assertEquals(ExecutionProgress.kCancelled, entries[1].item.progress);

          // Badge and status are visible.
          assertTrue(isVisible(getStatusBadge()));
          assertTrue(isVisible(getStatusTextElement()));

          // Badge shows that test was stopped.
          assertEquals(getStatusBadge().badgeType, BadgeType.STOPPED);
          dx_utils.assertTextContains(
              getStatusBadge().value,
              loadTimeData.getString('testStoppedBadgeText'));

          // Status text shows test that was cancelled.
          assertTrue(isVisible(getStatusTextElement()));
          dx_utils.assertElementContainsText(
              getStatusTextElement(),
              loadTimeData.getStringF(
                  'testCancelledText', getCurrentTestName()));
        });
  });

  test('CancelRunningAndQueuedRoutines', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      RoutineType.kCpuCache,
      RoutineType.kCpuStress,
    ];
    routineController.setFakeStandardRoutineResult(
        RoutineType.kCpuCache, StandardRoutineResult.kTestPassed);
    routineController.setFakeStandardRoutineResult(
        RoutineType.kCpuStress, StandardRoutineResult.kTestPassed);

    return initializeRoutineSection(routines)
        .then(() => clickRunTestsButton())
        .then(() => {
          // Badge and status are visible.
          assertTrue(isVisible(getStatusBadge()));
          assertTrue(isVisible(getStatusTextElement()));

          const entries = getEntries();
          // First routine should be running.
          assertEquals(routines[0], entries[0].item.routine);
          assertEquals(ExecutionProgress.kRunning, entries[0].item.progress);

          // Second routine is not started.
          assertEquals(routines[1], entries[1].item.routine);
          assertEquals(ExecutionProgress.kNotStarted, entries[1].item.progress);
        })
        // Stop running test.
        .then(() => clickStopTestsButton())
        .then(() => {
          // Badge and status are still visible.
          assertTrue(isVisible(getStatusBadge()));
          assertTrue(isVisible(getStatusTextElement()));

          const entries = getEntries();
          // First routine should be cancelled.
          assertEquals(ExecutionProgress.kCancelled, entries[0].item.progress);
          // Second routine should be cancelled.
          assertEquals(ExecutionProgress.kCancelled, entries[1].item.progress);

          // Status text shows test that was cancelled.
          dx_utils.assertElementContainsText(
              getStatusTextElement(),
              loadTimeData.getStringF(
                  'testCancelledText', getCurrentTestName()));
        });
  });

  test('RunAgainShownAfterCancellation', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      RoutineType.kCpuCache,
      RoutineType.kCpuStress,
    ];
    routineController.setFakeStandardRoutineResult(
        RoutineType.kCpuCache, StandardRoutineResult.kTestPassed);
    routineController.setFakeStandardRoutineResult(
        RoutineType.kCpuStress, StandardRoutineResult.kTestPassed);

    return initializeRoutineSection(routines)
        // Start tests.
        .then(() => clickRunTestsButton())
        // Stop running test.
        .then(() => clickStopTestsButton())
        .then(() => {
          // Badge and status are visible.
          assertTrue(isVisible(getStatusBadge()));
          assertTrue(isVisible(getStatusTextElement()));

          const entries = getEntries();
          // First routine should be cancelled.
          assertEquals(ExecutionProgress.kCancelled, entries[0].item.progress);
          // Second routine should be cancelled.
          assertEquals(ExecutionProgress.kCancelled, entries[1].item.progress);

          // Status text shows test that was cancelled.
          dx_utils.assertElementContainsText(
              getStatusTextElement(),
              loadTimeData.getStringF(
                  'testCancelledText', getCurrentTestName()));
          // Button is visible and text shows "Run again"
          assertTrue(isVisible(getRunTestsButton()));
          dx_utils.assertElementContainsText(
              getRunTestsButton(),
              loadTimeData.getString('runAgainButtonText'));
        });
  });

  test('RunTestsMultipleTimes', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      RoutineType.kCpuCache,
    ];
    routineController.setFakeStandardRoutineResult(
        RoutineType.kCpuCache, StandardRoutineResult.kTestPassed);

    return initializeRoutineSection(routines)
        .then(() => clickRunTestsButton())
        .then(() => routineController.resolveRoutineForTesting())
        .then(() => flushTasks())
        .then(() => {
          // Badge and status are visible.
          assertTrue(isVisible(getStatusBadge()));
          assertTrue(isVisible(getStatusTextElement()));

          const entries = getEntries();
          // First routine should be completed.
          assertEquals(routines[0], entries[0].item.routine);
          assertEquals(ExecutionProgress.kCompleted, entries[0].item.progress);

          // Status text shows that a routine succeeded.
          dx_utils.assertElementContainsText(
              getStatusTextElement(), loadTimeData.getString('testSuccess'));
          // Button is visible and text shows "Run again"
          assertTrue(isVisible(getRunTestsButton()));
          dx_utils.assertElementContainsText(
              getRunTestsButton(),
              loadTimeData.getString('runAgainButtonText'));
          return clickRunTestsButton();
        })
        .then(() => {
          // Badge and status are visible.
          assertTrue(isVisible(getStatusBadge()));
          assertTrue(isVisible(getStatusTextElement()));

          const entries = getEntries();
          // First routine should be running.
          assertEquals(ExecutionProgress.kRunning, entries[0].item.progress);

          // Button text should be "Stop test"
          dx_utils.assertElementContainsText(
              getStopTestsButton(),
              loadTimeData.getString('stopTestButtonText'));

          // Status text shows test that is running.
          dx_utils.assertElementContainsText(
              getStatusTextElement(),
              loadTimeData.getStringF(
                  'routineNameText', getCurrentTestName().toLowerCase()));
        });
  });

  test('ReportButtonHiddenWithSingleRoutine', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      RoutineType.kCpuCache,
    ];
    return initializeRoutineSection(routines)
        .then(() => clickRunTestsButton())
        .then(() => {
          assertFalse(isVisible(getToggleTestReportButton()));
        });
  });

  test('ReportButtonShownWithMultipleRoutines', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      RoutineType.kCpuCache,
      RoutineType.kCpuStress,
    ];
    return initializeRoutineSection(routines)
        .then(() => clickRunTestsButton())
        .then(() => {
          assertTrue(isVisible(getToggleTestReportButton()));
        });
  });

  test('RoutineRuntimeStatus', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      RoutineType.kMemory,
    ];

    routineController.setFakeStandardRoutineResult(
        RoutineType.kMemory, StandardRoutineResult.kTestPassed);

    setMockTime(0);

    return initializeRoutineSection(routines, 2)
        .then(() => {
          return clickRunTestsButton();
        })
        .then(() => {
          // Badge is visible with test running.
          assertTrue(isVisible(getStatusBadge()));
          assertEquals(getStatusBadge().badgeType, BadgeType.RUNNING);
          dx_utils.assertTextContains(
              getStatusBadge().value,
              loadTimeData.getStringF('routineRemainingMin', '2'));

          return triggerStatusUpdate();
        })
        .then(() => {
          dx_utils.assertTextContains(getStatusBadge().value, '2');

          setMockTime(110000);  // fast forward time to 110 seconds
          return triggerStatusUpdate();
        })
        .then(() => {
          // Display 'less than a minute remaining'
          dx_utils.assertTextContains(
              getStatusBadge().value,
              loadTimeData.getString('routineRemainingMinFinal'));

          resetMockTime();
        });
  });

  test('RoutineRuntimeStatusLarge', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [
      RoutineType.kMemory,
    ];

    routineController.setFakeStandardRoutineResult(
        RoutineType.kMemory, StandardRoutineResult.kTestPassed);

    setMockTime(0);

    return initializeRoutineSection(routines, 20)
        .then(() => {
          return clickRunTestsButton();
        })
        .then(() => {
          // Badge is visible with test running.
          assertTrue(isVisible(getStatusBadge()));
          assertEquals(getStatusBadge().badgeType, BadgeType.RUNNING);
          dx_utils.assertTextContains(
              getStatusBadge().value,
              loadTimeData.getStringF('routineRemainingMin', '20'));

          return triggerStatusUpdate();
        })
        .then(() => {
          // Should say about 20 minutes remaining.
          dx_utils.assertTextContains(getStatusBadge().value, '20');

          setMockTime(120000);  // set time to 120 seconds
          return triggerStatusUpdate();
        })
        .then(() => {
          // Should still say about 20 minutes remaining.
          dx_utils.assertTextContains(getStatusBadge().value, '20');

          setMockTime(1020000);  // set time to 17 minutes
          return triggerStatusUpdate();
        })
        .then(() => {
          // Should say about 5 minutes remaining.
          dx_utils.assertTextContains(getStatusBadge().value, '5');

          setMockTime(1500000);  // set time to 25 minutes (past estimate)
          return triggerStatusUpdate();
        })
        .then(() => {
          // Should say about 5 minutes remaining, even after estimated runtime.
          dx_utils.assertTextContains(getStatusBadge().value, '5');

          // Should update status to just a few more minutes..
          dx_utils.assertElementContainsText(
              getStatusTextElement(),
              loadTimeData.getString('routineRemainingMinFinalLarge'));
          resetMockTime();
        });
  });

  test('PageChangeStopsRunningTest', () => {
    /** @type {!Array<!RoutineType>} */
    const routines = [RoutineType.kMemory];

    routineController.setFakeStandardRoutineResult(
        RoutineType.kMemory, StandardRoutineResult.kTestPassed);
    return initializeRoutineSection(routines)
        .then(() => clickRunTestsButton())
        .then(() => {
          // Badge is visible with test running.
          assertFalse(getStatusBadge().hidden);
          assertEquals(getStatusBadge().badgeType, BadgeType.RUNNING);
          dx_utils.assertTextContains(
              getStatusBadge().value,
              loadTimeData.getString('routineRemainingMinFinal'));

          // Text is visible describing which test is being run.
          assertFalse(getStatusTextElement().hidden);
          dx_utils.assertElementContainsText(
              getStatusTextElement(),
              loadTimeData.getString('memoryRoutineText').toLowerCase());

          // Simulate a navigation page change event.
          return setIsActive(false);
        })
        .then(() => flushTasks())
        .then(() => {
          // Result list is no longer visible.
          assertFalse(isVisible(getResultList()));
          // Memory routine should be cancelled.
          assertEquals(
              ExecutionProgress.kCancelled, getEntries()[0].item.progress);
        });
  });

  test('RoutineStatusAndActionsHidden', () => {
    return initializeRoutineSection([])
        .then(() => setHideRoutineStatus(true))
        .then(() => {
          assertFalse(isVisible(getLearnMoreButton()));
          assertFalse(isVisible(/** @type {!HTMLElement} */ (
              routineSectionElement.$$('.routine-status-container'))));
          assertFalse(isVisible(/** @type {!HTMLElement} */ (
              routineSectionElement.$$('.button-container'))));
        });
  });


  test('StopAfterFirstBlockingFailureInRoutineGroup', () => {
    let localNetworkGroup = new RoutineGroup(
        [
          createRoutine(RoutineType.kGatewayCanBePinged, true),
          createRoutine(RoutineType.kLanConnectivity, true)
        ],
        'localNetworkGroupLabel');

    let nameResolutionGroup = new RoutineGroup(
        [createRoutine(RoutineType.kDnsResolverPresent, true)],
        'nameResolutionGroupLabel');
    let groups = [localNetworkGroup, nameResolutionGroup];
    routineController.setFakeStandardRoutineResult(
        RoutineType.kGatewayCanBePinged, StandardRoutineResult.kTestPassed);
    routineController.setFakeStandardRoutineResult(
        RoutineType.kLanConnectivity, StandardRoutineResult.kTestFailed);
    routineController.setFakeStandardRoutineResult(
        RoutineType.kDnsResolverPresent, StandardRoutineResult.kTestPassed);

    return initializeRoutineSection(groups)
        .then(() => clickRunTestsButton())
        .then(() => {
          const entries = getEntries();

          // First routine should be running.
          assertEquals(
              RoutineType.kGatewayCanBePinged, entries[0].item.routines[0]);
          assertEquals(ExecutionProgress.kRunning, entries[0].item.progress);

          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          const entries = getEntries();

          // Second routine in the first group should be running.
          assertEquals(
              RoutineType.kLanConnectivity, entries[0].item.routines[1]);
          assertEquals(ExecutionProgress.kRunning, entries[0].item.progress);

          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          const entries = getEntries();
          // We've encountered a test failure which means we should no longer
          // update the status of our remaining routine result entries.
          assertTrue(getResultList().ignoreRoutineStatusUpdates);

          // Second routine in the first group should have completed.

          assertEquals(
              RoutineType.kLanConnectivity, entries[0].item.routines[1]);
          assertEquals(ExecutionProgress.kCompleted, entries[0].item.progress);

          // Text badge should display 'FAILED' for the first group.
          const textBadge = entries[0].shadowRoot.querySelector('#status');
          dx_utils.assertElementContainsText(
              textBadge.$$('#textBadge'), 'FAILED');

          // Remaining routine groups should display the skipped state.
          assertEquals(ExecutionProgress.kSkipped, entries[1].item.progress);

          // Remaining routine should still be running in the background.
          assertEquals(
              routineSectionElement.testSuiteStatus, TestSuiteStatus.kRunning);

          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          // All tests are completed and the ignore updates flag should be off
          // again.
          assertEquals(
              routineSectionElement.testSuiteStatus,
              TestSuiteStatus.kCompleted);
          assertFalse(getResultList().ignoreRoutineStatusUpdates);
        });
  });

  test('NonBlockingRoutineFailureHandledCorrectly', () => {
    let localNetworkGroup = new RoutineGroup(
        [
          createRoutine(RoutineType.kSignalStrength, false),
          createRoutine(RoutineType.kCaptivePortal, false),
        ],
        'wifiGroupLabel');

    let nameResolutionGroup = new RoutineGroup(
        [createRoutine(RoutineType.kDnsResolverPresent, true)],
        'nameResolutionGroupLabel');
    let groups = [localNetworkGroup, nameResolutionGroup];
    routineController.setFakeStandardRoutineResult(
        RoutineType.kSignalStrength, StandardRoutineResult.kTestFailed);
    routineController.setFakeStandardRoutineResult(
        RoutineType.kCaptivePortal, StandardRoutineResult.kTestPassed);
    routineController.setFakeStandardRoutineResult(
        RoutineType.kDnsResolverPresent, StandardRoutineResult.kTestPassed);

    return initializeRoutineSection(groups)
        .then(() => clickRunTestsButton())
        .then(() => {
          const entries = getEntries();

          // First routine should be running.
          assertEquals(
              RoutineType.kSignalStrength, entries[0].item.routines[0]);
          assertEquals(ExecutionProgress.kRunning, entries[0].item.progress);

          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          const entries = getEntries();
          assertFalse(getResultList().ignoreRoutineStatusUpdates);

          // Second routine in the first group should still be running
          // despite the |kSignalStrength| routine failure.
          assertEquals(RoutineType.kCaptivePortal, entries[0].item.routines[1]);
          assertEquals(
              getCurrentTestName(),
              getRoutineType(entries[0].item.routines[1]));

          // Text badge should display 'WARNING' for the first group.
          const textBadge = entries[0].shadowRoot.querySelector('#status');
          dx_utils.assertElementContainsText(
              textBadge.$$('#textBadge'), 'WARNING');

          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          const entries = getEntries();

          // Text badge should still display 'WARNING' for the first group.
          const textBadge = entries[0].shadowRoot.querySelector('#status');
          dx_utils.assertElementContainsText(
              textBadge.$$('#textBadge'), 'WARNING');

          // First routine in the second group should be running.
          assertEquals(
              RoutineType.kDnsResolverPresent, entries[1].item.routines[0]);
          assertEquals(ExecutionProgress.kRunning, entries[1].item.progress);


          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          const entries = getEntries();

          // Text badge should display 'PASSED' for the second group.
          const textBadge = entries[1].shadowRoot.querySelector('#status');
          dx_utils.assertElementContainsText(
              textBadge.$$('#textBadge'), 'PASSED');
          assertEquals(ExecutionProgress.kCompleted, entries[1].item.progress);
          assertEquals(
              routineSectionElement.testSuiteStatus,
              TestSuiteStatus.kCompleted);
        });
  });

  test('MultipleNonBlockingTestsFail', () => {
    let groups = [new RoutineGroup(
        [
          createRoutine(RoutineType.kSignalStrength, false),
          createRoutine(RoutineType.kCaptivePortal, false),
        ],
        'wifiGroupLabel')];
    routineController.setFakeStandardRoutineResult(
        RoutineType.kSignalStrength, StandardRoutineResult.kTestFailed);
    routineController.setFakeStandardRoutineResult(
        RoutineType.kCaptivePortal, StandardRoutineResult.kTestFailed);

    return initializeRoutineSection(groups)
        .then(() => clickRunTestsButton())
        .then(() => {
          const entries = getEntries();
          // First routine should be running.
          assertEquals(
              RoutineType.kSignalStrength, entries[0].item.routines[0]);
          assertEquals(ExecutionProgress.kRunning, entries[0].item.progress);
          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          const entries = getEntries();
          assertFalse(getResultList().ignoreRoutineStatusUpdates);
          // Second routine in the first group should still be running
          // despite the |kSignalStrength| routine failure.
          assertEquals(RoutineType.kCaptivePortal, entries[0].item.routines[1]);
          assertEquals(
              getCurrentTestName(),
              getRoutineType(entries[0].item.routines[1]));
          // Text badge should display 'WARNING' for the first group.
          const textBadge = entries[0].shadowRoot.querySelector('#status');
          dx_utils.assertElementContainsText(
              textBadge.$$('#textBadge'), 'WARNING');
          // Failed test text should be set properly.
          assertEquals(entries[0].item.failedTest, RoutineType.kSignalStrength);
          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          const entries = getEntries();
          // Text badge should still display 'WARNING' for the first group.
          const textBadge = entries[0].shadowRoot.querySelector('#status');
          dx_utils.assertElementContainsText(
              textBadge.$$('#textBadge'), 'WARNING');
          // Failed test does not get overwritten.
          assertEquals(entries[0].item.failedTest, RoutineType.kSignalStrength);
        });
  });

  test('LastNonBlockingRoutineInGroupFails', () => {
    let groups = [new RoutineGroup(
        [
          createRoutine(RoutineType.kSignalStrength, false),
          createRoutine(RoutineType.kCaptivePortal, false),
        ],
        'wifiGroupLabel')];
    routineController.setFakeStandardRoutineResult(
        RoutineType.kSignalStrength, StandardRoutineResult.kTestPassed);
    routineController.setFakeStandardRoutineResult(
        RoutineType.kCaptivePortal, StandardRoutineResult.kTestFailed);

    return initializeRoutineSection(groups)
        .then(() => clickRunTestsButton())
        .then(() => {
          const entries = getEntries();
          // First routine should be running.
          assertEquals(
              RoutineType.kSignalStrength, entries[0].item.routines[0]);
          assertEquals(ExecutionProgress.kRunning, entries[0].item.progress);
          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          const entries = getEntries();
          assertEquals(RoutineType.kCaptivePortal, entries[0].item.routines[1]);
          assertEquals(
              getCurrentTestName(),
              getRoutineType(entries[0].item.routines[1]));
          // Text badge should display 'RUNNING' for the first group since
          // the signal strength test passed but we still have unfinished
          // routines in this group.
          const textBadge = entries[0].shadowRoot.querySelector('#status');
          dx_utils.assertElementContainsText(
              textBadge.$$('#textBadge'), 'RUNNING');
          // Failed test text should be unset.
          assertFalse(!!entries[0].item.failedTest);
          // Resolve the running test.
          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          const entries = getEntries();
          // Text badge should display 'WARNING' despite being in a completed
          // state.
          const textBadge = entries[0].shadowRoot.querySelector('#status');
          dx_utils.assertElementContainsText(
              textBadge.$$('#textBadge'), 'WARNING');
          assertEquals(entries[0].item.progress, ExecutionProgress.kCompleted);
          // Failed test text should be set properly.
          assertEquals(entries[0].item.failedTest, RoutineType.kCaptivePortal);
        });
  });

  test('AnnounceOnAllTestPassed', () => {
    let groups = [new RoutineGroup(
        [
          createRoutine(RoutineType.kSignalStrength, /* blocking */ false),
        ],
        'wifiGroupLabel')];
    routineController.setFakeStandardRoutineResult(
        RoutineType.kSignalStrength, StandardRoutineResult.kTestPassed);

    return initializeRoutineSection(groups)
        .then(() => clickRunTestsButton())
        .then(() => {
          assertEquals('', getAnnouncedText());

          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          assertEquals(
              routineSectionElement.testSuiteStatus,
              TestSuiteStatus.kCompleted);
          assertEquals('Diagnostics completed', getAnnouncedText());
        });
  });

  test('AnnounceOnAllNonBlockingTestPassed', () => {
    const groups = [
      new RoutineGroup(
          [
            createRoutine(RoutineType.kCaptivePortal, false),
          ],
          'wifiGroupLabel'),
      new RoutineGroup(
          [
            createRoutine(RoutineType.kDnsResolverPresent, true),
          ],
          'wifiGroupLabel')
    ];
    routineController.setFakeStandardRoutineResult(
        RoutineType.kCaptivePortal, StandardRoutineResult.kTestFailed);
    routineController.setFakeStandardRoutineResult(
        RoutineType.kDnsResolverPresent, StandardRoutineResult.kTestPassed);

    return initializeRoutineSection(groups)
        .then(() => clickRunTestsButton())
        .then(() => {
          assertEquals('', getAnnouncedText());

          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          assertEquals('', getAnnouncedText());

          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          assertEquals(
              routineSectionElement.testSuiteStatus,
              TestSuiteStatus.kCompleted);
          assertEquals('Diagnostics completed', getAnnouncedText());
        });
  });

  test('AnnounceOnBlockingTestFailed', () => {
    let groups = [new RoutineGroup(
        [
          createRoutine(RoutineType.kSignalStrength, /* blocking */ true),
        ],
        'wifiGroupLabel')];
    routineController.setFakeStandardRoutineResult(
        RoutineType.kSignalStrength, StandardRoutineResult.kTestFailed);

    return initializeRoutineSection(groups)
        .then(() => clickRunTestsButton())
        .then(() => {
          assertEquals('', getAnnouncedText());

          return routineController.resolveRoutineForTesting();
        })
        .then(() => flushTasks())
        .then(() => {
          assertEquals(
              routineSectionElement.testSuiteStatus,
              TestSuiteStatus.kCompleted);
          assertEquals('Diagnostics completed', getAnnouncedText());
        });
  });

  test('NoAnnounceOnBlockingTestCancelled', () => {
    let groups = [new RoutineGroup(
        [
          createRoutine(RoutineType.kSignalStrength, /* blocking */ true),
        ],
        'wifiGroupLabel')];
    routineController.setFakeStandardRoutineResult(
        RoutineType.kSignalStrength, StandardRoutineResult.kTestFailed);

    return initializeRoutineSection(groups)
        .then(() => clickRunTestsButton())
        .then(() => {
          assertEquals('', getAnnouncedText());

          return clickStopTestsButton();
        })
        .then(() => {
          assertEquals(
              routineSectionElement.testSuiteStatus,
              TestSuiteStatus.kNotRunning);
          assertEquals('', getAnnouncedText());
        });
  });
}
