// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ReimagingProvisioningPage} from 'chrome://shimless-rma/reimaging_provisioning_page.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';
import {ProvisioningStatus} from 'chrome://shimless-rma/shimless_rma_types.js';
import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function reimagingProvisioningPageTest() {
  /**
   * ShimlessRma is needed to handle the 'transition-state' event used
   * when handling calibration overall progress signals.
   * @type {?ShimlessRma}
   */
  let shimless_rma_component = null;

  /** @type {?ReimagingProvisioningPage} */
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
    shimless_rma_component.remove();
    shimless_rma_component = null;
    component.remove();
    component = null;
    service.reset();
  });

  /**
   * @return {!Promise}
   */
  function initializeWaitForProvisioningPage() {
    assertFalse(!!component);

    shimless_rma_component =
        /** @type {!ShimlessRma} */ (document.createElement('shimless-rma'));
    assertTrue(!!shimless_rma_component);
    document.body.appendChild(shimless_rma_component);

    component = /** @type {!ReimagingProvisioningPage} */ (
        document.createElement('reimaging-provisioning-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  test('WaitForManualWpDisablePageInitializes', async () => {
    await initializeWaitForProvisioningPage();
    const provisioningComponent =
        component.shadowRoot.querySelector('#provisioningDeviceStatus');
    assertFalse(provisioningComponent.hidden);
  });

  test('ProvisioningStartingDisablesNext', async () => {
    await initializeWaitForProvisioningPage();

    let savedResult;
    let savedError;
    component.onNextButtonClick()
        .then((result) => savedResult = result)
        .catch((error) => savedError = error);
    await flushTasks();

    assertTrue(savedError instanceof Error);
    assertEquals(savedError.message, 'Provisioning is not complete.');
    assertEquals(savedResult, undefined);
  });

  test('ProvisioningInProgressDisablesNext', async () => {
    await initializeWaitForProvisioningPage();
    service.triggerProvisioningObserver(ProvisioningStatus.kInProgress, 0.5, 0);
    await flushTasks();

    let savedResult;
    let savedError;
    component.onNextButtonClick()
        .then((result) => savedResult = result)
        .catch((error) => savedError = error);
    await flushTasks();

    assertTrue(savedError instanceof Error);
    assertEquals(savedError.message, 'Provisioning is not complete.');
    assertEquals(savedResult, undefined);
  });

  test('ProvisioningEnablesNext', async () => {
    const resolver = new PromiseResolver();
    await initializeWaitForProvisioningPage();
    service.triggerProvisioningObserver(ProvisioningStatus.kComplete, 1.0, 0);
    await flushTasks();
    service.provisioningComplete = () => {
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

  test('ProvisioningFailedBlockingRetry', async () => {
    const resolver = new PromiseResolver();
    await initializeWaitForProvisioningPage();

    const retryButton =
        component.shadowRoot.querySelector('#retryProvisioningButton');
    assertTrue(retryButton.hidden);

    let callCount = 0;
    service.retryProvisioning = () => {
      callCount++;
      return resolver.promise;
    };
    service.triggerProvisioningObserver(
        ProvisioningStatus.kFailedBlocking, 1.0, 0);
    await flushTasks();

    assertFalse(retryButton.hidden);
    retryButton.click();

    await flushTasks();
    assertEquals(1, callCount);
  });

  test('ProvisioningFailedNonBlockingRetry', async () => {
    const resolver = new PromiseResolver();
    await initializeWaitForProvisioningPage();

    const retryButton =
        component.shadowRoot.querySelector('#retryProvisioningButton');
    assertTrue(retryButton.hidden);

    let callCount = 0;
    service.retryProvisioning = () => {
      callCount++;
      return resolver.promise;
    };
    service.triggerProvisioningObserver(
        ProvisioningStatus.kFailedNonBlocking, 1.0, 0);
    await flushTasks();

    assertFalse(retryButton.hidden);
    retryButton.click();

    await flushTasks();
    assertEquals(1, callCount);
  });

  test('ProvisioningFailedRetryDisabled', async () => {
    await initializeWaitForProvisioningPage();

    const retryButton =
        component.shadowRoot.querySelector('#retryProvisioningButton');
    assertFalse(retryButton.disabled);
    component.allButtonsDisabled = true;
    assertTrue(retryButton.disabled);
  });
}
