// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as animate from '../../animation.js';
// eslint-disable-next-line no-unused-vars
import {Camera3DeviceInfo} from '../../device/camera3_device_info.js';
// eslint-disable-next-line no-unused-vars
import {DeviceInfoUpdater} from '../../device/device_info_updater.js';
import * as dom from '../../dom.js';
import {sendBarcodeEnabledEvent} from '../../metrics.js';
import * as localStorage from '../../models/local_storage.js';
import * as nav from '../../nav.js';
import * as state from '../../state.js';
import {Facing, Mode, PerfEvent, ViewName} from '../../type.js';
import * as util from '../../util.js';

/**
 * Creates a controller for the options of Camera view.
 */
export class Options {
  /**
   * @param {!DeviceInfoUpdater} infoUpdater
   * @param {function(): !Promise} doSwitchDevice Callback to trigger device
   *     switching.
   */
  constructor(infoUpdater, doSwitchDevice) {
    /**
     * @type {!DeviceInfoUpdater}
     * @private
     * @const
     */
    this.infoUpdater_ = infoUpdater;

    /**
     * @type {function(): !Promise}
     * @private
     * @const
     */
    this.doSwitchDevice_ = doSwitchDevice;

    /**
     * @type {!HTMLInputElement}
     * @private
     * @const
     */
    this.toggleMic_ = dom.get('#toggle-mic', HTMLInputElement);

    /**
     * @type {!HTMLInputElement}
     * @private
     * @const
     */
    this.toggleMirror_ = dom.get('#toggle-mirror', HTMLInputElement);

    /**
     * @type {!HTMLInputElement}
     * @private
     * @const
     */
    this.toggleBarcode_ = dom.get('#toggle-barcode', HTMLInputElement);

    /**
     * Device id of the camera device currently used or selected.
     * @type {?string}
     * @private
     */
    this.videoDeviceId_ = null;

    /**
     * Whether list of video devices is being refreshed now.
     * @type {boolean}
     * @private
     */
    this.refreshingVideoDeviceIds_ = false;

    /**
     * Whether the current device is HALv1 and lacks facing configuration.
     * get facing information.
     * @type {?boolean}
     * private
     */
    this.isV1NoFacingConfig_ = null;

    /**
     * Mirroring set per device.
     * @type {!Object}
     * @private
     */
    this.mirroringToggles_ = {};

    /**
     * Current audio track in use.
     * @type {?MediaStreamTrack}
     * @private
     */
    this.audioTrack_ = null;

    [['#switch-device', () => this.switchDevice_()],
     ['#open-settings', () => nav.open(ViewName.SETTINGS)],
    ]
        .forEach(
            ([selector, fn]) => dom.get(selector, HTMLButtonElement)
                                    .addEventListener('click', fn));

    this.toggleMic_.addEventListener('click', () => this.updateAudioByMic_());
    this.toggleMirror_.addEventListener('click', () => this.saveMirroring_());
    this.toggleBarcode_.addEventListener('click', () => this.updateBarcode_());

    state.addObserver(Mode.PHOTO, (inPhotoMode) => {
      if (!inPhotoMode) {
        this.toggleBarcode_.checked = false;
        this.updateBarcode_();
      }
    });

    util.bindElementAriaLabelWithState({
      element: dom.get('#toggle-timer', Element),
      state: state.State.TIMER_3SEC,
      onLabel: 'toggle_timer_3s_button',
      offLabel: 'toggle_timer_10s_button',
    });

    // Restore saved mirroring states per video device.
    localStorage.get({mirroringToggles: {}})
        .then((values) => this.mirroringToggles_ = values['mirroringToggles']);
    // Remove the deprecated values.
    localStorage.remove(['effectIndex', 'toggleMulti', 'toggleMirror']);

    this.infoUpdater_.addDeviceChangeListener(async (updater) => {
      state.set(
          state.State.MULTI_CAMERA,
          (await updater.getDevicesInfo()).length >= 2);
    });
  }

  /**
   * Device id of the camera device currently used or selected.
   * @return {?string}
   */
  get currentDeviceId() {
    return this.videoDeviceId_;
  }

  /**
   * Switches to the next available camera device.
   * @private
   */
  async switchDevice_() {
    if (!state.get(state.State.STREAMING) || state.get(state.State.TAKING)) {
      return;
    }
    state.set(PerfEvent.CAMERA_SWITCHING, true);
    const devices = await this.infoUpdater_.getDevicesInfo();
    animate.play(dom.get('#switch-device', HTMLElement));
    let index =
        devices.findIndex((entry) => entry.deviceId === this.videoDeviceId_);
    if (index === -1) {
      index = 0;
    }
    if (devices.length > 0) {
      index = (index + 1) % devices.length;
      this.videoDeviceId_ = devices[index].deviceId;
    }
    const isSuccess = await this.doSwitchDevice_();
    state.set(PerfEvent.CAMERA_SWITCHING, false, {hasError: !isSuccess});
  }

  /**
   * Maps MediaTrackSettings.facingMode to CCA facing type.
   * @param {string|undefined} facing The target facingMode to map.
   * @return {!Facing} The mapped CCA facing.
   * @private
   */
  mapFacing_(facing) {
    switch (facing) {
      case undefined:
        return Facing.EXTERNAL;
      case 'user':
        return Facing.USER;
      case 'environment':
        return Facing.ENVIRONMENT;
      default:
        throw new Error('Unknown facing: ' + facing);
    }
  }

  /**
   * Updates the options' values for the current constraints and stream.
   * @param {!MediaStream} stream Current Stream in use.
   * @return {!Promise<!Facing>} Facing-mode in use.
   */
  async updateValues(stream) {
    const track = stream.getVideoTracks()[0];
    const trackSettings = track.getSettings && track.getSettings();
    const facingMode = trackSettings && trackSettings.facingMode;
    if (this.isV1NoFacingConfig_ === null) {
      // Because the facing mode of external camera will be set to undefined on
      // all devices, to distinguish HALv1 device without facing configuration,
      // assume the first opened camera is built-in camera. Device without
      // facing configuration won't set facing of built-in cameras. Also if
      // HALv1 device with facing configuration opened external camera first
      // after CCA launched the logic here may misjudge it as this category.
      this.isV1NoFacingConfig_ = facingMode === undefined;
    }
    const facing =
        this.isV1NoFacingConfig_ ? Facing.NOT_SET : this.mapFacing_(facingMode);
    this.videoDeviceId_ = trackSettings && trackSettings.deviceId || null;
    this.updateMirroring_(facing);
    this.audioTrack_ = stream.getAudioTracks()[0];
    this.updateAudioByMic_();
    return facing;
  }

  /**
   * Updates mirroring for a new stream.
   * @param {!Facing} facing Facing of the stream.
   * @private
   */
  updateMirroring_(facing) {
    // Update mirroring by detected facing-mode. Enable mirroring by default if
    // facing-mode isn't available.
    let enabled = facing !== Facing.ENVIRONMENT;

    // Override mirroring only if mirroring was toggled manually.
    if (this.videoDeviceId_ in this.mirroringToggles_) {
      enabled = this.mirroringToggles_[this.videoDeviceId_];
    }

    util.toggleChecked(this.toggleMirror_, enabled);
  }

  /**
   * Saves the toggled mirror state for the current video device.
   * @private
   */
  saveMirroring_() {
    this.mirroringToggles_[this.videoDeviceId_] = this.toggleMirror_.checked;
    localStorage.set({mirroringToggles: this.mirroringToggles_});
  }

  /**
   * Enables/disables the current audio track according to the microphone
   * option.
   * @private
   */
  updateAudioByMic_() {
    if (this.audioTrack_) {
      this.audioTrack_.enabled = this.toggleMic_.checked;
    }
  }

  /**
   * Enables/disables barcode scanning according to the barcode option.
   * @private
   */
  updateBarcode_() {
    state.set(state.State.SCAN_BARCODE, this.toggleBarcode_.checked);
    if (this.toggleBarcode_.checked) {
      sendBarcodeEnabledEvent();
    }
  }

  /**
   * Gets the video device ids sorted by preference.
   * @return {!Promise<!Array<?string>>} May contain null for user facing camera
   *     on HALv1 devices.
   */
  async videoDeviceIds() {
    /** @type {!Array<(!Camera3DeviceInfo|!MediaDeviceInfo)>} */
    let devices;
    /**
     * Object mapping from device id to facing. Set to null on HALv1 device.
     * @type {?Object<string, !Facing>}
     */
    let facings = null;

    const camera3Info = await this.infoUpdater_.getCamera3DevicesInfo();
    if (camera3Info) {
      devices = camera3Info;
      facings = {};
      for (const {deviceId, facing} of camera3Info) {
        facings[deviceId] = facing;
      }
      this.isV1NoFacingConfig_ = false;
    } else {
      devices = await this.infoUpdater_.getDevicesInfo();
    }

    const defaultFacing = util.getDefaultFacing();
    // Put the selected video device id first.
    const sorted = devices.map((device) => device.deviceId).sort((a, b) => {
      if (a === b) {
        return 0;
      }
      if (this.videoDeviceId_ ? a === this.videoDeviceId_ :
                                (facings && facings[a] === defaultFacing)) {
        return -1;
      }
      return 1;
    });
    // Prepended 'null' deviceId means the system default camera on HALv1
    // device. Add it only when the app is launched (no video-device-id set).
    if (!facings && this.videoDeviceId_ === null) {
      sorted.unshift(null);
    }
    return sorted;
  }
}
