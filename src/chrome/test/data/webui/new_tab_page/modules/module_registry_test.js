// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {ModuleDescriptor, ModuleRegistry, NewTabPageProxy, WindowProxy} from 'chrome://new-tab-page/new_tab_page.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {PromiseResolver} from 'chrome://resources/js/promise_resolver.m.js';
import {assertDeepEquals, assertEquals} from '../../chai_assert.js';
import {TestBrowserProxy} from '../../test_browser_proxy.js';
import {flushTasks} from '../../test_util.m.js';
import {fakeMetricsPrivate, MetricsTracker} from '../metrics_test_support.js';
import {createMock} from '../test_support.js';

/** @return {!TestBrowserProxy} */
function installMockWindowProxy() {
  const {mock, callTracker} = createMock(WindowProxy);
  WindowProxy.setInstance(mock);
  return callTracker;
}

/** @return {!TestBrowserProxy} */
function installMockHandler() {
  const {mock, callTracker} = createMock(newTabPage.mojom.PageHandlerRemote);
  NewTabPageProxy.setInstance(mock, new newTabPage.mojom.PageCallbackRouter());
  return callTracker;
}

suite('NewTabPageModulesModuleRegistryTest', () => {
  /** @type {!TestBrowserProxy} */
  let windowProxy;

  /** @type {!TestBrowserProxy} */
  let handler;

  /** @type {!newTabPage.mojom.PageRemote} */
  let callbackRouterRemote;

  /** @type {!MetricsTracker} */
  let metrics;

  setup(async () => {
    loadTimeData.overrideValues({navigationStartTime: 0.0});
    metrics = fakeMetricsPrivate();
    windowProxy = installMockWindowProxy();
    handler = installMockHandler();
    callbackRouterRemote = NewTabPageProxy.getInstance()
                               .callbackRouter.$.bindNewPipeAndPassRemote();
  });

  test('instantiates non-reordered modules', async () => {
    // Arrange.
    const fooModule =
        /** @type {!HTMLElement} */ (document.createElement('div'));
    const bazModule =
        /** @type {!HTMLElement} */ (document.createElement('div'));
    const bazModuleResolver = new PromiseResolver();
    const descriptors = [
      new ModuleDescriptor('foo', 'bli', () => Promise.resolve(fooModule)),
      new ModuleDescriptor('bar', 'blu', () => Promise.resolve(null)),
      new ModuleDescriptor('baz', 'bla', () => bazModuleResolver.promise),
      new ModuleDescriptor('buz', 'blo', () => Promise.resolve(fooModule)),
    ];
    windowProxy.setResultFor('now', 5.0);
    handler.setResultFor('getModulesOrder', Promise.resolve({
      moduleIds: [],
    }));

    // Act.
    const moduleRegistry = new ModuleRegistry(descriptors);
    const modulesPromise = moduleRegistry.initializeModules(0);
    callbackRouterRemote.setDisabledModules(false, ['buz']);
    // Wait for first batch of modules.
    await flushTasks();
    // Move time forward to test metrics.
    windowProxy.setResultFor('now', 123.0);
    // Delayed promise resolution to test async module instantiation.
    bazModuleResolver.resolve(bazModule);
    const modules = await modulesPromise;

    // Assert.
    assertEquals(1, handler.getCallCount('updateDisabledModules'));
    assertEquals(2, modules.length);
    assertEquals('foo', modules[0].descriptor.id);
    assertDeepEquals(fooModule, modules[0].element);
    assertEquals('baz', modules[1].descriptor.id);
    assertDeepEquals(bazModule, modules[1].element);
    assertEquals(2, metrics.count('NewTabPage.Modules.Loaded'));
    assertEquals(1, metrics.count('NewTabPage.Modules.Loaded', 5));
    assertEquals(1, metrics.count('NewTabPage.Modules.Loaded', 123));
    assertEquals(1, metrics.count('NewTabPage.Modules.Loaded.foo'));
    assertEquals(1, metrics.count('NewTabPage.Modules.Loaded.foo', 5));
    assertEquals(1, metrics.count('NewTabPage.Modules.Loaded.baz'));
    assertEquals(1, metrics.count('NewTabPage.Modules.Loaded.baz', 123));
    assertEquals(2, metrics.count('NewTabPage.Modules.LoadDuration'));
    assertEquals(1, metrics.count('NewTabPage.Modules.LoadDuration', 0));
    assertEquals(1, metrics.count('NewTabPage.Modules.LoadDuration', 118));
    assertEquals(1, metrics.count('NewTabPage.Modules.LoadDuration.foo'));
    assertEquals(1, metrics.count('NewTabPage.Modules.LoadDuration.foo', 0));
    assertEquals(1, metrics.count('NewTabPage.Modules.LoadDuration.baz'));
    assertEquals(1, metrics.count('NewTabPage.Modules.LoadDuration.baz', 118));
  });

  test('instantiates reordered modules without disabled modules', async () => {
    // Arrange.
    const fooModule =
        /** @type {!HTMLElement} */ (document.createElement('div'));
    const barModule =
        /** @type {!HTMLElement} */ (document.createElement('div'));
    const bazModule =
        /** @type {!HTMLElement} */ (document.createElement('div'));
    const descriptors = [
      new ModuleDescriptor('foo', 'bli', () => Promise.resolve(fooModule)),
      new ModuleDescriptor('bar', 'blu', () => Promise.resolve(barModule)),
      new ModuleDescriptor('baz', 'bla', () => Promise.resolve(bazModule)),
    ];
    handler.setResultFor('getModulesOrder', Promise.resolve({
      moduleIds: ['bar', 'baz', 'foo'],
    }));

    // Act.
    const moduleRegistry = new ModuleRegistry(descriptors);
    const modulesPromise = moduleRegistry.initializeModules(0);
    callbackRouterRemote.setDisabledModules(false, []);
    // Wait for first batch of modules.
    await flushTasks();
    const modules = await modulesPromise;

    // Assert.
    assertEquals(3, modules.length);
    assertEquals('bar', modules[0].descriptor.id);
    assertDeepEquals(barModule, modules[0].element);
    assertEquals('baz', modules[1].descriptor.id);
    assertDeepEquals(bazModule, modules[1].element);
    assertEquals('foo', modules[2].descriptor.id);
    assertDeepEquals(fooModule, modules[2].element);
  });

  test('instantiates reordered modules with disabled modules', async () => {
    // Arrange.
    const fooModule =
        /** @type {!HTMLElement} */ (document.createElement('div'));
    const barModule =
        /** @type {!HTMLElement} */ (document.createElement('div'));
    const bazModule =
        /** @type {!HTMLElement} */ (document.createElement('div'));
    const bizModule =
        /** @type {!HTMLElement} */ (document.createElement('div'));
    const buzModule =
        /** @type {!HTMLElement} */ (document.createElement('div'));
    const descriptors = [
      new ModuleDescriptor('foo', 'bli', () => Promise.resolve(fooModule)),
      new ModuleDescriptor('bar', 'blu', () => Promise.resolve(barModule)),
      new ModuleDescriptor('baz', 'bla', () => Promise.resolve(bazModule)),
      new ModuleDescriptor('biz', 'blo', () => Promise.resolve(bizModule)),
      new ModuleDescriptor('buz', 'ble', () => Promise.resolve(buzModule)),
    ];
    handler.setResultFor('getModulesOrder', Promise.resolve({
      moduleIds: ['biz', 'bar'],
    }));

    // Act.
    const moduleRegistry = new ModuleRegistry(descriptors);
    let modulesPromise = moduleRegistry.initializeModules(0);
    callbackRouterRemote.setDisabledModules(false, ['foo', 'baz', 'buz']);
    // Wait for first batch of modules with disabled modules.
    await flushTasks();
    let modules = await modulesPromise;

    // Assert.
    assertEquals(2, modules.length);
    assertEquals('biz', modules[0].descriptor.id);
    assertDeepEquals(bizModule, modules[0].element);
    assertEquals('bar', modules[1].descriptor.id);
    assertDeepEquals(barModule, modules[1].element);

    // Act.
    modulesPromise = moduleRegistry.initializeModules(0);
    callbackRouterRemote.setDisabledModules(false, []);
    // Wait for second batch of modules with re-enabled modules.
    await flushTasks();
    modules = await modulesPromise;

    // Assert.
    assertEquals(5, modules.length);
    assertEquals('foo', modules[0].descriptor.id);
    assertDeepEquals(fooModule, modules[0].element);
    assertEquals('baz', modules[1].descriptor.id);
    assertDeepEquals(bazModule, modules[1].element);
    assertEquals('buz', modules[2].descriptor.id);
    assertDeepEquals(buzModule, modules[2].element);
    assertEquals('biz', modules[3].descriptor.id);
    assertDeepEquals(bizModule, modules[3].element);
    assertEquals('bar', modules[4].descriptor.id);
    assertDeepEquals(barModule, modules[4].element);
  });
});
