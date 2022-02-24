// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {CriticalErrorPage} from 'chrome://shimless-rma/critical_error_page.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {setShimlessRmaServiceForTesting} from 'chrome://shimless-rma/mojo_interface_provider.js';
import {ShimlessRma} from 'chrome://shimless-rma/shimless_rma.js';

import {assertFalse, assertTrue} from '../../chai_assert.js';
import {flushTasks} from '../../test_util.js';

export function criticalErrorPageTest() {
  /**
   * ShimlessRma is needed to handle the 'disable-all-buttons' event used by the
   * shutdown buttons.
   * @type {?ShimlessRma}
   */
  let shimless_rma_component = null;

  /** @type {?CriticalErrorPage} */
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
    shimless_rma_component.remove();
    shimless_rma_component = null;
    service.reset();
  });

  /**
   * @return {!Promise}
   */
  function initializeCriticalErrorPage() {
    assertFalse(!!component);

    shimless_rma_component =
        /** @type {!ShimlessRma} */ (document.createElement('shimless-rma'));
    assertTrue(!!shimless_rma_component);
    document.body.appendChild(shimless_rma_component);

    component = /** @type {!CriticalErrorPage} */ (
        document.createElement('critical-error-page'));
    assertTrue(!!component);
    document.body.appendChild(component);

    return flushTasks();
  }

  // Verify the buttons are disabled when the Exit to Login button is clicked.
  test('ClickExitToLoginButton', async () => {
    await initializeCriticalErrorPage();
    assertTrue(!!component);
    component.addEventListener('disable-all-buttons', (e) => {
      component.allButtonsDisabled = e.detail;
    });

    const resolver = new PromiseResolver();
    let allButtonsDisabled = false;
    component.addEventListener('disable-all-buttons', (e) => {
      allButtonsDisabled = e.detail;
      resolver.resolve();
    });

    component.shadowRoot.querySelector('#exitToLoginButton').click();

    await resolver.promise;
    assertTrue(allButtonsDisabled);
    assertTrue(
        component.shadowRoot.querySelector('#exitToLoginButton').disabled);
  });

  // Verify the buttons are disabled when the Reboot button is clicked.
  test('ClickRebootButton', async () => {
    await initializeCriticalErrorPage();
    assertTrue(!!component);
    component.addEventListener('disable-all-buttons', (e) => {
      component.allButtonsDisabled = e.detail;
    });

    const resolver = new PromiseResolver();
    let allButtonsDisabled = false;
    component.addEventListener('disable-all-buttons', (e) => {
      allButtonsDisabled = e.detail;
      resolver.resolve();
    });

    component.shadowRoot.querySelector('#rebootButton').click();

    await resolver.promise;
    assertTrue(allButtonsDisabled);
    assertTrue(component.shadowRoot.querySelector('#rebootButton').disabled);
  });
}
