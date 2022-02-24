// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as animate from '../animation.js';
import {
  assert,
  assertInstanceof,
} from '../assert.js';
import {
  CameraConfig,
  CameraManager,
  CameraViewUI,
  getDefaultScanCorners,
  GifResult,
  PhotoResult,
  setAvc1Parameters,
  VideoResult,
} from '../device/index.js';
import * as dom from '../dom.js';
import * as error from '../error.js';
import {Flag} from '../flag.js';
import {Point} from '../geometry.js';
import {I18nString} from '../i18n_string.js';
import * as metrics from '../metrics.js';
import {Filenamer} from '../models/file_namer.js';
import * as loadTimeData from '../models/load_time_data.js';
import * as localStorage from '../models/local_storage.js';
import {ResultSaver} from '../models/result_saver.js';
import {VideoSaver} from '../models/video_saver.js';
import {ChromeHelper} from '../mojo/chrome_helper.js';
import {DeviceOperator} from '../mojo/device_operator.js';
import * as nav from '../nav.js';
import * as newFeatureToast from '../new_feature_toast.js';
import {PerfLogger} from '../perf.js';
import * as sound from '../sound.js';
import {speak} from '../spoken_msg.js';
import * as state from '../state.js';
import * as toast from '../toast.js';
import {
  CanceledError,
  ErrorLevel,
  ErrorType,
  Facing,
  ImageBlob,
  MimeType,
  Mode,
  PerfEvent,
  Resolution,
  Rotation,
  ViewName,
} from '../type.js';
import * as util from '../util.js';
import {WaitableEvent} from '../waitable_event.js';

import {Layout} from './camera/layout.js';
import {Options} from './camera/options.js';
import {ScanOptions} from './camera/scan_options.js';
import * as timertick from './camera/timertick.js';
import {VideoEncoderOptions} from './camera/video_encoder_options.js';
import {CropDocument} from './crop_document.js';
import {Dialog} from './dialog.js';
import {PTZPanel} from './ptz_panel.js';
import * as review from './review.js';
import {PrimarySettings} from './settings.js';
import {PTZPanelOptions, View} from './view.js';
import {WarningType} from './warning.js';

/**
 * Camera-view controller.
 */
export class Camera extends View implements CameraViewUI {
  private readonly cropDocument = new CropDocument();
  private readonly docModeDialogView =
      new Dialog(ViewName.DOCUMENT_MODE_DIALOG);
  private readonly subViews: View[];

  /**
   * Layout handler for the camera view.
   */
  private readonly layoutHandler = new Layout();

  private readonly scanOptions: ScanOptions;

  private readonly videoEncoderOptions =
      new VideoEncoderOptions((parameters) => setAvc1Parameters(parameters));

  /**
   * Clock-wise rotation that needs to be applied to the recorded video in
   * order for the video to be replayed in upright orientation.
   */
  private outputVideoRotation = 0;

  /**
   * Device id of video device of active preview stream. Sets to null when
   * preview become inactive.
   */
  private activeDeviceId: string|null = null;

  protected readonly review = new review.Review();
  protected facing = Facing.NOT_SET;
  protected shutterType = metrics.ShutterType.UNKNOWN;

  /**
   * Event for tracking camera availability state.
   */
  private cameraReady: WaitableEvent = new WaitableEvent();

  /**
   * Promise for the current take of photo or recording.
   */
  private take: Promise<void>|null = null;

  private readonly openPTZPanel = dom.get('#open-ptz-panel', HTMLButtonElement);

  private readonly modesGroup = dom.get('#modes-group', HTMLElement);

  constructor(
      protected readonly resultSaver: ResultSaver,
      private readonly cameraManager: CameraManager,
      readonly perfLogger: PerfLogger,
  ) {
    super(ViewName.CAMERA);

    this.subViews = [
      new PrimarySettings(this.cameraManager),
      new PTZPanel(),
      this.review,
      this.cropDocument,
      this.docModeDialogView,
      new View(ViewName.FLASH),
    ];

    this.scanOptions = new ScanOptions(this.cameraManager);

    // Options for the camera.
    // Put it here for it controls the UI visually under camera view but it
    // currently won't interact with the view. To prevent typescript checker
    // complainting about the unused reference, it's left here without any
    // reference point to it.
    new Options(this.cameraManager);

    /**
     * Gets type of ways to trigger shutter from click event.
     */
    const getShutterType = (e: MouseEvent) => {
      if (e.clientX === 0 && e.clientY === 0) {
        return metrics.ShutterType.KEYBOARD;
      }
      return e.sourceCapabilities && e.sourceCapabilities.firesTouchEvents ?
          metrics.ShutterType.TOUCH :
          metrics.ShutterType.MOUSE;
    };

    dom.get('#start-takephoto', HTMLButtonElement)
        .addEventListener('click', (e) => {
          const mouseEvent = assertInstanceof(e, MouseEvent);
          this.beginTake(getShutterType(mouseEvent));
        });

    dom.get('#stop-takephoto', HTMLButtonElement)
        .addEventListener('click', () => this.endTake());

    const videoShutter = dom.get('#recordvideo', HTMLButtonElement);
    videoShutter.addEventListener('click', (e) => {
      if (!state.get(state.State.TAKING)) {
        this.beginTake(getShutterType(assertInstanceof(e, MouseEvent)));
      } else {
        this.endTake();
      }
    });

    dom.get('#video-snapshot', HTMLButtonElement)
        .addEventListener('click', () => {
          this.cameraManager.takeVideoSnapshot();
        });

    const pauseShutter = dom.get('#pause-recordvideo', HTMLButtonElement);
    pauseShutter.addEventListener('click', () => {
      this.cameraManager.toggleVideoRecordingPause();
    });

    // TODO(shik): Tune the timing for playing video shutter button animation.
    // Currently the |TAKING| state is ended when the file is saved.
    util.bindElementAriaLabelWithState({
      element: videoShutter,
      state: state.State.TAKING,
      onLabel: I18nString.RECORD_VIDEO_STOP_BUTTON,
      offLabel: I18nString.RECORD_VIDEO_START_BUTTON,
    });
    util.bindElementAriaLabelWithState({
      element: pauseShutter,
      state: state.State.RECORDING_PAUSED,
      onLabel: I18nString.RECORD_VIDEO_RESUME_BUTTON,
      offLabel: I18nString.RECORD_VIDEO_PAUSE_BUTTON,
    });

    this.cameraManager.registerCameraUI({
      onTryingNewConfig: (config: CameraConfig) => {
        this.updateModeUI(config.mode);
      },
      onUpdateConfig: async (config: CameraConfig) => {
        nav.close(ViewName.WARNING, WarningType.NO_CAMERA);
        this.facing = config.facing;
        this.updateActiveCamera(config.deviceId);

        // Update current mode.
        const supportedModes =
            await this.cameraManager.getSupportedModes(config.deviceId);
        const items = dom.getAll('div.mode-item', HTMLDivElement);
        let first: HTMLElement|null = null;
        let last: HTMLElement|null = null;
        for (const el of items) {
          const radio = dom.getFrom(el, 'input[type=radio]', HTMLInputElement);
          const supported = supportedModes.includes(
              util.assertEnumVariant(Mode, radio.dataset['mode']));
          el.classList.toggle('hide', !supported);
          if (supported) {
            if (first === null) {
              first = el;
            }
            last = el;
          }
        }
        for (const el of items) {
          el.classList.toggle('first', el === first);
          el.classList.toggle('last', el === last);
        }
      },
      onCameraUnavailable: () => {
        this.cameraReady = new WaitableEvent();
      },
      onCameraAvailble: () => {
        this.cameraReady.signal();
      },
    });

    for (const el of dom.getAll('.mode-item>input', HTMLInputElement)) {
      el.addEventListener('click', (event) => {
        if (!this.cameraReady) {
          event.preventDefault();
        }
      });
      el.addEventListener('change', async () => {
        if (el.checked) {
          const mode = util.assertEnumVariant(Mode, el.dataset['mode']);
          this.updateModeUI(mode);
          state.set(state.State.MODE_SWITCHING, true);
          const isSuccess = await this.cameraManager.switchMode(mode);
          state.set(state.State.MODE_SWITCHING, false, {hasError: !isSuccess});
        }
      });
    }

    this.initOpenPTZPanel();
  }

  /**
   * Initializes camera view.
   */
  async initialize(): Promise<void> {
    state.addObserver(state.State.ENABLE_FULL_SIZED_VIDEO_SNAPSHOT, () => {
      this.cameraManager.reconfigure();
    });
    state.addObserver(state.State.ENABLE_MULTISTREAM_RECORDING, () => {
      this.cameraManager.reconfigure();
    });

    this.initVideoEncoderOptions();
    await this.initScanMode();
  }

  private updateModeUI(mode: Mode) {
    for (const m of Object.values(Mode)) {
      state.set(m, m === mode);
    }
    const element =
        dom.get(`.mode-item>input[data-mode=${mode}]`, HTMLInputElement);
    element.checked = true;
    const wrapper = assertInstanceof(element.parentElement, HTMLDivElement);
    const scrollLeft = wrapper.offsetLeft -
        (this.modesGroup.offsetWidth - wrapper.offsetWidth) / 2;
    this.modesGroup.scrollTo({
      left: scrollLeft,
      top: 0,
      behavior: 'smooth',
    });
  }

  private initOpenPTZPanel() {
    this.openPTZPanel.addEventListener('click', () => {
      nav.open(ViewName.PTZ_PANEL, new PTZPanelOptions({
                 stream: this.cameraManager.getPreviewVideo().getStream(),
                 vidPid: this.cameraManager.getVidPid(),
                 resetPTZ: () => this.cameraManager.resetPTZ(),
               }));
      highlight(false);
    });

    // Highlight effect for PTZ button.
    let toastShown = false;
    const highlight = (enabled: boolean) => {
      if (!enabled) {
        if (toastShown) {
          newFeatureToast.hide();
          toastShown = false;
        }
        return;
      }
      toastShown = true;
      newFeatureToast.show(this.openPTZPanel);
      newFeatureToast.focus();
    };

    this.cameraManager.registerCameraUI({
      onUpdateConfig: () => {
        const ptzToastKey = 'isPTZToastShown';
        if (!state.get(state.State.ENABLE_PTZ) ||
            state.get(state.State.IS_NEW_FEATURE_TOAST_SHOWN) ||
            localStorage.getBool(ptzToastKey)) {
          highlight(false);
          return;
        }
        localStorage.set(ptzToastKey, true);
        state.set(state.State.IS_NEW_FEATURE_TOAST_SHOWN, true);
        highlight(true);
      },
    });
  }

  private initVideoEncoderOptions() {
    const options = this.videoEncoderOptions;
    this.cameraManager.registerCameraUI({
      onUpdateConfig: () => {
        if (state.get(Mode.VIDEO)) {
          const {width, height, frameRate} =
              this.cameraManager.getPreviewVideo().getVideoSettings();
          assert(width !== undefined);
          assert(height !== undefined);
          assert(frameRate !== undefined);
          options.updateValues(new Resolution(width, height), frameRate);
        }
      },
    });
    options.initialize();
  }

  private async initScanMode() {
    const isPlatformSupport =
        await ChromeHelper.getInstance().isDocumentModeSupported();
    state.set(state.State.SHOW_SCAN_MODE, isPlatformSupport);
    if (!isPlatformSupport) {
      return;
    }

    // Check show toast.
    const docModeToastKey = 'isDocModeToastShown';
    if (!state.get(state.State.IS_NEW_FEATURE_TOAST_SHOWN) &&
        !localStorage.getBool(docModeToastKey)) {
      state.set(state.State.IS_NEW_FEATURE_TOAST_SHOWN, true);
      localStorage.set(docModeToastKey, true);
      // aria-owns don't work on HTMLInputElement, show toast on parent div
      // instead.
      const scanModeBtn = dom.get('input[data-mode="scan"]', HTMLInputElement);
      const scanModeItem =
          assertInstanceof(scanModeBtn.parentElement, HTMLDivElement);
      newFeatureToast.show(scanModeItem);
      scanModeBtn.addEventListener('click', () => {
        newFeatureToast.hide();
      });
    }

    const docModeDialogKey = 'isDocModeDialogShown';
    if (!localStorage.getBool(docModeDialogKey)) {
      this.cameraManager.registerCameraUI({
        onUpdateConfig: () => {
          if (localStorage.getBool(docModeDialogKey) || !state.get(Mode.SCAN) ||
              !this.scanOptions.isDocumentModeEanbled()) {
            return;
          }
          localStorage.set(docModeDialogKey, true);
          const message = loadTimeData.getI18nMessage(
              I18nString.DOCUMENT_MODE_DIALOG_INTRO_TITLE);
          nav.open(ViewName.DOCUMENT_MODE_DIALOG, {message});
        },
      });
    }

    // When entering document mode, refocus to shutter button for letting user
    // to take document photo with space key as shortcut. See b/196907822.
    const checkRefocus = () => {
      if (!state.get(state.State.CAMERA_CONFIGURING) && state.get(Mode.SCAN) &&
          this.scanOptions.isDocumentModeEanbled() &&
          nav.isTopMostView(this.name)) {
        dom.getAll('button.shutter', HTMLButtonElement)
            .forEach((btn) => btn.offsetParent && btn.focus());
      }
    };
    state.addObserver(state.State.CAMERA_CONFIGURING, checkRefocus);
    this.scanOptions.onChange = checkRefocus;
  }

  getSubViews(): View[] {
    return this.subViews;
  }

  focus(): void {
    (async () => {
      await this.cameraReady.wait();

      // Check the view is still on the top after await.
      if (!nav.isTopMostView(ViewName.CAMERA)) {
        return;
      }

      if (newFeatureToast.isShowing()) {
        newFeatureToast.focus();
        return;
      }

      // Avoid focusing invisible shutters.
      dom.getAll('button.shutter', HTMLButtonElement)
          .forEach((btn) => btn.offsetParent && btn.focus());
    })();
  }

  /**
   * Begins to take photo or recording with the current options, e.g. timer.
   * @param shutterType The shutter is triggered by which shutter type.
   * @return Promise resolved when take action completes. Returns null if CCA
   *     can't start take action.
   */
  beginTake(shutterType: metrics.ShutterType): Promise<void>|null {
    if (state.get(state.State.CAMERA_CONFIGURING) ||
        state.get(state.State.TAKING)) {
      return null;
    }

    state.set(state.State.TAKING, true);
    this.shutterType = shutterType;
    this.focus();  // Refocus the visible shutter button for ChromeVox.
    this.take = (async () => {
      let hasError = false;
      try {
        // Record and keep the rotation only at the instance the user starts the
        // capture. Users may change the device orientation while taking video.
        const cameraFrameRotation = await (async () => {
          const deviceOperator = await DeviceOperator.getInstance();
          if (deviceOperator === null) {
            return 0;
          }
          assert(this.activeDeviceId !== null);
          return await deviceOperator.getCameraFrameRotation(
              this.activeDeviceId);
        })();
        // Translate the camera frame rotation back to the UI rotation, which is
        // what we need to rotate the captured video with.
        this.outputVideoRotation = (360 - cameraFrameRotation) % 360;
        await timertick.start();
        const captureDone = await this.cameraManager.startCapture();
        await captureDone();
      } catch (e) {
        hasError = true;
        if (e instanceof CanceledError) {
          return;
        }
        error.reportError(
            ErrorType.START_CAPTURE_FAILURE, ErrorLevel.ERROR,
            assertInstanceof(e, Error));
      } finally {
        this.take = null;
        state.set(state.State.TAKING, false, {hasError, facing: this.facing});
        this.focus();  // Refocus the visible shutter button for ChromeVox.
      }
    })();
    return this.take;
  }

  /**
   * Ends the current take (or clears scheduled further takes if any.)
   * @return Promise for the operation.
   */
  private async endTake(): Promise<void> {
    timertick.cancel();
    this.cameraManager.stopCapture();
    await this.take;
  }

  private async checkPhotoResult<T>(pendingPhotoResult: Promise<T>):
      Promise<T> {
    try {
      return await pendingPhotoResult;
    } catch (e) {
      this.onPhotoError();
      throw e;
    }
  }

  async handleVideoSnapshot({resolution, blob, timestamp, metadata}:
                                PhotoResult): Promise<void> {
    metrics.sendCaptureEvent({
      facing: this.facing,
      resolution,
      shutterType: this.shutterType,
      isVideoSnapshot: true,
    });
    try {
      const name = (new Filenamer(timestamp)).newImageName();
      await this.resultSaver.savePhoto(blob, name, metadata);
    } catch (e) {
      toast.show(I18nString.ERROR_MSG_SAVE_FILE_FAILED);
      throw e;
    }
  }

  async onPhotoError(): Promise<void> {
    toast.show(I18nString.ERROR_MSG_TAKE_PHOTO_FAILED);
  }

  async onNoPortrait(): Promise<void> {
    toast.show(I18nString.ERROR_MSG_TAKE_PORTRAIT_BOKEH_PHOTO_FAILED);
  }

  async onPhotoCaptureDone(pendingPhotoResult: Promise<PhotoResult>):
      Promise<void> {
    state.set(PerfEvent.PHOTO_CAPTURE_POST_PROCESSING, true);
    try {
      const {resolution, blob, timestamp, metadata} =
          await this.checkPhotoResult(pendingPhotoResult);

      metrics.sendCaptureEvent({
        facing: this.facing,
        resolution,
        shutterType: this.shutterType,
        isVideoSnapshot: false,
      });

      try {
        const name = (new Filenamer(timestamp)).newImageName();
        await this.resultSaver.savePhoto(blob, name, metadata);
      } catch (e) {
        toast.show(I18nString.ERROR_MSG_SAVE_FILE_FAILED);
        throw e;
      }
      state.set(
          PerfEvent.PHOTO_CAPTURE_POST_PROCESSING, false,
          {resolution, facing: this.facing});
    } catch (e) {
      state.set(
          PerfEvent.PHOTO_CAPTURE_POST_PROCESSING, false, {hasError: true});
      throw e;
    }
  }

  async onPortraitCaptureDone(
      pendingReference: Promise<PhotoResult>,
      pendingPortrait: Promise<PhotoResult|null>): Promise<void> {
    state.set(PerfEvent.PORTRAIT_MODE_CAPTURE_POST_PROCESSING, true);
    let hasError = false;
    try {
      const {timestamp, resolution, blob, metadata} =
          await this.checkPhotoResult(pendingReference);

      metrics.sendCaptureEvent({
        facing: this.facing,
        resolution,
        shutterType: this.shutterType,
        isVideoSnapshot: false,
      });

      try {
        // Save reference.
        const filenamer = new Filenamer(timestamp);
        const name = filenamer.newBurstName(false);
        await this.resultSaver.savePhoto(blob, name, metadata);

        const portrait = await pendingPortrait;
        if (portrait === null) {
          toast.show(I18nString.ERROR_MSG_TAKE_PORTRAIT_BOKEH_PHOTO_FAILED);
        } else {
          // Save portrait.
          const {blob: portraitBlob, metadata: portraitMetadata} = portrait;
          const name = filenamer.newBurstName(true);
          await this.resultSaver.savePhoto(
              portraitBlob, name, portraitMetadata);
        }
      } catch (e) {
        toast.show(I18nString.ERROR_MSG_SAVE_FILE_FAILED);
        throw e;
      }
    } catch (e) {
      hasError = true;
      throw e;
    } finally {
      state.set(
          PerfEvent.PORTRAIT_MODE_CAPTURE_POST_PROCESSING, false,
          {hasError, facing: this.facing});
    }
  }

  async onDocumentCaptureDone(pendingPhotoResult: Promise<PhotoResult>):
      Promise<void> {
    const {blob: rawBlob, resolution, timestamp, metadata} =
        await this.checkPhotoResult(pendingPhotoResult);
    const helper = ChromeHelper.getInstance();
    const corners = await helper.scanDocumentCorners(rawBlob);
    const reviewResult =
        await this.reviewDocument({blob: rawBlob, resolution}, corners);
    if (reviewResult === null) {
      throw new CanceledError('Cancelled after review document');
    }
    const {docBlob, mimeType} = reviewResult;
    let blob = docBlob;
    if (mimeType === MimeType.PDF) {
      blob = await helper.convertToPdf(blob);
    }
    try {
      const name = (new Filenamer(timestamp)).newDocumentName(mimeType);
      await this.resultSaver.savePhoto(blob, name, metadata);
    } catch (e) {
      toast.show(I18nString.ERROR_MSG_SAVE_FILE_FAILED);
      throw e;
    }
  }

  /**
   * Opens review view to review input blob.
   */
  protected async prepareReview(doReview: () => Promise<void>): Promise<void> {
    // Because the review view will cover the whole camera view, prepare for
    // temporarily turn off camera by stopping preview.
    await this.cameraManager.requestSuspend();
    try {
      await doReview();
    } finally {
      await this.cameraManager.requestResume();
    }
  }

  /**
   * @param originImage Original photo to be cropped document from.
   * @param refCorners Initial reference document corner positions detected by
   *     scan API. Sets to null if scan API cannot find any reference corner
   *     from |rawBlob|.
   * @return Returns the processed document blob and which mime type user
   *     choose to save. Null for cancel document.
   */
  private async reviewDocument(
      originImage: ImageBlob, refCorners: Point[]|null):
      Promise<{docBlob: Blob, mimeType: MimeType}|null> {
    const needFirstRecrop = refCorners === null;
    const allowRecrop = loadTimeData.getChromeFlag(Flag.DOCUMENT_MANUAL_CROP);
    if (needFirstRecrop && !allowRecrop) {
      const message = loadTimeData.getI18nMessage(
          I18nString.DOCUMENT_MODE_DIALOG_NOT_DETECTED_TITLE);
      nav.open(ViewName.DOCUMENT_MODE_DIALOG, {message});
      throw new CanceledError(`Couldn't detect a document`);
    }

    nav.open(ViewName.FLASH);
    const helper = ChromeHelper.getInstance();
    let result = null;
    try {
      await this.prepareReview(async () => {
        const doCrop = (blob: Blob, corners: Point[], rotation: number) =>
            helper.convertToDocument(blob, corners, rotation, MimeType.JPEG);
        let corners =
            refCorners || getDefaultScanCorners(originImage.resolution);
        // This is definitely assigned in either the async doRecrop or the else
        // branch doCrop.
        let docBlob!: Blob;
        let fixType = metrics.DocFixType.NONE;
        const sendEvent = (docResult: metrics.DocResultType) => {
          metrics.sendCaptureEvent({
            facing: this.facing,
            resolution: originImage.resolution,
            shutterType: this.shutterType,
            docResult,
            docFixType: fixType,
          });
        };

        const doRecrop = async () => {
          const {corners: newCorners, rotation} =
              await this.cropDocument.reviewCropArea(corners);

          fixType = (() => {
            const isFixRotation = rotation !== Rotation.ANGLE_0;
            const isFixPosition = newCorners.some(({x, y}, idx) => {
              const {x: oldX, y: oldY} = corners[idx];
              return Math.abs(x - oldX) * originImage.resolution.width > 1 ||
                  Math.abs(y - oldY) * originImage.resolution.height > 1;
            });
            if (isFixRotation && isFixPosition) {
              return metrics.DocFixType.FIX_BOTH;
            }
            if (isFixRotation) {
              return metrics.DocFixType.FIX_ROTATION;
            }
            if (isFixPosition) {
              return metrics.DocFixType.FIX_POSITION;
            }
            return metrics.DocFixType.NO_FIX;
          })();

          corners = newCorners;
          docBlob = await (async () => {
            nav.open(ViewName.FLASH);
            try {
              return await doCrop(originImage.blob, corners, rotation);
            } finally {
              nav.close(ViewName.FLASH);
            }
          })();
          await this.review.setReviewPhoto(docBlob);
        };

        await this.cropDocument.setReviewPhoto(originImage.blob);
        if (needFirstRecrop) {
          nav.close(ViewName.FLASH);
          await doRecrop();
        } else {
          docBlob = await doCrop(originImage.blob, corners, Rotation.ANGLE_0);
          await this.review.setReviewPhoto(docBlob);
          nav.close(ViewName.FLASH);
        }

        const positive = new review.OptionGroup({
          template: review.ButtonGroupTemplate.positive,
          options: [
            new review.Option({text: I18nString.LABEL_SAVE_PDF_DOCUMENT}, {
              callback: () => {
                sendEvent(metrics.DocResultType.SAVE_AS_PDF);
              },
              exitValue: MimeType.PDF,
            }),
            new review.Option({text: I18nString.LABEL_SAVE_PHOTO_DOCUMENT}, {
              callback: () => {
                sendEvent(metrics.DocResultType.SAVE_AS_PHOTO);
              },
              exitValue: MimeType.JPEG,
            }),
            new review.Option({text: I18nString.LABEL_SHARE}, {
              callback: async () => {
                sendEvent(metrics.DocResultType.SHARE);
                const type = MimeType.JPEG;
                const name = (new Filenamer()).newDocumentName(type);
                await util.share(new File([docBlob], name, {type}));
              },
            }),
          ],
        });

        const negOptions = [
          new review.Option({text: I18nString.LABEL_RETAKE}, {
            callback: () => {
              sendEvent(metrics.DocResultType.CANCELED);
            },
            exitValue: null,
          }),
        ];
        if (allowRecrop) {
          negOptions.unshift(
              new review.Option({text: I18nString.LABEL_FIX_DOCUMENT}, {
                callback: doRecrop,
                hasPopup: true,
              }));
        }
        const negative = new review.OptionGroup({
          template: review.ButtonGroupTemplate.negative,
          options: negOptions,
        });

        const mimeType = await this.review.startReview(positive, negative);
        assert(mimeType !== undefined);
        if (mimeType !== null) {
          result = {docBlob, mimeType};
        }
      });
    } finally {
      nav.close(ViewName.FLASH);
    }
    return result;
  }

  createVideoSaver(): Promise<VideoSaver> {
    return this.resultSaver.startSaveVideo(this.outputVideoRotation);
  }

  playShutterEffect(): void {
    sound.play(dom.get('#sound-shutter', HTMLAudioElement));
    animate.play(this.cameraManager.getPreviewVideo().video);
  }

  async onGifCaptureDone({name, gifSaver, resolution, duration}: GifResult):
      Promise<void> {
    nav.open(ViewName.FLASH);

    // Measure the latency of gif encoder finishing rest of the encoding
    // works.
    state.set(PerfEvent.GIF_CAPTURE_POST_PROCESSING, true);
    const blob = await gifSaver.endWrite();
    state.set(PerfEvent.GIF_CAPTURE_POST_PROCESSING, false);

    const sendEvent = (gifResult: metrics.GifResultType) => {
      metrics.sendCaptureEvent({
        recordType: metrics.RecordType.GIF,
        facing: this.facing,
        resolution,
        duration,
        shutterType: this.shutterType,
        gifResult,
      });
    };

    let result: boolean|null = false;
    await this.prepareReview(async () => {
      await this.review.setReviewPhoto(blob);
      const positive = new review.OptionGroup({
        template: review.ButtonGroupTemplate.positive,
        options: [
          new review.Option({text: I18nString.LABEL_SAVE}, {exitValue: true}),
          new review.Option({text: I18nString.LABEL_SHARE}, {
            callback: async () => {
              sendEvent(metrics.GifResultType.SHARE);
              await util.share(new File([blob], name, {type: MimeType.GIF}));
            },
          }),
        ],
      });
      const negative = new review.OptionGroup({
        template: review.ButtonGroupTemplate.negative,
        options: [new review.Option(
            {text: I18nString.LABEL_RETAKE}, {exitValue: null})],
      });
      nav.close(ViewName.FLASH);
      result = (await this.review.startReview(positive, negative)) as boolean;
    });
    if (result) {
      sendEvent(metrics.GifResultType.SAVE);
      await this.resultSaver.saveGif(blob, name);
    } else {
      sendEvent(metrics.GifResultType.RETAKE);
    }
  }

  async onVideoCaptureDone({resolution, videoSaver, duration, everPaused}:
                               VideoResult): Promise<void> {
    state.set(PerfEvent.VIDEO_CAPTURE_POST_PROCESSING, true);
    try {
      metrics.sendCaptureEvent({
        recordType: metrics.RecordType.NORMAL_VIDEO,
        facing: this.facing,
        duration,
        resolution,
        shutterType: this.shutterType,
        everPaused,
      });
      await this.resultSaver.finishSaveVideo(videoSaver);
      state.set(
          PerfEvent.VIDEO_CAPTURE_POST_PROCESSING, false,
          {resolution, facing: this.facing});
    } catch (e) {
      state.set(
          PerfEvent.VIDEO_CAPTURE_POST_PROCESSING, false, {hasError: true});
      throw e;
    }
  }

  layout(): void {
    this.layoutHandler.update();
  }

  handlingKey(key: string): boolean {
    if (key === 'Ctrl-R') {
      toast.showDebugMessage(
          this.cameraManager.getPreviewResolution().toString());
      return true;
    }
    if ((key === 'AudioVolumeUp' || key === 'AudioVolumeDown') &&
        state.get(state.State.TABLET) && state.get(state.State.STREAMING)) {
      if (state.get(state.State.TAKING)) {
        this.endTake();
      } else {
        this.beginTake(metrics.ShutterType.VOLUME_KEY);
      }
      return true;
    }
    return false;
  }

  /**
   * Updates |this.activeDeviceId|.
   */
  private updateActiveCamera(newDeviceId: string) {
    // Make the different active camera announced by screen reader.
    if (newDeviceId === this.activeDeviceId) {
      return;
    }
    this.activeDeviceId = newDeviceId;
    const info = this.cameraManager.getCameraInfo().getDeviceInfo(newDeviceId);
    if (info !== null) {
      speak(I18nString.STATUS_MSG_CAMERA_SWITCHED, info.label);
    }
  }
}
