// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ReimagingCalibrationSetupPageElement} from 'chrome://shimless-rma/reimaging_calibration_setup_page.js';
import {CalibrationComponentStatus, CalibrationSetupInstruction, CalibrationStatus, ComponentType} from 'chrome://shimless-rma/shimless_rma_types.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function reimagingCalibrationSetupPageTest() {
  /** @type {?ReimagingCalibrationSetupPageElement} */
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
   * @param {!CalibrationSetupInstruction} instructions
   * @return {!Promise}
   */
  function initializeCalibrationPage(instructions) {
    assertFalse(!!component);
    service.setGetCalibrationSetupInstructionsResult(instructions);

    component = /** @type {!ReimagingCalibrationSetupPageElement} */ (
        document.createElement('reimaging-calibration-setup-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  test('Initializes', async () => {
    await initializeCalibrationPage(
        CalibrationSetupInstruction
            .kCalibrationInstructionPlaceBaseOnFlatSurface);
    const instructions = component.shadowRoot.querySelector('#instructions');
    assertFalse(instructions.hidden);
  });

  test('NextButtonTriggersRunCalibrationStep', async () => {
    const resolver = new PromiseResolver();
    await initializeCalibrationPage(
        CalibrationSetupInstruction
            .kCalibrationInstructionPlaceBaseOnFlatSurface);
    let runCalibrationCalls = 0;
    service.runCalibrationStep = () => {
      runCalibrationCalls++;
      return resolver.promise;
    };

    let expectedResult = {foo: 'bar'};
    let savedResult;
    component.onNextButtonClick().then((result) => savedResult = result);
    // Resolve to a distinct result to confirm it was not modified.
    resolver.resolve(expectedResult);
    await flushTasks();

    assertEquals(1, runCalibrationCalls);
    assertDeepEquals(savedResult, expectedResult);
  });
}
