// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const {assert} = chai;

import type * as SDKModule from '../../../../front_end/sdk/sdk.js';
import * as Resources from '../../../../front_end/resources/resources.js';

import Helper = Resources.ServiceWorkerUpdateCycleHelper.ServiceWorkerUpdateCycleHelper;

describe('ServiceWorkerUpdateCycleHelper', () => {
  let versionId = 0;
  let SDK: typeof SDKModule;
  before(async () => {
    SDK = await import('../../../../front_end/sdk/sdk.js');
  });

  it('calculates update cycle ranges', () => {
    const payload:
        Protocol.ServiceWorker.ServiceWorkerRegistration = {registrationId: '', scopeURL: '', isDeleted: false};
    const registration: SDKModule.ServiceWorkerManager.ServiceWorkerRegistration =
        new SDK.ServiceWorkerManager.ServiceWorkerRegistration(payload);

    let ranges = Helper.calculateServiceWorkerUpdateRanges(registration);
    assert.strictEqual(ranges.length, 0, 'A nascent registration has no ranges to display.');

    versionId++;
    let versionPayload: Protocol.ServiceWorker.ServiceWorkerVersion = {
      registrationId: '',
      versionId: versionId.toString(),
      scriptURL: '',
      status: Protocol.ServiceWorker.ServiceWorkerVersionStatus.New,
      runningStatus: Protocol.ServiceWorker.ServiceWorkerVersionRunningStatus.Starting,
    };
    registration._updateVersion(versionPayload);
    ranges = Helper.calculateServiceWorkerUpdateRanges(registration);
    assert.strictEqual(ranges.length, 0, 'A new registration has no ranges to display.');

    versionId++;
    versionPayload = {
      registrationId: '',
      versionId: versionId.toString(),
      scriptURL: '',
      status: Protocol.ServiceWorker.ServiceWorkerVersionStatus.Installing,
      runningStatus: Protocol.ServiceWorker.ServiceWorkerVersionRunningStatus.Running,
    };
    registration._updateVersion(versionPayload);
    ranges = Helper.calculateServiceWorkerUpdateRanges(registration);
    assert.strictEqual(ranges.length, 1, 'An installing registration has a range to display.');

    versionId++;
    versionPayload = {
      registrationId: '',
      versionId: versionId.toString(),
      scriptURL: '',
      status: Protocol.ServiceWorker.ServiceWorkerVersionStatus.Installing,
      runningStatus: Protocol.ServiceWorker.ServiceWorkerVersionRunningStatus.Running,
    };
    registration._updateVersion(versionPayload);
    ranges = Helper.calculateServiceWorkerUpdateRanges(registration);
    assert.strictEqual(
        ranges.length, 1, 'An installing registration (reported multiple times) has a range to display.');

    versionId++;
    versionPayload = {
      registrationId: '',
      versionId: versionId.toString(),
      scriptURL: '',
      status: Protocol.ServiceWorker.ServiceWorkerVersionStatus.Installed,
      runningStatus: Protocol.ServiceWorker.ServiceWorkerVersionRunningStatus.Running,
    };
    registration._updateVersion(versionPayload);
    ranges = Helper.calculateServiceWorkerUpdateRanges(registration);
    assert.strictEqual(ranges.length, 1, 'An installed registration has a range to display. ');

    versionId++;
    versionPayload = {
      registrationId: '',
      versionId: versionId.toString(),
      scriptURL: '',
      status: Protocol.ServiceWorker.ServiceWorkerVersionStatus.Activating,
      runningStatus: Protocol.ServiceWorker.ServiceWorkerVersionRunningStatus.Running,
    };
    registration._updateVersion(versionPayload);
    ranges = Helper.calculateServiceWorkerUpdateRanges(registration);
    assert.strictEqual(ranges.length, 3, 'An activating registration has ranges to display.');

    versionId++;
    versionPayload = {
      registrationId: '',
      versionId: versionId.toString(),
      scriptURL: '',
      status: Protocol.ServiceWorker.ServiceWorkerVersionStatus.Activating,
      runningStatus: Protocol.ServiceWorker.ServiceWorkerVersionRunningStatus.Running,
    };
    registration._updateVersion(versionPayload);
    ranges = Helper.calculateServiceWorkerUpdateRanges(registration);
    assert.strictEqual(ranges.length, 3, 'An activating registration has ranges to display.');

    versionId++;
    versionPayload = {
      registrationId: '',
      versionId: versionId.toString(),
      scriptURL: '',
      status: Protocol.ServiceWorker.ServiceWorkerVersionStatus.Activated,
      runningStatus: Protocol.ServiceWorker.ServiceWorkerVersionRunningStatus.Running,
    };
    registration._updateVersion(versionPayload);
    ranges = Helper.calculateServiceWorkerUpdateRanges(registration);
    assert.strictEqual(ranges.length, 3, 'An activated registration has ranges to display.');

    versionId++;
    versionPayload = {
      registrationId: '',
      versionId: versionId.toString(),
      scriptURL: '',
      status: Protocol.ServiceWorker.ServiceWorkerVersionStatus.Redundant,
      runningStatus: Protocol.ServiceWorker.ServiceWorkerVersionRunningStatus.Stopped,
    };
    registration._updateVersion(versionPayload);
    ranges = Helper.calculateServiceWorkerUpdateRanges(registration);
    assert.strictEqual(ranges.length, 3, 'A redundent registration has ranges to display.');
  });
});
