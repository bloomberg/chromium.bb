// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://crostini-upgrader/app.js';

import {BrowserProxy} from 'chrome://crostini-upgrader/browser_proxy.js';
import {TestBrowserProxy} from 'chrome://test/test_browser_proxy.m.js';
import {flushTasks} from 'chrome://test/test_util.m.js';

class FakePageHandler extends TestBrowserProxy {
  constructor() {
    super([
      'backup', 'startPrechecks', 'upgrade', 'restore', 'cancel',
      'cancelBeforeStart', 'onPageClosed', 'launch'
    ]);
  }

  /** @override */
  backup() {
    this.methodCalled('backup');
  }

  /** @override */
  startPrechecks() {
    this.methodCalled('startPrechecks');
  }

  /** @override */
  upgrade() {
    this.methodCalled('upgrade');
  }

  /** @override */
  restore() {
    this.methodCalled('restore');
  }

  /** @override */
  cancel() {
    this.methodCalled('cancel');
  }

  /** @override */
  cancelBeforeStart() {
    this.methodCalled('cancelBeforeStart');
  }

  /** @override */
  onPageClosed() {
    this.methodCalled('onPageClosed');
  }

  /** @override */
  launch() {
    this.methodCalled('launch');
  }
}

class FakeBrowserProxy {
  constructor() {
    this.handler = new FakePageHandler();
    this.callbackRouter =
        new chromeos.crostiniUpgrader.mojom.PageCallbackRouter();
    /** @type {appManagement.mojom.PageRemote} */
    this.page = this.callbackRouter.$.bindNewPipeAndPassRemote();
  }
}

suite('<crostini-upgrader-app>', () => {
  let fakeBrowserProxy;
  let app;

  setup(async () => {
    fakeBrowserProxy = new FakeBrowserProxy();
    BrowserProxy.instance_ = fakeBrowserProxy;

    app = document.createElement('crostini-upgrader-app');
    PolymerTest.clearBody();
    document.body.appendChild(app);

    await flushTasks();
  });

  teardown(function() {
    app.remove();
  });

  const clickButton = async (button) => {
    assertFalse(button.hidden);
    assertFalse(button.disabled);
    button.click();
    await flushTasks();
  };

  const getActionButton = () => {
    return app.$$('.action-button');
  };

  const getCancelButton = () => {
    return app.$$('.cancel-button');
  };

  const clickAction = async () => {
    await clickButton(getActionButton());
  };

  const clickCancel = async () => {
    await clickButton(getCancelButton());
  };

  const getBackupProgressBar = () => {
    return app.$$('#backup-progress-bar');
  };

  const getUpgradeProgressBar = () => {
    return app.$$('#upgrade-progress-bar');
  };

  const getRestoreProgressBar = () => {
    return app.$$('#restore-progress-bar');
  };

  const getProgressMessage = () => {
    return app.$$('#progress-message');
  };

  const waitMillis = (millis) => {
    return new Promise(resolve => {
      setTimeout(() => {
        resolve();
      }, millis);
    });
  };

  test('upgradeFlow', async () => {
    expectFalse(app.$$('#prompt-message').hidden);
    expectEquals(fakeBrowserProxy.handler.getCallCount('backup'), 0);

    // The page will not register that backup has started until the first
    // progress message.
    await clickAction();
    fakeBrowserProxy.page.onBackupProgress(0);
    await flushTasks();
    expectFalse(getProgressMessage().hidden);
    expectEquals(fakeBrowserProxy.handler.getCallCount('backup'), 1);
    expectTrue(getActionButton().hidden);
    expectTrue(getCancelButton().hidden);

    fakeBrowserProxy.page.onBackupProgress(50);
    await flushTasks();
    expectTrue(
        !!getProgressMessage().textContent, 'progress message should be set');
    expectFalse(getBackupProgressBar().hidden);
    expectEquals(app.$$('#backup-progress-bar > paper-progress').value, 50);

    fakeBrowserProxy.page.onBackupSucceeded();
    await flushTasks();
    expectEquals(fakeBrowserProxy.handler.getCallCount('upgrade'), 0);
    // The UI pauses for 2000 ms before continuing.
    await waitMillis(2010).then(flushTasks());
    fakeBrowserProxy.page.precheckStatus(
        chromeos.crostiniUpgrader.mojom.UpgradePrecheckStatus.OK);
    await flushTasks();

    expectEquals(fakeBrowserProxy.handler.getCallCount('upgrade'), 1);
    expectFalse(getUpgradeProgressBar().hidden);
    fakeBrowserProxy.page.onUpgradeProgress(['foo', 'bar']);
    fakeBrowserProxy.page.onUpgradeSucceeded();
    await flushTasks();

    expectEquals(fakeBrowserProxy.handler.getCallCount('onPageClosed'), 0);
    expectTrue(getRestoreProgressBar().hidden);

    await clickAction();
    expectEquals(fakeBrowserProxy.handler.getCallCount('launch'), 1);
    expectEquals(fakeBrowserProxy.handler.getCallCount('onPageClosed'), 1);
  });

  test('upgradeFlowFailureOffersRestore', async () => {
    expectFalse(app.$$('#prompt-message').hidden);
    expectEquals(fakeBrowserProxy.handler.getCallCount('backup'), 0);

    // The page will not register that backup has started until the first
    // progress message.
    await clickAction();
    fakeBrowserProxy.page.onBackupProgress(0);
    await flushTasks();
    expectFalse(getProgressMessage().hidden);
    expectEquals(fakeBrowserProxy.handler.getCallCount('backup'), 1);
    expectTrue(getActionButton().hidden);
    expectTrue(getCancelButton().hidden);

    fakeBrowserProxy.page.onBackupSucceeded();
    await flushTasks();
    expectEquals(fakeBrowserProxy.handler.getCallCount('upgrade'), 0);
    // The UI pauses for 2000 ms before continuing.
    await waitMillis(2010).then(flushTasks());

    const kMaxUpgradeAttempts = 3;
    for (let i = 0; i < kMaxUpgradeAttempts; i++) {
      fakeBrowserProxy.page.precheckStatus(
          chromeos.crostiniUpgrader.mojom.UpgradePrecheckStatus.OK);
      await flushTasks();
      expectEquals(fakeBrowserProxy.handler.getCallCount('upgrade'), i + 1);
      expectFalse(getUpgradeProgressBar().hidden);
      fakeBrowserProxy.page.onUpgradeProgress(['foo', 'bar']);
      fakeBrowserProxy.page.onUpgradeFailed();
      await flushTasks();
    }

    expectEquals(fakeBrowserProxy.handler.getCallCount('restore'), 0);
    await clickAction();
    expectEquals(fakeBrowserProxy.handler.getCallCount('restore'), 1);
    fakeBrowserProxy.page.onRestoreProgress(50);
    await flushTasks();
    expectTrue(
        !!getProgressMessage().textContent, 'progress message should be set');
    expectFalse(getRestoreProgressBar().hidden);
    expectEquals(app.$$('#restore-progress-bar > paper-progress').value, 50);
    fakeBrowserProxy.page.onRestoreSucceeded();
    await flushTasks();

    await clickAction();
    expectEquals(fakeBrowserProxy.handler.getCallCount('launch'), 1);
    expectEquals(fakeBrowserProxy.handler.getCallCount('onPageClosed'), 1);
  });
});
