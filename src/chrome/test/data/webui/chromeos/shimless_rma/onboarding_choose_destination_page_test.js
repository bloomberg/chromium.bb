// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {OnboardingChooseDestinationPageElement} from 'chrome://shimless-rma/onboarding_choose_destination_page.js';

import {assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function onboardingChooseDestinationPageTest() {
  /** @type {?OnboardingChooseDestinationPageElement} */
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
  function initializeChooseDestinationPage() {
    assertFalse(!!component);

    component = /** @type {!OnboardingChooseDestinationPageElement} */ (
        document.createElement('onboarding-choose-destination-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  test('ChooseDestinationPageInitializes', async () => {
    await initializeChooseDestinationPage();
    const originalOwnerComponent =
        component.shadowRoot.querySelector('#destinationOriginalOwner');
    const newOwnerComponent =
        component.shadowRoot.querySelector('#destinationNewOwner');

    assertFalse(originalOwnerComponent.checked);
    assertFalse(newOwnerComponent.checked);
  });

  test('ChooseDestinationPageOneChoiceOnly', async () => {
    await initializeChooseDestinationPage();
    const originalOwnerComponent =
        component.shadowRoot.querySelector('#destinationOriginalOwner');
    const newOwnerComponent =
        component.shadowRoot.querySelector('#destinationNewOwner');

    originalOwnerComponent.click();
    await flushTasks;

    assertTrue(originalOwnerComponent.checked);
    assertFalse(newOwnerComponent.checked);

    newOwnerComponent.click();
    await flushTasks;

    assertFalse(originalOwnerComponent.checked);
    assertTrue(newOwnerComponent.checked);
  });

  test('ChooseDestinationPageSameOwnerOnNextCallsSetSameOwner', async () => {
    const resolver = new PromiseResolver();
    await initializeChooseDestinationPage();
    service.setSameOwner = () => resolver.promise;
    const originalOwnerComponent =
        component.shadowRoot.querySelector('#destinationOriginalOwner');

    originalOwnerComponent.click();
    await flushTasks;
    assertTrue(originalOwnerComponent.checked);

    assertEquals(component.onNextButtonClick(), resolver.promise);
  });

  test(
      'ChooseDestinationPageDifferentOwnerOnNextCallsSetDifferentOwner',
      async () => {
        const resolver = new PromiseResolver();
        await initializeChooseDestinationPage();
        service.setDifferentOwner = () => resolver.promise;
        const newOwnerComponent =
            component.shadowRoot.querySelector('#destinationNewOwner');

        newOwnerComponent.click();
        await flushTasks;
        assertTrue(newOwnerComponent.checked);

        assertEquals(component.onNextButtonClick(), resolver.promise);
      });

  test('ChooseDestinationPageDisabledRadioGroup', async () => {
    await initializeChooseDestinationPage();

    const chooseDestinationGroup =
        component.shadowRoot.querySelector('#chooseDestinationGroup');
    assertFalse(chooseDestinationGroup.disabled);
    component.allButtonsDisabled = true;
    assertTrue(chooseDestinationGroup.disabled);
  });
}
