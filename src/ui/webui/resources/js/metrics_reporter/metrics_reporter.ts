// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert_ts.js';
import {TimeDelta} from 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import {BrowserProxy, BrowserProxyImpl} from './browser_proxy.js';

function timeFromMojo(delta: TimeDelta): bigint {
  return delta.microseconds;
}

function timeToMojo(mark: bigint): TimeDelta {
  return {microseconds: mark};
}

/*
 * MetricsReporter: A Time Measuring Utility.
 *
 * Usages:
 *   - Use getInstance() to acquire the singleton of MetricsReporter.
 *   - Use mark(markName) to mark a timestamp.
 *     Use measure(startMark[, endMark]) to measure the duration between two
 * marks. If `endMark` is not given, current time will be used.
 *   - Use umaReportTime(metricsName, duration) to record UMA histogram.
 *   - You can call these functions from the C++ side. The marks are accessible
 * from both C++ and WebUI Javascript.
 *
 * Examples:
 *   1. Pure JS.
 *     metricsReporter.mark('StartMark');
 *     // your code to be measured ...
 *     metricsReporter.umaReportTime(
 *       'Your.Histogram', await metricsReporter.measure('StartMark'));
 *
 *   2. C++ & JS.
 *     // In C++
 *     void PageAction() {
 *       metrics_reporter_.Mark("StartMark");
 *       page->PageAction();
 *     }
 *     // In JS
 *     pageAction() {
 *       // your code to be measure ...
 *       metricsReporter.umaReportTime(
 *         metricsReporter.measure('StartMark'));
 *     }
 */
export class MetricsReporter {
  private marks_: Map<string, bigint> = new Map();
  private browserProxy_: BrowserProxy = BrowserProxyImpl.getInstance();

  constructor() {
    const callbackRouter = this.browserProxy_.getCallbackRouter();
    callbackRouter.onGetMark.addListener((name: string) => {
      return this.marks_.has(name) ? timeToMojo(this.marks_.get(name)!) : null;
    });

    callbackRouter.onClearMark.addListener(
        (name: string) => this.marks_.delete(name));
  }

  static getInstance(): MetricsReporter {
    return instance || (instance = new MetricsReporter());
  }

  mark(name: string) {
    this.marks_.set(name, this.browserProxy_.now());
  }

  async measure(startMark: string, endMark?: string): Promise<bigint> {
    let endTime: bigint;
    if (endMark) {
      const entry = this.marks_.get(endMark);
      assert(entry, `Mark "${endMark}" does not exist locally.`);
      endTime = entry;
    } else {
      endTime = this.browserProxy_.now();
    }

    let startTime: bigint;
    if (this.marks_.has(startMark)) {
      startTime = this.marks_.get(startMark)!;
    } else {
      const remoteStartTime = await this.browserProxy_.getMark(startMark);
      assert(
          remoteStartTime.markedTime,
          `Mark "${startMark}" does not exist locally or remotely.`);
      startTime = timeFromMojo(remoteStartTime.markedTime);
    }

    return endTime - startTime;
  }

  async hasMark(name: string): Promise<boolean> {
    if (this.marks_.has(name)) {
      return true;
    }
    const remoteMark = await this.browserProxy_.getMark(name);
    return remoteMark !== null && remoteMark.markedTime !== null;
  }

  clearMark(name: string) {
    assert(this.marks_.delete(name));
    this.browserProxy_.clearMark(name);
  }

  umaReportTime(histogram: string, time: bigint) {
    this.browserProxy_.umaReportTime(histogram, timeToMojo(time));
  }
}

let instance: MetricsReporter|null = null;