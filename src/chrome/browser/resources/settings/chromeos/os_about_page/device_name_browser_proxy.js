// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';
import {DeviceNameState} from './device_name_util.js';
// clang-format on

/**
 * @typedef {{
 *   deviceName: string,
 *   deviceNameState: !DeviceNameState,
 * }}
 */
export let DeviceNameMetadata;

/** @interface */
export class DeviceNameBrowserProxy {
  /**
   * Notifies the system that the page is ready for the device name.
   * @return {!Promise<!DeviceNameMetadata>}
   */
  notifyReadyForDeviceName() {}
}

/**
 * @implements {DeviceNameBrowserProxy}
 */
export class DeviceNameBrowserProxyImpl {
  /** @override */
  notifyReadyForDeviceName() {
    return chrome.send('notifyReadyForDeviceName');
  }
}

// The singleton instance_ is replaced with a test version of this wrapper
// during testing.
addSingletonGetter(DeviceNameBrowserProxyImpl);
