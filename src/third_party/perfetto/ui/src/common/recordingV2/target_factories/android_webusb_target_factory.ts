// Copyright (C) 2022 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

import {assertExists} from '../../../base/logging';
import {
  ADB_DEVICE_FILTER,
  findInterfaceAndEndpoint,
} from '../adb_over_webusb_utils';
import {
  OnTargetChangedCallback,
  RecordingTargetV2,
  TargetFactory,
} from '../recording_interfaces_v2';
import {targetFactoryRegistry} from '../target_factory_registry';
import {AndroidWebusbTarget} from '../targets/android_webusb_target';

export const ANDROID_WEBUSB_TARGET_FACTORY = 'AndroidWebusbTargetFactory';
const SERIAL_NUMBER_ISSUE = 'an invalid serial number';
const ADB_INTERFACE_ISSUE = 'an incompatible adb interface';

interface DeviceValidity {
  isValid: boolean;
  issues: string[];
}

function createDeviceErrorMessage(device: USBDevice, issue: string): string {
  const productName = device.productName;
  return `USB device${productName ? ' ' + productName : ''} has ${issue}`;
}

export class AndroidWebusbTargetFactory implements TargetFactory {
  readonly kind = ANDROID_WEBUSB_TARGET_FACTORY;
  onDevicesChanged?: OnTargetChangedCallback;
  private recordingProblems: string[] = [];
  private targets: Map<string, AndroidWebusbTarget> =
      new Map<string, AndroidWebusbTarget>();

  constructor(private usb: USB) {
    this.init();
  }

  getName() {
    return 'Android WebUsb';
  }

  listTargets(): RecordingTargetV2[] {
    return Array.from(this.targets.values());
  }

  listRecordingProblems(): string[] {
    return this.recordingProblems;
  }

  async connectNewTarget(): Promise<RecordingTargetV2> {
    const device: USBDevice =
        await this.usb.requestDevice({filters: [ADB_DEVICE_FILTER]});
    const deviceValid = this.checkDeviceValidity(device);
    if (!deviceValid.isValid) {
      return Promise.reject(new Error(deviceValid.issues.join('\n')));
    }

    const androidTarget = new AndroidWebusbTarget(device);
    this.targets.set(assertExists(device.serialNumber), androidTarget);
    return androidTarget;
  }

  private async init() {
    for (const device of await this.usb.getDevices()) {
      if (this.checkDeviceValidity(device).isValid) {
        this.targets.set(
            assertExists(device.serialNumber), new AndroidWebusbTarget(device));
      }
    }

    this.usb.addEventListener('connect', (ev: USBConnectionEvent) => {
      if (this.checkDeviceValidity(ev.device).isValid) {
        this.targets.set(
            assertExists(ev.device.serialNumber),
            new AndroidWebusbTarget(ev.device));
        if (this.onDevicesChanged) {
          this.onDevicesChanged();
        }
      }
    });

    this.usb.addEventListener('disconnect', async (ev: USBConnectionEvent) => {
      // We don't check device validity when disconnecting because if the device
      // is invalid we would not have connected in the first place.
      const serialNumber = assertExists(ev.device.serialNumber);
      await assertExists(this.targets.get(serialNumber))
          .disconnect(`Device with serial ${serialNumber} was disconnected.`);
      this.targets.delete(serialNumber);
      if (this.onDevicesChanged) {
        this.onDevicesChanged();
      }
    });
  }

  private checkDeviceValidity(device: USBDevice): DeviceValidity {
    const deviceValidity: DeviceValidity = {isValid: true, issues: []};
    if (!device.serialNumber) {
      deviceValidity.issues.push(
          createDeviceErrorMessage(device, SERIAL_NUMBER_ISSUE));
      deviceValidity.isValid = false;
    }
    if (!findInterfaceAndEndpoint(device)) {
      deviceValidity.issues.push(
          createDeviceErrorMessage(device, ADB_INTERFACE_ISSUE));
      deviceValidity.isValid = false;
    }
    this.recordingProblems.push(...deviceValidity.issues);
    return deviceValidity;
  }
}

if (navigator.usb) {
  targetFactoryRegistry.register(new AndroidWebusbTargetFactory(navigator.usb));
}
