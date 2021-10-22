// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ReimagingCalibrationRunPageElement} from 'chrome://shimless-rma/reimaging_calibration_run_page.js';
import {CalibrationOverallStatus, CalibrationStatus, ComponentType} from 'chrome://shimless-rma/shimless_rma_types.js';

import {assertDeepEquals, assertEquals, assertFalse, assertNotEquals, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function reimagingCalibrationRunPageTest() {
  /** @type {?ReimagingCalibrationRunPageElement} */
  let component = null;

  /** @type {?FakeShimlessRmaService} */
  let service = null;

  suiteSetup(() => {
    service = new FakeShimlessRmaService();
    setShimlessRmaServiceForTesting(service);
  });

  setup(() => {
    document.body.innerHTML = '';
  });

  teardown(() => {
    component.remove();
    component = null;
    service.reset();
  });

  /**
   * @return {!Promise}
   */
  function initializeCalibrationRunPage() {
    assertFalse(!!component);

    component = /** @type {!ReimagingCalibrationRunPageElement} */ (
        document.createElement('reimaging-calibration-run-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  test('Initializes', async () => {
    await initializeCalibrationRunPage();
    const statusMessage = component.shadowRoot.querySelector('#calibration');
    assertFalse(statusMessage.hidden);
  });

  test('NextButtonBeforeCalibrationCompleteFails', async () => {
    const resolver = new PromiseResolver();
    await initializeCalibrationRunPage();
    let calibrationCompleteCalls = 0;
    service.calibrationComplete = () => {
      calibrationCompleteCalls++;
      return resolver.promise;
    };

    let savedResult;
    let savedError;
    component.onNextButtonClick()
        .then((result) => savedResult = result)
        .catch((error) => savedError = error);
    await flushTasks();

    assertEquals(0, calibrationCompleteCalls);
    assertTrue(savedError instanceof Error);
    assertEquals(savedError.message, 'Calibration is not complete.');
    assertEquals(savedResult, undefined);
  });

  test('NextButtonAfterCalibrationCompleteTriggersContinue', async () => {
    const resolver = new PromiseResolver();
    await initializeCalibrationRunPage();
    let calibrationCompleteCalls = 0;
    service.calibrationComplete = () => {
      calibrationCompleteCalls++;
      return resolver.promise;
    };
    service.triggerCalibrationOverallObserver(
        CalibrationOverallStatus.kCalibrationOverallComplete, 0);
    await flushTasks();

    let expectedResult = {foo: 'bar'};
    let savedResult;
    component.onNextButtonClick().then((result) => savedResult = result);
    // Resolve to a distinct result to confirm it was not modified.
    resolver.resolve(expectedResult);
    await flushTasks();

    assertEquals(1, calibrationCompleteCalls);
    assertDeepEquals(savedResult, expectedResult);
  });

  test(
      'CalibrationOverallProgressRoundCompleteCallsContinueCalibration',
      async () => {
        const resolver = new PromiseResolver();
        await initializeCalibrationRunPage();
        let continueCalibrationCalls = 0;
        service.continueCalibration = () => {
          continueCalibrationCalls++;
          return resolver.promise;
        };
        service.triggerCalibrationOverallObserver(
            CalibrationOverallStatus.kCalibrationOverallCurrentRoundComplete,
            0);
        await flushTasks();

        assertEquals(1, continueCalibrationCalls);
      });

  test(
      'CalibrationOverallProgressRoundFailedCallsContinueCalibration',
      async () => {
        const resolver = new PromiseResolver();
        await initializeCalibrationRunPage();
        let continueCalibrationCalls = 0;
        service.continueCalibration = () => {
          continueCalibrationCalls++;
          return resolver.promise;
        };
        service.triggerCalibrationOverallObserver(
            CalibrationOverallStatus.kCalibrationOverallCurrentRoundFailed, 0);
        await flushTasks();

        assertEquals(1, continueCalibrationCalls);
      });

  test(
      'CalibrationOverallProgressIniitalizationFailedCallsContinueCalibration',
      async () => {
        const resolver = new PromiseResolver();
        await initializeCalibrationRunPage();
        let continueCalibrationCalls = 0;
        service.continueCalibration = () => {
          continueCalibrationCalls++;
          return resolver.promise;
        };
        service.triggerCalibrationOverallObserver(
            CalibrationOverallStatus.kCalibrationOverallInitializationFailed,
            0);
        await flushTasks();

        assertEquals(1, continueCalibrationCalls);
      });

  test('CalibrationProgressUpdatesStatusMessage', async () => {
    await initializeCalibrationRunPage();
    const statusMessage = component.shadowRoot.querySelector('#calibration');
    let message = statusMessage.innerHTML;
    service.triggerCalibrationObserver(
        {
          component: ComponentType.kGyroscope,
          status: CalibrationStatus.kCalibrationInProgress,
          progress: 0.5
        },
        0);
    await flushTasks();
    let message2 = statusMessage.innerHTML;
    assertNotEquals(message, statusMessage.innerHTML);
    service.triggerCalibrationObserver(
        {
          component: ComponentType.kLidAccelerometer,
          status: CalibrationStatus.kCalibrationWaiting,
          progress: 0.0
        },
        0);
    await flushTasks();
    assertNotEquals(message, statusMessage.innerHTML);
    assertNotEquals(message2, statusMessage.innerHTML);
  });
}
