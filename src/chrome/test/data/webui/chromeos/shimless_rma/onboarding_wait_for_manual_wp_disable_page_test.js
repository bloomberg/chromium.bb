// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingWaitForManualWpDisablePageElement} from 'chrome://shimless-rma/onboarding_wait_for_manual_wp_disable_page.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.m.js';

export function onboardingWaitForManualWpDisablePageTest() {
  /** @type {?OnboardingWaitForManualWpDisablePageElement} */
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
  function initializeWaitForManualWpDisablePage() {
    assertFalse(!!component);

    component = /** @type {!OnboardingWaitForManualWpDisablePageElement} */ (
        document.createElement('onboarding-wait-for-manual-wp-disable-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  test('WaitForManualWpDisablePageInitializes', async () => {
    await initializeWaitForManualWpDisablePage();
    const manualDisableComponent =
        component.shadowRoot.querySelector('#manuallyDisableHwwpInstructions');
    assertFalse(manualDisableComponent.hidden);
  });

  test('HwwpEnabledDisablesNext', async () => {
    await initializeWaitForManualWpDisablePage();

    let savedResult;
    let savedError;
    component.onNextButtonClick()
        .then((result) => savedResult = result)
        .catch((error) => savedError = error);
    await flushTasks();

    assertTrue(savedError instanceof Error);
    assertEquals(
        savedError.message, 'Hardware Write Protection is not disabled.');
    assertEquals(savedResult, undefined);
  });

  test('HwwpDisabledEnablesNext', async () => {
    const resolver = new PromiseResolver();
    await initializeWaitForManualWpDisablePage();
    service.triggerHardwareWriteProtectionObserver(false, 0);
    await flushTasks();
    service.transitionNextState = () => {
      return resolver.promise;
    };

    let expectedResult = {foo: 'bar'};
    let savedResult;
    component.onNextButtonClick().then((result) => savedResult = result);
    // Resolve to a distinct result to confirm it was not modified.
    resolver.resolve(expectedResult);
    await flushTasks();

    assertDeepEquals(savedResult, expectedResult);
  });
}
