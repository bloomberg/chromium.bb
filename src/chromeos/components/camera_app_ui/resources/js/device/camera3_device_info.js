// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DeviceOperator} from '../mojo/device_operator.js';
import {
  Facing,        // eslint-disable-line no-unused-vars
  FpsRangeList,  // eslint-disable-line no-unused-vars
  MaxFpsInfo,    // eslint-disable-line no-unused-vars
  Resolution,
  ResolutionList,  // eslint-disable-line no-unused-vars
  VideoConfig,     // eslint-disable-line no-unused-vars
} from '../type.js';

/**
 * Video device information queried from HALv3 mojo private API.
 */
export class Camera3DeviceInfo {
  /**
   * @param {!MediaDeviceInfo} deviceInfo Information of the video device.
   * @param {!Facing} facing Camera facing of the video device.
   * @param {!ResolutionList} photoResols Supported available photo resolutions
   *     of the video device.
   * @param {!Array<!VideoConfig>} videoResolFpses Supported available video
   *     resolutions and maximal capture fps of the video device.
   * @param {!FpsRangeList} fpsRanges Supported fps ranges of the video device.
   * @param {boolean} supportPTZ Is supported PTZ controls.
   * @param {boolean} supportDocumentScan Is supported document scanning.
   */
  constructor(
      deviceInfo, facing, photoResols, videoResolFpses, fpsRanges, supportPTZ,
      supportDocumentScan) {
    /**
     * @const {string}
     * @public
     */
    this.deviceId = deviceInfo.deviceId;

    /**
     * @const {!Facing}
     * @public
     */
    this.facing = facing;

    /**
     * @const {!ResolutionList}
     * @public
     */
    this.photoResols = photoResols;

    /**
     * @const {!ResolutionList}
     * @public
     */
    this.videoResols = [];

    /**
     * @const {!MaxFpsInfo}
     * @public
     */
    this.videoMaxFps = {};

    /**
     * @const {!FpsRangeList}
     * @public
     */
    this.fpsRanges = fpsRanges;

    /**
     * @const {boolean}
     * @public
     */
    this.supportPTZ = supportPTZ;

    /**
     * @const {boolean}
     * @public
     */
    this.supportDocumentScan = supportDocumentScan;

    videoResolFpses.filter(({maxFps}) => maxFps >= 24)
        .forEach(({width, height, maxFps}) => {
          const r = new Resolution(width, height);
          this.videoResols.push(r);
          this.videoMaxFps[r] = maxFps;
        });
  }

  /**
   * Creates a Camera3DeviceInfo by given device info and the mojo device
   *     operator.
   * @param {!MediaDeviceInfo} deviceInfo
   * @param {function(!VideoConfig): boolean} videoConfigFilter Filters the
   *     available video capability exposed by device.
   * @return {!Promise<!Camera3DeviceInfo>}
   * @throws {!Error} Thrown when the device operation is not supported.
   */
  static async create(deviceInfo, videoConfigFilter) {
    const deviceId = deviceInfo.deviceId;

    const deviceOperator = await DeviceOperator.getInstance();
    if (!deviceOperator) {
      throw new Error('Device operation is not supported');
    }
    const facing = await deviceOperator.getCameraFacing(deviceId);
    const supportPTZ =
        (await deviceOperator.getPanDefault(deviceId)) !== undefined ||
        (await deviceOperator.getTiltDefault(deviceId)) !== undefined ||
        (await deviceOperator.getZoomDefault(deviceId)) !== undefined;
    const photoResolution = await deviceOperator.getPhotoResolutions(deviceId);
    const videoConfigs = await deviceOperator.getVideoConfigs(deviceId);
    const filteredVideoConfigs = videoConfigs.filter(videoConfigFilter);
    const supportedFpsRanges =
        await deviceOperator.getSupportedFpsRanges(deviceId);
    const supportDocumentScan = false;

    return new Camera3DeviceInfo(
        deviceInfo, facing, photoResolution, filteredVideoConfigs,
        supportedFpsRanges, supportPTZ, supportDocumentScan);
  }
}
