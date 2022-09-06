// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://webui-test/mojo_webui_test_support.js';

import {BrowserProxyImpl} from 'chrome://resources/js/metrics_reporter/browser_proxy.js';
import {MetricsReporter, MetricsReporterImpl} from 'chrome://resources/js/metrics_reporter/metrics_reporter.js';
import {PageMetricsCallbackRouter} from 'chrome://resources/js/metrics_reporter/metrics_reporter.mojom-webui.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';
import {TestBrowserProxy} from 'chrome://webui-test/test_browser_proxy.js';

suite('MetricsReporterTest', function() {
  const DELTA_TIME: bigint = 1000n;
  let now: bigint;

  let callbackRouter: PageMetricsCallbackRouter;
  const apiProxy = TestBrowserProxy.fromClass(BrowserProxyImpl);
  let metricsReporter: MetricsReporter;

  function forwardTime() {
    apiProxy.setResultFor('now', now += DELTA_TIME);
  }

  function makeMarkPromiseResult(time: bigint|null) {
    return Promise.resolve(
        {markedTime: time === null ? null : {microseconds: time}});
  }

  suiteSetup(function() {
    callbackRouter = new PageMetricsCallbackRouter();
  });

  setup(function() {
    now = 0n;
    apiProxy.reset();
    apiProxy.setResultFor('getCallbackRouter', callbackRouter);
    apiProxy.setResultFor('now', now);
    BrowserProxyImpl.setInstance(apiProxy);
    metricsReporter = new MetricsReporterImpl();
  });

  test('markAndMeasureLoally', async function() {
    metricsReporter.mark('mark');
    forwardTime();
    assertEquals(DELTA_TIME, await metricsReporter.measure('mark'));
  });

  test('measureWithEndMark', async function() {
    metricsReporter.mark('start-mark');
    forwardTime();
    metricsReporter.mark('end-mark');
    assertEquals(
        DELTA_TIME, await metricsReporter.measure('start-mark', 'end-mark'));
  });

  test('measureRemoteMark', async function() {
    apiProxy.setResultFor('getMark', makeMarkPromiseResult(now));
    forwardTime();
    assertEquals(DELTA_TIME, await metricsReporter.measure('mark'));
    await apiProxy.whenCalled('getMark') === 'mark';
    assertEquals(1, apiProxy.getCallCount('getMark'));
  });

  test('hasMarkLocal', async function() {
    metricsReporter.mark('mark');
    assertTrue(await metricsReporter.hasMark('mark'));
  });

  test('hasLocalMark', function() {
    assertFalse(metricsReporter.hasLocalMark('mark'));
    metricsReporter.mark('mark');
    assertTrue(metricsReporter.hasLocalMark('mark'));
  });

  test('hasMarkRemote', async function() {
    apiProxy.setResultFor('getMark', makeMarkPromiseResult(now));
    assertTrue(await metricsReporter.hasMark('mark'));
    await apiProxy.whenCalled('getMark') === 'mark';
    assertEquals(1, apiProxy.getCallCount('getMark'));
  });

  test('clearMark', async function() {
    metricsReporter.mark('mark');
    metricsReporter.clearMark('mark');
    await apiProxy.whenCalled('clearMark') === 'mark';
    apiProxy.setResultFor('getMark', makeMarkPromiseResult(null));
    assertFalse(await metricsReporter.hasMark('mark'));
    await apiProxy.whenCalled('getMark') === 'mark';
    assertEquals(1, apiProxy.getCallCount('clearMark'));
    assertEquals(1, apiProxy.getCallCount('getMark'));
  });
});
