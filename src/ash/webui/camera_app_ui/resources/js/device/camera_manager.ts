// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  assert,
  assertExists,
  assertInstanceof,
  assertString,
} from '../assert.js';
import * as error from '../error.js';
import {Point} from '../geometry.js';
import {ChromeHelper} from '../mojo/chrome_helper.js';
import {ScreenState} from '../mojo/type.js';
import * as nav from '../nav.js';
import {PerfLogger} from '../perf.js';
import * as state from '../state.js';
import {
  ErrorLevel,
  ErrorType,
  Facing,
  Mode,
  PerfEvent,
  PreviewVideo,
  Resolution,
  ViewName,
} from '../type.js';
import * as util from '../util.js';
import {WarningType} from '../views/warning.js';
import {WaitableEvent} from '../waitable_event.js';
import {windowController} from '../window_controller.js';

import {EventListener, OperationScheduler} from './camera_operation.js';
import {VideoCaptureCandidate} from './capture_candidate.js';
import {DeviceInfoUpdater} from './device_info_updater.js';
import {Preview} from './preview.js';
import {
  CameraConfig,
  CameraInfo,
  CameraUI,
  CameraViewUI,
  ModeConstraints,
} from './type.js';

class ResumeStateWatchdog {
  // This is definitely assigned in this.start() in the first statement of the
  // while loop.
  private trialDone!: WaitableEvent<boolean>;

  private succeed = false;

  constructor(private readonly doReconfigure: () => Promise<boolean>) {
    this.start();
  }

  private async start() {
    while (!this.succeed) {
      this.trialDone = new WaitableEvent<boolean>();
      await util.sleep(100);
      this.succeed = await this.doReconfigure();
      this.trialDone.signal(this.succeed);
    }
  }

  /**
   * Waits for the next unfinished reconfigure result.
   *
   * @return The reconfigure is succeed or failed.
   */
  async waitNextReconfigure(): Promise<boolean> {
    return this.trialDone.wait();
  }
}

/**
 * Manges usage of all camera operations.
 * TODO(b/209726472): Move more camera logic in camera view to here.
 */
export class CameraManager implements EventListener {
  private hasExternalScreen = false;

  private screenOffAuto = false;

  private cameraAvailable = false;

  /**
   * Whether the device is in locked state.
   */
  private locked = false;

  private suspendRequested = false;

  private readonly scheduler: OperationScheduler;

  private watchdog: ResumeStateWatchdog|null = null;

  private readonly infoUpdater = new DeviceInfoUpdater();

  private readonly cameraUIs: CameraUI[] = [];

  private readonly preview: Preview;

  constructor(
      private readonly perfLogger: PerfLogger,
      defaultFacing: Facing|null,
      modeConstraints: ModeConstraints,
  ) {
    this.preview = new Preview(async () => {
      await this.reconfigure();
    });

    this.scheduler = new OperationScheduler(
        this.infoUpdater,
        this,
        this.preview,
        defaultFacing,
        modeConstraints,
    );

    // Monitor the states to stop camera when locked/minimized.
    const idleDetector = new IdleDetector();
    idleDetector.addEventListener('change', () => {
      this.locked = idleDetector.screenState === 'locked';
      if (this.locked) {
        this.reconfigure();
      }
    });
    idleDetector.start().catch((e) => {
      error.reportError(
          ErrorType.IDLE_DETECTOR_FAILURE, ErrorLevel.ERROR,
          assertInstanceof(e, Error));
    });

    document.addEventListener('visibilitychange', () => {
      const recording = state.get(state.State.TAKING) && state.get(Mode.VIDEO);
      if (this.isTabletBackground() && !recording) {
        this.reconfigure();
      }
    });
  }

  getCameraInfo(): CameraInfo {
    return assertExists(this.scheduler.cameraInfo);
  }

  private getDeviceId(): string {
    return assertString(this.scheduler.reconfigurer.config?.deviceId);
  }

  getPreviewVideo(): PreviewVideo {
    return this.preview.getVideo();
  }

  getAudioTrack(): MediaStreamTrack {
    return this.getPreviewVideo().getStream().getAudioTracks()[0];
  }

  /**
   * USB camera vid:pid identifier of the opened stream.
   *
   * @return Identifier formatted as "vid:pid" or null for non-USB camera.
   */
  getVidPid(): string|null {
    return this.preview.getVidPid();
  }

  getPreviewResolution(): Resolution {
    const {video} = this.getPreviewVideo();
    const {videoWidth, videoHeight} = video;
    return new Resolution(videoWidth, videoHeight);
  }

  getCaptureResolution(): Resolution|null {
    assert(this.scheduler.reconfigurer.config !== null);
    return this.scheduler.reconfigurer.config.captureCandidate.resolution;
  }

  getConstFps(): number|null {
    assert(this.scheduler.reconfigurer.config !== null);
    const c = this.scheduler.reconfigurer.config.captureCandidate;
    if (!(c instanceof VideoCaptureCandidate)) {
      return null;
    }
    return c.constFps;
  }

  async getSupportedModes(deviceId: string|null): Promise<Mode[]> {
    const modes: Mode[] = [];
    for (const mode of Object.values(Mode)) {
      if (await this.scheduler.modes.isSupported(mode, deviceId)) {
        modes.push(mode);
      }
    }
    return modes;
  }

  async onUpdateConfig(config: CameraConfig): Promise<void> {
    for (const ui of this.cameraUIs) {
      await ui.onUpdateConfig?.(config);
    }
  }

  onTryingNewConfig(config: CameraConfig): void {
    for (const ui of this.cameraUIs) {
      ui.onTryingNewConfig?.(config);
    }
  }

  onUpdateCapability(cameraInfo: CameraInfo): void {
    for (const ui of this.cameraUIs) {
      ui.onUpdateCapability?.(cameraInfo);
    }
  }

  registerCameraUI(ui: CameraUI): void {
    this.cameraUIs.push(ui);
  }

  /**
   * @return Whether window is put to background in tablet mode.
   */
  private isTabletBackground(): boolean {
    return state.get(state.State.TABLET) &&
        document.visibilityState === 'hidden';
  }

  /**
   * @return If the App window is invisible to user with respect to screen off
   *     state.
   */
  private get screenOff(): boolean {
    return this.screenOffAuto && !this.hasExternalScreen;
  }

  async initialize(cameraViewUI: CameraViewUI): Promise<void> {
    const helper = ChromeHelper.getInstance();

    function setTablet(isTablet: boolean) {
      state.set(state.State.TABLET, isTablet);
    }
    const isTablet = await helper.initTabletModeMonitor(setTablet);
    setTablet(isTablet);

    const handleScreenStateChange = () => {
      if (this.screenOff) {
        this.reconfigure();
      }
    };

    const updateScreenOffAuto = (screenState: ScreenState) => {
      const isOffAuto = screenState === ScreenState.OFF_AUTO;
      if (this.screenOffAuto !== isOffAuto) {
        this.screenOffAuto = isOffAuto;
        handleScreenStateChange();
      }
    };
    const screenState =
        await helper.initScreenStateMonitor(updateScreenOffAuto);
    updateScreenOffAuto(screenState);

    const updateExternalScreen = (hasExternalScreen: boolean) => {
      if (this.hasExternalScreen !== hasExternalScreen) {
        this.hasExternalScreen = hasExternalScreen;
        handleScreenStateChange();
      }
    };
    const hasExternalScreen =
        await helper.initExternalScreenMonitor(updateExternalScreen);
    updateExternalScreen(hasExternalScreen);

    await this.scheduler.initialize(cameraViewUI);
  }

  requestSuspend(): Promise<boolean> {
    state.set(state.State.SUSPEND, true);
    this.suspendRequested = true;
    return this.reconfigure();
  }

  requestResume(): Promise<boolean> {
    state.set(state.State.SUSPEND, false);
    this.suspendRequested = false;
    if (this.watchdog !== null) {
      return this.watchdog.waitNextReconfigure();
    }
    return this.reconfigure();
  }

  /**
   * Switches to the next available camera device.
   */
  switchCamera(): Promise<void>|null {
    const promise = this.tryReconfigure(() => {
      state.set(PerfEvent.CAMERA_SWITCHING, true);
      const devices = this.infoUpdater.getDevicesInfo();
      let index =
          devices.findIndex((entry) => entry.deviceId === this.getDeviceId());
      if (index === -1) {
        index = 0;
      }
      if (devices.length > 0) {
        index = (index + 1) % devices.length;
        assert(this.scheduler.reconfigurer.config !== null);
        this.scheduler.reconfigurer.config.deviceId = devices[index].deviceId;
      }
    });
    if (promise === null) {
      return null;
    }
    return promise.then((succeed) => {
      state.set(PerfEvent.CAMERA_SWITCHING, false, {hasError: !succeed});
    });
  }

  switchMode(mode: Mode): Promise<boolean>|null {
    return this.tryReconfigure(() => {
      assert(this.scheduler.reconfigurer.config !== null);
      this.scheduler.reconfigurer.config.mode = mode;
    });
  }

  private async setCapturePref(deviceId: string, setPref: () => void):
      Promise<boolean> {
    if (!this.cameraAvailable) {
      return false;
    }
    if (deviceId !== this.getDeviceId()) {
      // Changing the configure of the camera not currently opened, thus no
      // reconfiguration are required.
      setPref();
      return true;
    }
    return this.tryReconfigure(setPref) ?? false;
  }

  getPrefPhotoResolution(deviceId: string): Resolution|null {
    return this.scheduler.reconfigurer.capturePreferrer.getPrefPhotoResolution(
        deviceId);
  }

  setPrefPhotoResolution(deviceId: string, r: Resolution):
      Promise<boolean>|null {
    return this.setCapturePref(deviceId, () => {
      this.scheduler.reconfigurer.capturePreferrer.setPrefPhotoResolution(
          deviceId, r);
    });
  }

  getPrefVideoResolution(deviceId: string): Resolution|null {
    return this.scheduler.reconfigurer.capturePreferrer.getPrefVideoResolution(
        deviceId);
  }

  setPrefVideoResolution(deviceId: string, r: Resolution):
      Promise<boolean>|null {
    return this.setCapturePref(deviceId, () => {
      this.scheduler.reconfigurer.capturePreferrer.setPrefVideoResolution(
          deviceId, r);
    });
  }

  /**
   * Sets fps of constant video recording on currently opened camera and
   * resolution.
   */
  setPrefVideoConstFps(deviceId: string, r: Resolution, fps: number):
      Promise<boolean>|null {
    return this.setCapturePref(deviceId, () => {
      this.scheduler.reconfigurer.capturePreferrer.setPrefVideoConstFps(
          deviceId, r, fps);
    });
  }

  /**
   * Apply point of interest to the stream.
   *
   * @param point The point in normalize coordidate system, which means both
   *     |x| and |y| are in range [0, 1).
   */
  setPointOfInterest(point: Point): Promise<void> {
    return this.preview.setPointOfInterest(point);
  }

  resetPTZ(): Promise<void> {
    return this.preview.resetPTZ();
  }

  /**
   * Whether app window is suspended.
   */
  private shouldSuspend(): boolean {
    return this.locked || windowController.isMinimized() ||
        this.suspendRequested || this.screenOff || this.isTabletBackground();
  }

  async startCapture(): Promise<[Promise<void>]> {
    this.setCameraAvailable(false);
    const captureDone = await this.scheduler.startCapture();
    this.setCameraAvailable(true);
    return assertExists(captureDone);
  }

  stopCapture(): void {
    this.scheduler.stopCapture();
  }

  takeVideoSnapshot(): void {
    this.scheduler.takeVideoSnapshot();
  }

  toggleVideoRecordingPause(): void {
    this.scheduler.toggleVideoRecordingPause();
  }

  private setCameraAvailable(available: boolean): void {
    if (available === this.cameraAvailable) {
      return;
    }
    this.cameraAvailable = available;
    for (const ui of this.cameraUIs) {
      if (this.cameraAvailable) {
        ui.onCameraAvailable?.();
      } else {
        ui.onCameraUnavailable?.();
      }
    }
  }

  private tryReconfigure(setNewConfig: () => void): Promise<boolean>|null {
    if (!this.cameraAvailable) {
      return null;
    }
    setNewConfig();
    return this.reconfigure();
  }

  async reconfigure(): Promise<boolean> {
    if (this.watchdog !== null) {
      if (!await this.watchdog.waitNextReconfigure()) {
        return false;
      }
      // The watchdog.waitNextReconfigure() only return the most recent
      // reconfigure result which may not reflect the setting before calling it.
      // Thus still fallthrough here to start another reconfigure.
    }

    return this.doReconfigure();
  }

  private async doReconfigure(): Promise<boolean> {
    state.set(state.State.CAMERA_CONFIGURING, true);
    this.setCameraAvailable(false);
    this.scheduler.reconfigurer.setShouldSuspend(this.shouldSuspend());
    try {
      if (!(await this.scheduler.reconfigure())) {
        throw new Error('camera suspended');
      }
    } catch (e) {
      if (this.watchdog === null) {
        if (!this.shouldSuspend()) {
          // Suspension is caused by unexpected error, show the camera failure
          // view.
          // TODO(b/209726472): Move nav out of this module.
          nav.open(ViewName.WARNING, WarningType.NO_CAMERA);
        }
        this.watchdog = new ResumeStateWatchdog(() => this.doReconfigure());
      }
      this.perfLogger.interrupt();
      return false;
    }

    // TODO(b/209726472): Move nav out of this module.
    nav.close(ViewName.WARNING);
    this.watchdog = null;
    state.set(state.State.CAMERA_CONFIGURING, false);
    this.setCameraAvailable(true);
    return true;
  }
}
