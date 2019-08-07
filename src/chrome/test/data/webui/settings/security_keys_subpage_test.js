// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @implements {settings.SecurityKeysBrowserProxy} */
class TestSecurityKeysBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'startSetPIN',
      'setPIN',
      'reset',
      'completeReset',
      'close',
    ]);

    /**
     * A map from method names to a promise to return when that method is
     * called. (If no promise is installed, a never-resolved promise is
     * returned.)
     * @private {!Map<string, !Promise>}
     */
    this.promiseMap_ = new Map();
  }

  /**
   * @param {string} methodName
   * @param {!Promise} promise
   */
  setResponseFor(methodName, promise) {
    this.promiseMap_.set(methodName, promise);
  }

  /**
   * @param {string} methodName
   * @param {*} opt_arg
   * @return {!Promise}
   * @private
   */
  handleMethod_(methodName, opt_arg) {
    this.methodCalled(methodName, opt_arg);
    const promise = this.promiseMap_.get(methodName);
    if (promise != undefined) {
      this.promiseMap_.delete(methodName);
      return promise;
    }

    // Return a Promise that never resolves.
    return new Promise(() => {});
  }

  /** @override */
  startSetPIN() {
    return this.handleMethod_('startSetPIN');
  }

  /** @override */
  setPIN(oldPIN, newPIN) {
    return this.handleMethod_('setPIN', {oldPIN, newPIN});
  }

  /** @override */
  reset() {
    return this.handleMethod_('reset');
  }

  /** @override */
  completeReset() {
    return this.handleMethod_('completeReset');
  }

  /** @override */
  close() {
    this.methodCalled('close');
  }
}

suite('SecurityKeysResetDialog', function() {
  let dialog = null;

  setup(function() {
    browserProxy = new TestSecurityKeysBrowserProxy();
    settings.SecurityKeysBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    dialog = document.createElement('settings-security-keys-reset-dialog');
  });

  function assertShown(expectedID) {
    const allDivs = [
      'initial', 'noReset', 'resetFailed', 'reset2', 'resetSuccess',
      'resetNotAllowed'
    ];
    assertTrue(allDivs.includes(expectedID));

    const allShown =
        allDivs.filter(id => dialog.$[id].className == 'iron-selected');
    assertEquals(allShown.length, 1);
    assertEquals(allShown[0], expectedID);
  }

  function assertComplete() {
    assertEquals(dialog.$.button.textContent.trim(), 'OK');
    assertEquals(dialog.$.button.className, 'action-button');
  }

  function assertNotComplete() {
    assertEquals(dialog.$.button.textContent.trim(), 'Cancel');
    assertEquals(dialog.$.button.className, 'cancel-button');
  }

  test('Initialization', async function() {
    document.body.appendChild(dialog);
    await browserProxy.whenCalled('reset');
    assertShown('initial');
    assertNotComplete();
  });

  test('Cancel', async function() {
    document.body.appendChild(dialog);
    await browserProxy.whenCalled('reset');
    assertShown('initial');
    assertNotComplete();
    dialog.$.button.click();
    await browserProxy.whenCalled('close');
    assertFalse(dialog.$.dialog.open);
  });

  test('NotSupported', async function() {
    browserProxy.setResponseFor(
        'reset', Promise.resolve(1 /* INVALID_COMMAND */));
    document.body.appendChild(dialog);

    await browserProxy.whenCalled('reset');
    await browserProxy.whenCalled('close');
    assertComplete();
    assertShown('noReset');
  });

  test('ImmediateUnknownError', async function() {
    const error = 1000 /* undefined error code */;
    browserProxy.setResponseFor('reset', Promise.resolve(error));
    document.body.appendChild(dialog);

    await browserProxy.whenCalled('reset');
    await browserProxy.whenCalled('close');
    assertComplete();
    assertShown('resetFailed');
    assertTrue(
        dialog.$.resetFailed.textContent.trim().includes(error.toString()));
  });

  test('ImmediateUnknownError', async function() {
    browserProxy.setResponseFor('reset', Promise.resolve(0 /* success */));
    const promiseResolver = new PromiseResolver();
    browserProxy.setResponseFor('completeReset', promiseResolver.promise);
    document.body.appendChild(dialog);

    await browserProxy.whenCalled('reset');
    await browserProxy.whenCalled('completeReset');
    assertNotComplete();
    assertShown('reset2');
    promiseResolver.resolve(0 /* success */);
    await browserProxy.whenCalled('close');
    assertComplete();
    assertShown('resetSuccess');
  });

  test('UnknownError', async function() {
    const error = 1000 /* undefined error code */;
    browserProxy.setResponseFor('reset', Promise.resolve(0 /* success */));
    browserProxy.setResponseFor('completeReset', Promise.resolve(error));
    document.body.appendChild(dialog);

    await browserProxy.whenCalled('reset');
    await browserProxy.whenCalled('completeReset');
    await browserProxy.whenCalled('close');
    assertComplete();
    assertShown('resetFailed');
    assertTrue(
        dialog.$.resetFailed.textContent.trim().includes(error.toString()));
  });

  test('ResetRejected', async function() {
    browserProxy.setResponseFor('reset', Promise.resolve(0 /* success */));
    browserProxy.setResponseFor(
        'completeReset', Promise.resolve(48 /* NOT_ALLOWED */));
    document.body.appendChild(dialog);

    await browserProxy.whenCalled('reset');
    await browserProxy.whenCalled('completeReset');
    await browserProxy.whenCalled('close');
    assertComplete();
    assertShown('resetNotAllowed');
  });
});

suite('SecurityKeysSetPINDialog', function() {
  let dialog = null;

  setup(function() {
    browserProxy = new TestSecurityKeysBrowserProxy();
    settings.SecurityKeysBrowserProxyImpl.instance_ = browserProxy;
    PolymerTest.clearBody();
    dialog = document.createElement('settings-security-keys-set-pin-dialog');
  });

  function assertShown(expectedID) {
    const allDivs = [
      'initial',
      'noPINSupport',
      'pinPrompt',
      'success',
      'error',
      'locked',
      'reinsert',
    ];
    assertTrue(allDivs.includes(expectedID));

    const allShown =
        allDivs.filter(id => dialog.$[id].className == 'iron-selected');
    assertEquals(allShown.length, 1);
    assertEquals(allShown[0], expectedID);
  }

  function assertComplete() {
    assertEquals(dialog.$.closeButton.textContent.trim(), 'OK');
    assertEquals(dialog.$.closeButton.className, 'action-button');
    assertEquals(dialog.$.pinSubmit.hidden, true);
  }

  function assertNotComplete() {
    assertEquals(dialog.$.closeButton.textContent.trim(), 'Cancel');
    assertEquals(dialog.$.closeButton.className, 'cancel-button');
    assertEquals(dialog.$.pinSubmit.hidden, false);
  }

  test('Initialization', async function() {
    document.body.appendChild(dialog);
    await browserProxy.whenCalled('startSetPIN');
    assertShown('initial');
    assertNotComplete();
  });

  // Test error codes that are returned immediately.
  for (let testCase of [
           [1 /* INVALID_COMMAND */, 'noPINSupport'],
           [52 /* temporary lock */, 'reinsert'], [50 /* locked */, 'locked'],
           [1000 /* invalid error */, 'error']]) {
    test('ImmediateError' + testCase[0].toString(), async function() {
      browserProxy.setResponseFor(
          'startSetPIN',
          Promise.resolve([1 /* operation complete */, testCase[0]]));
      document.body.appendChild(dialog);

      await browserProxy.whenCalled('startSetPIN');
      await browserProxy.whenCalled('close');
      assertComplete();
      assertShown(testCase[1]);
      if (testCase[1] == 'error') {
        // Unhandled error codes display the numeric code.
        assertTrue(
            dialog.$.error.textContent.trim().includes(testCase[0].toString()));
      }
    });
  }

  test('ZeroRetries', async function() {
    // Authenticators can also signal that they are locked by indicating zero
    // attempts remaining.
    browserProxy.setResponseFor(
        'startSetPIN',
        Promise.resolve([0 /* not yet complete */, 0 /* no retries */]));
    document.body.appendChild(dialog);

    await browserProxy.whenCalled('startSetPIN');
    await browserProxy.whenCalled('close');
    assertComplete();
    assertShown('locked');
  });

  function setPINEntry(inputElement, pinValue) {
    inputElement.value = pinValue;
    // Dispatch input events to trigger validation and UI updates.
    inputElement.dispatchEvent(
        new CustomEvent('input', {bubbles: true, cancelable: true}));
  }

  function setNewPINEntry(pinValue) {
    setPINEntry(dialog.$.newPIN, pinValue);
  }

  function setNewPINEntries(pinValue, confirmPINValue) {
    setPINEntry(dialog.$.newPIN, pinValue);
    setPINEntry(dialog.$.confirmPIN, confirmPINValue);
  }

  function setChangePINEntries(currentPINValue, pinValue, confirmPINValue) {
    setNewPINEntries(pinValue, confirmPINValue);
    setPINEntry(dialog.$.currentPIN, currentPINValue);
  }

  test('SetPIN', async function() {
    const startSetPINResolver = new PromiseResolver();
    browserProxy.setResponseFor('startSetPIN', startSetPINResolver.promise);
    document.body.appendChild(dialog);
    const uiReady = test_util.eventToPromise('ui-ready', dialog);

    await browserProxy.whenCalled('startSetPIN');
    startSetPINResolver.resolve(
        [0 /* not yet complete */, null /* no current PIN */]);
    await uiReady;
    assertNotComplete();
    assertShown('pinPrompt');
    assertTrue(dialog.$.currentPINEntry.hidden);

    setNewPINEntries('123', '');
    assertTrue(dialog.$.pinSubmit.disabled);

    setNewPINEntries('123', '123');
    assertTrue(dialog.$.pinSubmit.disabled);

    setNewPINEntries('1234', '123');
    assertTrue(dialog.$.pinSubmit.disabled);

    setNewPINEntries('1234', '1234');
    assertFalse(dialog.$.pinSubmit.disabled);  // Note True -> False

    const setPINResolver = new PromiseResolver();
    browserProxy.setResponseFor('setPIN', setPINResolver.promise);
    dialog.$.pinSubmit.click();
    ({oldPIN, newPIN} = await browserProxy.whenCalled('setPIN'));
    assertTrue(dialog.$.pinSubmit.disabled);
    assertEquals(oldPIN, '');
    assertEquals(newPIN, '1234');

    setPINResolver.resolve([1 /* complete */, 0 /* success */]);
    await browserProxy.whenCalled('close');
    assertShown('success');
    assertComplete();
  });

  // Test error codes that are only returned after attempting to set a PIN.
  for (let testCase of [
           [52 /* temporary lock */, 'reinsert'], [50 /* locked */, 'locked'],
           [1000 /* invalid error */, 'error']]) {
    test('Error' + testCase[0].toString(), async function() {
      const startSetPINResolver = new PromiseResolver();
      browserProxy.setResponseFor('startSetPIN', startSetPINResolver.promise);
      document.body.appendChild(dialog);
      const uiReady = test_util.eventToPromise('ui-ready', dialog);

      await browserProxy.whenCalled('startSetPIN');
      startSetPINResolver.resolve(
          [0 /* not yet complete */, null /* no current PIN */]);
      await uiReady;
      setNewPINEntries('1234', '1234');

      browserProxy.setResponseFor(
          'setPIN', Promise.resolve([1 /* complete */, testCase[0]]));
      dialog.$.pinSubmit.click();
      await browserProxy.whenCalled('setPIN');
      await browserProxy.whenCalled('close');
      assertComplete();
      assertShown(testCase[1]);
      if (testCase[1] == 'error') {
        // Unhandled error codes display the numeric code.
        assertTrue(
            dialog.$.error.textContent.trim().includes(testCase[0].toString()));
      }
    });
  }

  test('ChangePIN', async function() {
    const startSetPINResolver = new PromiseResolver();
    browserProxy.setResponseFor('startSetPIN', startSetPINResolver.promise);
    document.body.appendChild(dialog);
    let uiReady = test_util.eventToPromise('ui-ready', dialog);

    await browserProxy.whenCalled('startSetPIN');
    startSetPINResolver.resolve(
        [0 /* not yet complete */, 2 /* two attempts */]);
    await uiReady;
    assertNotComplete();
    assertShown('pinPrompt');
    assertFalse(dialog.$.currentPINEntry.hidden);

    setChangePINEntries('123', '', '');
    assertTrue(dialog.$.pinSubmit.disabled);

    setChangePINEntries('123', '123', '');
    assertTrue(dialog.$.pinSubmit.disabled);

    setChangePINEntries('1234', '123', '1234');
    assertTrue(dialog.$.pinSubmit.disabled);

    setChangePINEntries('123', '1234', '1234');
    assertTrue(dialog.$.pinSubmit.disabled);

    setChangePINEntries('4321', '1234', '1234');
    assertFalse(dialog.$.pinSubmit.disabled);  // Note True -> False

    // Changing the new PIN so that it no longer matches the confirm PIN should
    // prevent submitting the dialog.
    setNewPINEntry('12345');
    assertTrue(dialog.$.pinSubmit.disabled);

    // Fixing the new PIN should be sufficient to address that.
    setNewPINEntry('1234');
    assertFalse(dialog.$.pinSubmit.disabled);

    let setPINResolver = new PromiseResolver();
    browserProxy.setResponseFor('setPIN', setPINResolver.promise);
    dialog.$.pinSubmit.click();
    let {oldPIN, newPIN} = await browserProxy.whenCalled('setPIN');
    assertShown('pinPrompt');
    assertNotComplete();
    assertTrue(dialog.$.pinSubmit.disabled);
    assertEquals(oldPIN, '4321');
    assertEquals(newPIN, '1234');

    // Simulate an incorrect PIN.
    uiReady = test_util.eventToPromise('ui-ready', dialog);
    setPINResolver.resolve([1 /* complete */, 49 /* incorrect PIN */]);
    await uiReady;
    // Text box for current PIN should be cleared.
    assertEquals(dialog.$.currentPIN.value, '');
    assertTrue(dialog.$.pinSubmit.disabled);

    setChangePINEntries('43211', '1234', '1234');
    assertFalse(dialog.$.pinSubmit.disabled);

    browserProxy.resetResolver('setPIN');
    setPINResolver = new PromiseResolver();
    browserProxy.setResponseFor('setPIN', setPINResolver.promise);
    dialog.$.pinSubmit.click();
    ({oldPIN, newPIN} = await browserProxy.whenCalled('setPIN'));
    assertTrue(dialog.$.pinSubmit.disabled);
    assertEquals(oldPIN, '43211');
    assertEquals(newPIN, '1234');

    setPINResolver.resolve([1 /* complete */, 0 /* success */]);
    await browserProxy.whenCalled('close');
    assertShown('success');
    assertComplete();
  });
});
