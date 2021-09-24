// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://os-settings/chromeos/os_settings.js';

// #import {flushTasks} from 'chrome://test/test_util.js';
// #import {setupFakeHandler, replaceBody, isHidden} from './test_util.m.js';
// #import {FakePageHandler} from 'chrome://os-settings/chromeos/os_settings.js';
// clang-format on

'use strict';

suite('<app-management-resize-lock-item>', () => {
  let resizeLockItem;
  let fakeHandler;

  setup(async () => {
    fakeHandler = setupFakeHandler();
    resizeLockItem = document.createElement('app-management-resize-lock-item');

    replaceBody(resizeLockItem);
    test_util.flushTasks();
  });

  test('Resize lock setting visibility', async () => {
    // Add an arc app with the default options, and make it the currently
    // selected app.
    const defaultArcOptions = {type: apps.mojom.AppType.kArc, permissions: {}};
    const defaultArcApp = await fakeHandler.addApp('app', defaultArcOptions);
    await fakeHandler.flushPipesForTesting();
    resizeLockItem.app = defaultArcApp;
    // The resize lock setting is hidden by default.
    assertTrue(isHidden(resizeLockItem));

    // Enable resize lock, but it's still hidden.
    const arcOptionsWithResizeLocked = {
      type: apps.mojom.AppType.kArc,
      resizeLocked: true,
    };
    const appWithResizeLocked =
        await fakeHandler.addApp(null, arcOptionsWithResizeLocked);
    await fakeHandler.flushPipesForTesting();
    resizeLockItem.app = appWithResizeLocked;
    assertTrue(isHidden(resizeLockItem));

    // Disable resize lock again, and it's still hidden.
    const arcOptionsWithoutResizeLocked = {
      type: apps.mojom.AppType.kArc,
      resizeLocked: false,
    };
    const appWithoutResizeLocked =
        await fakeHandler.addApp(null, arcOptionsWithoutResizeLocked);
    await fakeHandler.flushPipesForTesting();
    resizeLockItem.app = appWithoutResizeLocked;
    assertTrue(isHidden(resizeLockItem));

    // Setting |hideResizeLocked| to false shows the setting.
    const arcOptionsWithHideResizeLockedFalse = {
      type: apps.mojom.AppType.kArc,
      hideResizeLocked: false,
    };
    const appWithHideResizeLockedFalse =
        await fakeHandler.addApp(null, arcOptionsWithHideResizeLockedFalse);
    await fakeHandler.flushPipesForTesting();
    resizeLockItem.app = appWithHideResizeLockedFalse;
    assertFalse(isHidden(resizeLockItem));

    // Setting |hideResizeLocked| back to true hides the setting.
    const arcOptionsWithHideResizeLockedTrue = {
      type: apps.mojom.AppType.kArc,
      hideResizeLocked: true,
    };
    const appWithHideResizeLockedTrue =
        await fakeHandler.addApp(null, arcOptionsWithHideResizeLockedTrue);
    await fakeHandler.flushPipesForTesting();
    resizeLockItem.app = appWithHideResizeLockedTrue;
    assertTrue(isHidden(resizeLockItem));
  });
});
