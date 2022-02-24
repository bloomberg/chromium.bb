// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof} from '../assert.js';
import {Facing, Mode} from '../type.js';

import {Camera3DeviceInfo} from './camera3_device_info.js';
import {DeviceInfoUpdater} from './device_info_updater.js';
import {CaptureHandler} from './mode/index.js';

export interface ModeConstraints {
  exact?: Mode;
  default?: Mode;
}

export type CameraViewUI = CaptureHandler;

export class CameraInfo {
  readonly devicesInfo: Array<MediaDeviceInfo>;
  readonly camera3DevicesInfo: Array<Camera3DeviceInfo>|null;

  private readonly idToDeviceInfo: Map<string, MediaDeviceInfo>;
  private readonly idToCamera3DeviceInfo: Map<string, Camera3DeviceInfo>|null;

  constructor(updater: DeviceInfoUpdater) {
    this.devicesInfo = updater.getDevicesInfo();
    this.camera3DevicesInfo = updater.getCamera3DevicesInfo();
    this.idToDeviceInfo = new Map(this.devicesInfo.map((d) => [d.deviceId, d]));
    this.idToCamera3DeviceInfo = this.camera3DevicesInfo &&
        new Map(this.camera3DevicesInfo.map((d) => [d.deviceId, d]));
  }

  getDeviceInfo(deviceId: string): MediaDeviceInfo {
    const info = this.idToDeviceInfo.get(deviceId);
    assert(info !== undefined);
    return info;
  }

  getCamera3DeviceInfo(deviceId: string): Camera3DeviceInfo|null {
    if (this.idToCamera3DeviceInfo === null) {
      return null;
    }
    const info = this.idToCamera3DeviceInfo.get(deviceId);
    return assertInstanceof(info, Camera3DeviceInfo);
  }
}

/**
 * The configuration of currently opened camera or the configuration which
 * camera will be opened with.
 */
export interface CameraConfig {
  /**
   * May be null for device using legacy linux VCD.
   */
  deviceId: string|null;

  /**
   * May be Facing.NOT_SET for device using legacy linux VCD.
   */
  facing: Facing;
  mode: Mode;
}

export interface CameraUI {
  onUpdateCapability?(cameraInfo: CameraInfo): void;
  onTryingNewConfig?(config: CameraConfig): void;
  onUpdateConfig?(config: CameraConfig): void|Promise<void>;
  onCameraUnavailable?(): void;
  onCameraAvailble?(): void;
}
