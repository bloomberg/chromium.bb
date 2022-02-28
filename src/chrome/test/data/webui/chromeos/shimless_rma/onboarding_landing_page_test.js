// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {fakeStates} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingLandingPage} from 'chrome://shimless-rma/onboarding_landing_page.js';
import {State} from 'chrome://shimless-rma/shimless_rma_types.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks, isVisible} from '../../test_util.js';


export function onboardingLandingPageTest() {
  /** @type {?OnboardingLandingPage} */
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
  function initializeLandingPage() {
    assertFalse(!!component);

    component = /** @type {!OnboardingLandingPage} */ (
        document.createElement('onboarding-landing-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  test('ComponentRenders', async () => {
    await initializeLandingPage();
    assertTrue(!!component);

    const basePage = component.shadowRoot.querySelector('base-page');
    assertTrue(!!basePage);
  });

  test('OnBoardingPageValidationNotCompleteNextDisabled', async () => {
    const resolver = new PromiseResolver();
    await initializeLandingPage();
    let callCounter = 0;
    service.beginFinalization = () => {
      callCounter++;
      return resolver.promise;
    };

    let savedError;
    component.onNextButtonClick().catch((err) => savedError = err);
    await flushTasks();

    assertEquals(0, callCounter);
    assertEquals('Hardware verification is not complete.', savedError.message);
  });

  test('OnBoardingPageValidationCompleteEnablesNextButton', async () => {
    await initializeLandingPage();
    let disableNextButtonEventFired = false;
    let disableNextButton = true;
    component.addEventListener('disable-next-button', (e) => {
      disableNextButtonEventFired = true;
      disableNextButton = e.detail;
    });

    service.triggerHardwareVerificationStatusObserver(true, '', 0);
    await flushTasks();
    assertTrue(disableNextButtonEventFired);
    assertFalse(disableNextButton);
  });

  test(
      'OnBoardingPageValidationCompleteOnNextCallsBeginFinalization',
      async () => {
        const resolver = new PromiseResolver();
        await initializeLandingPage();
        service.triggerHardwareVerificationStatusObserver(true, '', 0);
        await flushTasks();
        let callCounter = 0;
        service.beginFinalization = () => {
          callCounter++;
          return resolver.promise;
        };

        const expectedResult = {foo: 'bar'};
        let savedResult;
        component.onNextButtonClick().then((result) => savedResult = result);
        // Resolve to a distinct result to confirm it was not modified.
        resolver.resolve(expectedResult);
        await flushTasks();

        assertEquals(1, callCounter);
        assertDeepEquals(expectedResult, savedResult);
      });

  test('OnBoardingPageValidationSuccessCheckVisible', async () => {
    await initializeLandingPage();

    const busy = component.shadowRoot.querySelector('#busyIcon');
    const verification =
        component.shadowRoot.querySelector('#verificationIcon');
    const error = component.shadowRoot.querySelector('#errorMessage');
    assertTrue(isVisible(busy));
    assertFalse(isVisible(verification));
    assertFalse(isVisible(error));
  });

  test('OnBoardingPageValidationSuccessCheckVisible', async () => {
    await initializeLandingPage();
    service.triggerHardwareVerificationStatusObserver(true, '', 0);
    await flushTasks();

    const busy = component.shadowRoot.querySelector('#busyIcon');
    const verification =
        component.shadowRoot.querySelector('#verificationIcon');
    const error = component.shadowRoot.querySelector('#errorMessage');
    assertFalse(isVisible(busy));
    assertTrue(isVisible(verification));
    assertEquals('shimless-icon:check', verification.icon);
    assertFalse(isVisible(error));
  });

  test('OnBoardingPageValidationFailedWarningAndErrorVisible', async () => {
    await initializeLandingPage();
    service.triggerHardwareVerificationStatusObserver(false, 'FAILURE', 0);
    await flushTasks();

    const busy = component.shadowRoot.querySelector('#busyIcon');
    const verification =
        component.shadowRoot.querySelector('#verificationIcon');
    const error = component.shadowRoot.querySelector('#errorMessage');
    assertTrue(busy.hidden);
    assertTrue(isVisible(verification));
    assertEquals('shimless-icon:warning', verification.icon);
    assertTrue(isVisible(error));
    assertEquals('FAILURE', error.innerHTML);
  });
}
