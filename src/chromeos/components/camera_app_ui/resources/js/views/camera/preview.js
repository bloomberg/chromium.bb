// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as barcodeChip from '../../barcode_chip.js';
import {assert, assertInstanceof} from '../../chrome_util.js';
import * as dom from '../../dom.js';
import {FaceOverlay} from '../../face.js';
import {BarcodeScanner} from '../../models/barcode.js';
import {DeviceOperator, parseMetadata} from '../../mojo/device_operator.js';
import * as nav from '../../nav.js';
import * as state from '../../state.js';
import {Mode} from '../../type.js';
import * as util from '../../util.js';
import {windowController} from '../../window_controller.js';

/**
 * Creates a controller for the video preview of Camera view.
 */
export class Preview {
  /**
   * @param {function(): !Promise} onNewStreamNeeded Callback to request new
   *     stream.
   */
  constructor(onNewStreamNeeded) {
    /**
     * @type {function(): !Promise}
     * @private
     */
    this.onNewStreamNeeded_ = onNewStreamNeeded;

    /**
     * Video element to capture the stream.
     * @type {!HTMLVideoElement}
     * @private
     */
    this.video_ = dom.get('#preview-video', HTMLVideoElement);

    /**
     * The observer id for preview metadata.
     * @type {?number}
     * @private
     */
    this.metadataObserverId_ = null;

    /**
     * The face overlay for showing faces over preview.
     * @type {?FaceOverlay}
     * @private
     */
    this.faceOverlay_ = null;

    /**
     * Current active stream.
     * @type {?MediaStream}
     * @private
     */
    this.stream_ = null;

    /**
     * Watchdog for stream-end.
     * @type {?number}
     * @private
     */
    this.watchdog_ = null;

    /**
     * Promise for the current applying focus.
     * @type {?Promise}
     * @private
     */
    this.focus_ = null;

    /**
     * @type {?BarcodeScanner}
     * @private
     */
    this.scanner_ = null;

    window.addEventListener('resize', () => this.onWindowStatusChanged_());

    windowController.addListener(() => this.onWindowStatusChanged_());

    [state.State.EXPERT, state.State.SHOW_METADATA].forEach((s) => {
      state.addObserver(s, this.updateShowMetadata_.bind(this));
    });
    [state.State.EXPERT, state.State.SCAN_BARCODE].forEach((s) => {
      state.addObserver(s, this.updateScanBarcode_.bind(this));
    });
  }

  /**
   * Current active stream.
   * @return {?MediaStream}
   */
  get stream() {
    return this.stream_;
  }

  /**
   * @return {!HTMLVideoElement}
   */
  get video() {
    return this.video_;
  }

  /**
   * Whether the opened camera supports PTZ controls.
   * @return {boolean}
   */
  isSupportPTZ() {
    const {pan, tilt, zoom} = this.stream.getVideoTracks()[0].getCapabilities();
    return pan !== undefined || tilt !== undefined || zoom !== undefined;
  }

  /**
   * @override
   */
  toString() {
    const {videoWidth, videoHeight} = this.video_;
    return videoHeight ? `${videoWidth} x ${videoHeight}` : '';
  }

  /**
   * Sets video element's source.
   * @param {!MediaStream} stream Stream to be the source.
   * @return {!Promise} Promise for the operation.
   */
  async setSource_(stream) {
    const tpl = util.instantiateTemplate('#preview-video-template');
    const video = dom.getFrom(tpl, 'video', HTMLVideoElement);
    await new Promise((resolve) => {
      const handler = () => {
        video.removeEventListener('canplay', handler);
        resolve();
      };
      video.addEventListener('canplay', handler);
      video.srcObject = stream;
    });
    await video.play();
    this.video_.parentElement.replaceChild(tpl, this.video_);
    this.video_.removeAttribute('srcObject');
    this.video_.load();
    this.video_ = video;
    video.addEventListener('resize', () => this.onIntrinsicSizeChanged_());
    video.addEventListener(
        'click',
        (event) => this.onFocusClicked_(assertInstanceof(event, MouseEvent)));
    return this.onIntrinsicSizeChanged_();
  }

  /**
   * Opens preview stream.
   * @param {!MediaStreamConstraints} constraints Constraints of preview stream.
   * @return {!Promise<!MediaStream>} Promise resolved to opened preview stream.
   */
  async open(constraints) {
    this.stream_ = await navigator.mediaDevices.getUserMedia(constraints);
    try {
      await this.setSource_(this.stream_);
      // Use a watchdog since the stream.onended event is unreliable in the
      // recent version of Chrome. As of 55, the event is still broken.
      this.watchdog_ = setInterval(() => {
        // Check if video stream is ended (audio stream may still be live).
        if (this.stream_.getVideoTracks().length === 0 ||
            this.stream_.getVideoTracks()[0].readyState === 'ended') {
          clearInterval(this.watchdog_);
          this.watchdog_ = null;
          this.stream_ = null;
          this.onNewStreamNeeded_();
        }
      }, 100);
      this.scanner_ = new BarcodeScanner(this.video_, (value) => {
        barcodeChip.show(value);
      });
      this.updateScanBarcode_();
      this.updateShowMetadata_();

      const deviceOperator = await DeviceOperator.getInstance();
      if (deviceOperator !== null) {
        const deviceId =
            this.stream_.getVideoTracks()[0].getSettings().deviceId;
        const isSuccess =
            await deviceOperator.setCameraFrameRotationEnabledAtSource(
                deviceId, false);
        if (!isSuccess) {
          console.warn(
              'Cannot disable camera frame rotation. ' +
              'The camera is probably being used by another app.');
        }
      }

      const track = this.stream.getVideoTracks()[0];
      const settings = track.getSettings();
      const {pan, tilt, zoom} = track.getCapabilities();
      // PTZ function is excluded from builtin camera until we set up its AVL
      // calibration standard.
      const isBuiltinCamera = settings.facingMode !== undefined;
      state.set(
          state.State.HAS_PTZ_SUPPORT,
          !isBuiltinCamera &&
              (pan !== undefined || tilt !== undefined || zoom !== undefined));

      state.set(state.State.STREAMING, true);
    } catch (e) {
      await this.close();
      throw e;
    }
    return this.stream_;
  }

  /**
   * Closes the preview.
   * @return {!Promise}
   */
  async close() {
    if (this.watchdog_ !== null) {
      clearInterval(this.watchdog_);
      this.watchdog_ = null;
    }
    // Pause video element to avoid black frames during transition.
    this.video_.pause();
    this.disableShowMetadata_();
    if (this.stream_ !== null) {
      const track = this.stream_.getVideoTracks()[0];
      const {deviceId} = track.getSettings();
      track.stop();
      const deviceOperator = await DeviceOperator.getInstance();
      if (deviceOperator !== null) {
        deviceOperator.dropConnection(deviceId);
      }
      this.stream_ = null;
    }
    if (this.scanner_ !== null) {
      this.scanner_.stop();
      this.scanner_ = null;
    }
    state.set(state.State.STREAMING, false);
  }

  /**
   * Checks whether to scan barcode on preview or not.
   * @private
   */
  updateScanBarcode_() {
    if (this.scanner_ === null) {
      return;
    }
    if (state.get(Mode.PHOTO) && state.get(state.State.SCAN_BARCODE)) {
      this.scanner_.start();
    } else {
      this.scanner_.stop();
      barcodeChip.dismiss();
    }
  }

  /**
   * Checks preview whether to show preview metadata or not.
   * @private
   */
  updateShowMetadata_() {
    if (state.get(state.State.EXPERT) && state.get(state.State.SHOW_METADATA)) {
      this.enableShowMetadata_();
    } else {
      this.disableShowMetadata_();
    }
  }

  /**
   * Creates an image blob of the current frame.
   * @return {!Promise<!Blob>} Promise for the result.
   */
  toImage() {
    const {canvas, ctx} = util.newDrawingCanvas(
        {width: this.video_.videoWidth, height: this.video_.videoHeight});
    ctx.drawImage(this.video_, 0, 0);
    return new Promise((resolve, reject) => {
      canvas.toBlob((blob) => {
        if (blob) {
          resolve(blob);
        } else {
          reject(new Error('Photo blob error.'));
        }
      }, 'image/jpeg');
    });
  }

  /**
   * Displays preview metadata on preview screen.
   * @return {!Promise} Promise for the operation.
   * @private
   */
  async enableShowMetadata_() {
    if (!this.stream_) {
      return;
    }

    dom.getAll('.metadata.value', HTMLElement).forEach((element) => {
      element.style.display = 'none';
    });

    const displayCategory = (selector, enabled) => {
      dom.get(selector, HTMLElement).classList.toggle('mode-on', enabled);
    };

    const showValue = (selector, val) => {
      const element = dom.get(selector, HTMLElement);
      element.style.display = '';
      element.textContent = val;
    };

    /**
     * @param {!Object<string, number>} obj
     * @param {string} prefix
     * @return {!Map<number, string>}
     */
    const buildInverseMap = (obj, prefix) => {
      const map = new Map();
      for (const [key, val] of Object.entries(obj)) {
        if (!key.startsWith(prefix)) {
          continue;
        }
        if (map.has(val)) {
          console.error(`Duplicated value: ${val}`);
          continue;
        }
        map.set(val, key.slice(prefix.length));
      }
      return map;
    };

    const afStateName = buildInverseMap(
        cros.mojom.AndroidControlAfState, 'ANDROID_CONTROL_AF_STATE_');
    const aeStateName = buildInverseMap(
        cros.mojom.AndroidControlAeState, 'ANDROID_CONTROL_AE_STATE_');
    const awbStateName = buildInverseMap(
        cros.mojom.AndroidControlAwbState, 'ANDROID_CONTROL_AWB_STATE_');
    const aeAntibandingModeName = buildInverseMap(
        cros.mojom.AndroidControlAeAntibandingMode,
        'ANDROID_CONTROL_AE_ANTIBANDING_MODE_');

    const tag = cros.mojom.CameraMetadataTag;
    const metadataEntryHandlers = {
      [tag.ANDROID_LENS_FOCUS_DISTANCE]: ([value]) => {
        if (value === 0) {
          // Fixed-focus camera
          return;
        }
        const focusDistance = (100 / value).toFixed(1);
        showValue('#preview-focus-distance', `${focusDistance} cm`);
      },
      [tag.ANDROID_CONTROL_AF_STATE]: ([value]) => {
        showValue('#preview-af-state', afStateName.get(value));
      },
      [tag.ANDROID_SENSOR_SENSITIVITY]: ([value]) => {
        const sensitivity = value;
        showValue('#preview-sensitivity', `ISO ${sensitivity}`);
      },
      [tag.ANDROID_SENSOR_EXPOSURE_TIME]: ([value]) => {
        const shutterSpeed = Math.round(1e9 / value);
        showValue('#preview-exposure-time', `1/${shutterSpeed}`);
      },
      [tag.ANDROID_SENSOR_FRAME_DURATION]: ([value]) => {
        const frameFrequency = Math.round(1e9 / value);
        showValue('#preview-frame-duration', `${frameFrequency} Hz`);
      },
      [tag.ANDROID_CONTROL_AE_ANTIBANDING_MODE]: ([value]) => {
        showValue(
            '#preview-ae-antibanding-mode', aeAntibandingModeName.get(value));
      },
      [tag.ANDROID_CONTROL_AE_STATE]: ([value]) => {
        showValue('#preview-ae-state', aeStateName.get(value));
      },
      [tag.ANDROID_COLOR_CORRECTION_GAINS]: ([valueRed, , , valueBlue]) => {
        const wbGainRed = valueRed.toFixed(2);
        showValue('#preview-wb-gain-red', `${wbGainRed}x`);
        const wbGainBlue = valueBlue.toFixed(2);
        showValue('#preview-wb-gain-blue', `${wbGainBlue}x`);
      },
      [tag.ANDROID_CONTROL_AWB_STATE]: ([value]) => {
        showValue('#preview-awb-state', awbStateName.get(value));
      },
      [tag.ANDROID_CONTROL_AF_MODE]: ([value]) => {
        displayCategory(
            '#preview-af',
            value !==
                cros.mojom.AndroidControlAfMode.ANDROID_CONTROL_AF_MODE_OFF);
      },
      [tag.ANDROID_CONTROL_AE_MODE]: ([value]) => {
        displayCategory(
            '#preview-ae',
            value !==
                cros.mojom.AndroidControlAeMode.ANDROID_CONTROL_AE_MODE_OFF);
      },
      [tag.ANDROID_CONTROL_AWB_MODE]: ([value]) => {
        displayCategory(
            '#preview-awb',
            value !==
                cros.mojom.AndroidControlAwbMode.ANDROID_CONTROL_AWB_MODE_OFF);
      },
    };

    // These should be per session static information and we don't need to
    // recalculate them in every callback.
    const {videoWidth, videoHeight} = this.video_;
    const resolution = `${videoWidth}x${videoHeight}`;
    const videoTrack = this.stream_.getVideoTracks()[0];
    const deviceName = videoTrack.label;

    // Currently there is no easy way to calculate the fps of a video element.
    // Here we use the metadata events to calculate a reasonable approximation.
    const updateFps = (() => {
      const FPS_MEASURE_FRAMES = 100;
      const timestamps = [];
      return () => {
        const now = performance.now();
        timestamps.push(now);
        if (timestamps.length > FPS_MEASURE_FRAMES) {
          timestamps.shift();
        }
        if (timestamps.length === 1) {
          return null;
        }
        return (timestamps.length - 1) / (now - timestamps[0]) * 1000;
      };
    })();

    const deviceOperator = await DeviceOperator.getInstance();
    if (!deviceOperator) {
      return;
    }

    const deviceId = videoTrack.getSettings().deviceId;
    const activeArraySize = await deviceOperator.getActiveArraySize(deviceId);
    const sensorOrientation =
        await deviceOperator.getSensorOrientation(deviceId);
    this.faceOverlay_ = new FaceOverlay(activeArraySize, sensorOrientation);

    const updateFace = (mode, rects) => {
      if (mode ===
          cros.mojom.AndroidStatisticsFaceDetectMode
              .ANDROID_STATISTICS_FACE_DETECT_MODE_OFF) {
        dom.get('#preview-num-faces', HTMLDivElement).style.display = 'none';
        return;
      }
      assert(rects.length % 4 === 0);
      const numFaces = rects.length / 4;
      const label = numFaces >= 2 ? 'Faces' : 'Face';
      showValue('#preview-num-faces', `${numFaces} ${label}`);
      this.faceOverlay_.show(rects);
    };

    const callback = (metadata) => {
      showValue('#preview-resolution', resolution);
      showValue('#preview-device-name', deviceName);
      const fps = updateFps();
      if (fps !== null) {
        showValue('#preview-fps', `${fps.toFixed(0)} FPS`);
      }

      let faceMode = cros.mojom.AndroidStatisticsFaceDetectMode
                         .ANDROID_STATISTICS_FACE_DETECT_MODE_OFF;
      let faceRects = [];

      const tryParseFaceEntry = (entry) => {
        switch (entry.tag) {
          case tag.ANDROID_STATISTICS_FACE_DETECT_MODE: {
            const data = parseMetadata(entry);
            assert(data.length === 1);
            faceMode = data;
            return true;
          }
          case tag.ANDROID_STATISTICS_FACE_RECTANGLES: {
            faceRects = parseMetadata(entry);
            return true;
          }
        }
        return false;
      };

      for (const entry of metadata.entries) {
        if (entry.count === 0) {
          continue;
        }
        if (tryParseFaceEntry(entry)) {
          continue;
        }
        const handler = metadataEntryHandlers[entry.tag];
        if (handler === undefined) {
          continue;
        }
        handler(parseMetadata(entry));
      }

      // We always need to run updateFace() even if face rectangles are obsent
      // in the metadata, which may happen if there is no face detected.
      updateFace(faceMode, faceRects);
    };

    this.metadataObserverId_ = await deviceOperator.addMetadataObserver(
        deviceId, callback, cros.mojom.StreamType.PREVIEW_OUTPUT);
  }

  /**
   * Hide display preview metadata on preview screen.
   * @return {!Promise} Promise for the operation.
   * @private
   */
  async disableShowMetadata_() {
    if (!this.stream_ || this.metadataObserverId_ === null) {
      return;
    }

    const deviceOperator = await DeviceOperator.getInstance();
    if (!deviceOperator) {
      return;
    }

    const deviceId = this.stream_.getVideoTracks()[0].getSettings().deviceId;
    const isSuccess = await deviceOperator.removeMetadataObserver(
        deviceId, this.metadataObserverId_);
    if (!isSuccess) {
      console.error(`Failed to remove metadata observer with id: ${
          this.metadataObserverId_}`);
    }
    this.metadataObserverId_ = null;

    if (this.faceOverlay_ !== null) {
      this.faceOverlay_.clear();
      this.faceOverlay_ = null;
    }
  }

  /**
   * Handles the the window state or window size changed.
   * @private
   */
  onWindowStatusChanged_() {
    nav.onWindowStatusChanged();
  }

  /**
   * Handles changed intrinsic size (first loaded or orientation changes).
   * @return {!Promise}
   * @private
   */
  async onIntrinsicSizeChanged_() {
    if (this.video_.videoWidth && this.video_.videoHeight) {
      this.onWindowStatusChanged_();
    }
    this.cancelFocus_();
  }

  /**
   * Handles clicking for focus.
   * @param {!MouseEvent} event Click event.
   * @private
   */
  onFocusClicked_(event) {
    this.cancelFocus_();

    // Normalize to square space coordinates by W3C spec.
    const x = event.offsetX / this.video_.offsetWidth;
    const y = event.offsetY / this.video_.offsetHeight;
    const constraints = {advanced: [{pointsOfInterest: [{x, y}]}]};
    const track = this.stream.getVideoTracks()[0];
    const focus = (async () => {
      try {
        await track.applyConstraints(constraints);
      } catch {
        // The device might not support setting pointsOfInterest. Ignore the
        // error and return.
        return;
      }
      if (focus !== this.focus_) {
        return;  // Focus was cancelled.
      }
      const aim = dom.get('#preview-focus-aim', HTMLObjectElement);
      const clone = aim.cloneNode(true);
      clone.style.left = `${event.offsetX + this.video_.offsetLeft}px`;
      clone.style.top = `${event.offsetY + this.video_.offsetTop}px`;
      clone.hidden = false;
      aim.parentElement.replaceChild(clone, aim);
    })();
    this.focus_ = focus;
  }

  /**
   * Cancels the current applying focus.
   * @private
   */
  cancelFocus_() {
    this.focus_ = null;
    const aim = dom.get('#preview-focus-aim', HTMLObjectElement);
    aim.hidden = true;
  }
}
