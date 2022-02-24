// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {
  getDefaultWindowSize,
} from './app_window.js';
import {assert, assertInstanceof} from './assert.js';
import {CameraManager} from './device/index.js';
import * as dom from './dom.js';
import {reportError} from './error.js';
import * as focusRing from './focus_ring.js';
import {GalleryButton} from './gallerybutton.js';
import {I18nString} from './i18n_string.js';
import {Intent} from './intent.js';
import * as metrics from './metrics.js';
import * as filesystem from './models/file_system.js';
import * as loadTimeData from './models/load_time_data.js';
import * as localStorage from './models/local_storage.js';
import {ChromeHelper} from './mojo/chrome_helper.js';
import * as nav from './nav.js';
import {PerfLogger} from './perf.js';
import {preloadImagesList} from './preload_images.js';
import * as state from './state.js';
import * as toast from './toast.js';
import * as tooltip from './tooltip.js';
import {
  ErrorLevel,
  ErrorType,
  Facing,
  Mode,
  PerfEvent,
  ViewName,
} from './type.js';
import {addUnloadCallback} from './unload.js';
import * as util from './util.js';
import {checkEnumVariant} from './util.js';
import {Camera} from './views/camera.js';
import {CameraIntent} from './views/camera_intent.js';
import {Dialog} from './views/dialog.js';
import {View} from './views/view.js';
import {Warning, WarningType} from './views/warning.js';
import {WaitableEvent} from './waitable_event.js';

/**
 * The app window instance which is used for communication with Tast tests. For
 * non-test sessions, it should be null.
 */
const appWindow = window.appWindow;

/**
 * Creates the Camera App main object.
 */
export class App {
  private perfLogger: PerfLogger;
  private intent: Intent|null;
  private readonly cameraManager: CameraManager;
  private galleryButton = new GalleryButton();
  private cameraView: Camera;

  constructor({perfLogger, intent, facing, mode: defaultMode}: {
    perfLogger: PerfLogger,
    intent: Intent|null,
    facing: Facing,
    mode: Mode|null,
  }) {
    this.perfLogger = perfLogger;

    this.intent = intent;
    const shouldHandleIntentResult = this.intent?.shouldHandleResult === true;
    state.set(
        state.State.SHOULD_HANDLE_INTENT_RESULT, shouldHandleIntentResult);

    const modeConstraints = shouldHandleIntentResult ?
        {exact: defaultMode} :
        {default: defaultMode ?? Mode.PHOTO};
    this.cameraManager =
        new CameraManager(this.perfLogger, facing, modeConstraints);

    this.cameraView = (() => {
      if (shouldHandleIntentResult) {
        return new CameraIntent(
            this.intent, this.cameraManager, this.perfLogger);
      } else {
        return new Camera(
            this.galleryButton, this.cameraManager, this.perfLogger);
      }
    })();

    document.body.addEventListener(
        'keydown', (event) => this.onKeyPressed(event));

    // Disable the zoom in-out gesture which is triggered by wheel and pinch on
    // trackpad.
    document.body.addEventListener('wheel', (event) => {
      if (event.ctrlKey) {
        event.preventDefault();
      }
    }, {passive: false, capture: true});

    util.setupI18nElements(document.body);
    this.setupToggles();
    this.setupEffect();
    focusRing.initialize();

    // Set up views navigation by their DOM z-order.
    nav.setup([
      this.cameraView,
      new Warning(),
      new Dialog(ViewName.MESSAGE_DIALOG),
      new View(ViewName.SPLASH),
    ]);

    nav.open(ViewName.SPLASH);
  }

  /**
   * Sets up toggles (checkbox and radio) by data attributes.
   */
  private setupToggles() {
    dom.getAll('input', HTMLInputElement).forEach((element) => {
      element.addEventListener('keypress', (event) => {
        const e = assertInstanceof(event, KeyboardEvent);
        if (util.getShortcutIdentifier(e) === 'Enter') {
          element.click();
        }
      });

      const save = (element: HTMLInputElement) => {
        if (element.dataset['key'] !== undefined) {
          localStorage.set(element.dataset['key'], element.checked);
        }
      };
      element.addEventListener('change', (event) => {
        if (element.dataset['state'] !== undefined) {
          state.set(
              state.assertState(element.dataset['state']), element.checked);
        }
        if (event.isTrusted) {
          save(element);
          if (element.type === 'radio' && element.checked) {
            // Handle unchecked grouped sibling radios.
            const grouped =
                `input[type=radio][name=${element.name}]:not(:checked)`;
            for (const radio of dom.getAll(grouped, HTMLInputElement)) {
              radio.dispatchEvent(new Event('change'));
              save(radio);
            }
          }
        }
      });
      if (element.dataset['state'] !== undefined) {
        const s = state.assertState(element.dataset['state']);
        state.addObserver(s, (value) => {
          if (value !== element.checked) {
            util.toggleChecked(element, value);
          }
        });
        state.set(s, element.checked);
      }
      if (element.dataset['key'] !== undefined) {
        // Restore the previously saved state on startup.
        const value =
            localStorage.getBool(element.dataset['key'], element.checked);
        util.toggleChecked(element, value);
      }
    });
  }

  /**
   * Sets up visual effect for all applicable elements.
   */
  private setupEffect() {
    dom.getAll('.inkdrop', HTMLElement)
        .forEach((el) => util.setInkdropEffect(el));

    const observer = new MutationObserver((mutationList) => {
      mutationList.forEach((mutation) => {
        assert(mutation.type === 'childList');
        // Only the newly added nodes with inkdrop class are considered here. So
        // simply adding class attribute on existing element will not work.
        for (const node of mutation.addedNodes) {
          if (!(node instanceof HTMLElement)) {
            continue;
          }
          const el = assertInstanceof(node, HTMLElement);
          if (el.classList.contains('inkdrop')) {
            util.setInkdropEffect(el);
          }
        }
      });
    });
    observer.observe(document.body, {
      subtree: true,
      childList: true,
    });
  }

  /**
   * Starts the app by loading the model and opening the camera-view.
   */
  async start(launchType: metrics.LaunchType): Promise<void> {
    document.documentElement.dir = loadTimeData.getTextDirection();
    try {
      await filesystem.initialize();
      const cameraDir = filesystem.getCameraDirectory();
      assert(cameraDir !== null);

      // There are three possible cases:
      // 1. Regular instance
      //      (intent === null)
      // 2. STILL_CAPTURE_CAMERA and VIDEO_CAMERA intents
      //      (intent !== null && shouldHandleResult === false)
      // 3. Other intents
      //      (intent !== null && shouldHandleResult === true)
      // Only (1) and (2) will show gallery button on the UI.
      if (this.intent === null || !this.intent.shouldHandleResult) {
        this.galleryButton.initialize(cameraDir);
      }
    } catch (error) {
      reportError(ErrorType.FILE_SYSTEM_FAILURE, ErrorLevel.ERROR, error);
      nav.open(ViewName.WARNING, WarningType.FILESYSTEM_FAILURE);
    }

    const showWindow = (async () => {
      // For intent only requiring open camera with specific mode without
      // returning the capture result, finish it directly.
      if (this.intent !== null && !this.intent.shouldHandleResult) {
        this.intent.finish();
      }
    })();

    const cameraResourceInitialized = new WaitableEvent();
    const exploitUsage = async () => {
      if (cameraResourceInitialized.isSignaled()) {
        this.resume();
      } else {
        // CCA must get camera usage for completing its initialization when
        // first launched.
        await this.cameraManager.initialize(this.cameraView);
        await this.cameraView.initialize();
        cameraResourceInitialized.signal();
      }
    };
    const releaseUsage = async () => {
      assert(cameraResourceInitialized.isSignaled());
      await this.suspend();
    };
    await ChromeHelper.getInstance().initCameraUsageMonitor(
        exploitUsage, releaseUsage);

    const startCamera = (async () => {
      await cameraResourceInitialized.wait();
      const isSuccess = await this.cameraManager.requestResume();

      if (isSuccess) {
        const {aspectRatio} = this.cameraManager.getPreviewResolution();
        const {width, height} = getDefaultWindowSize(aspectRatio);
        window.resizeTo(width, height);
      }

      nav.close(ViewName.SPLASH);
      nav.open(ViewName.CAMERA);

      const windowCreationTime = window.windowCreationTime;
      this.perfLogger.start(
          PerfEvent.LAUNCHING_FROM_WINDOW_CREATION, windowCreationTime);
      this.perfLogger.stop(
          PerfEvent.LAUNCHING_FROM_WINDOW_CREATION, {hasError: !isSuccess});
      if (appWindow !== null) {
        appWindow.onAppLaunched();
      }
    })();

    const preloadImages = (async () => {
      const loadImage = (url: string) =>
          new Promise<void>((resolve, reject) => {
            const link = document.createElement('link');
            link.rel = 'preload';
            link.as = 'image';
            link.href = url;
            link.onload = () => resolve();
            link.onerror = () =>
                reject(new Error(`Failed to preload image ${url}`));
            document.head.appendChild(link);
          });
      const results = await Promise.allSettled(
          preloadImagesList.map((name) => loadImage(`/images/${name}`)));
      for (const result of results) {
        if (result.status === 'rejected') {
          reportError(
              ErrorType.PRELOAD_IMAGE_FAILURE, ErrorLevel.ERROR,
              assertInstanceof(result.reason, Error));
          break;
        }
      }
    })();

    metrics.sendLaunchEvent({launchType});
    await Promise.all([showWindow, startCamera, preloadImages]);
  }

  /**
   * Handles pressed keys.
   * @param event Key press event.
   */
  private onKeyPressed(event: Event) {
    tooltip.hide();  // Hide shown tooltip on any keypress.
    nav.onKeyPressed(assertInstanceof(event, KeyboardEvent));
  }

  /**
   * Suspends app and hides app window.
   */
  async suspend(): Promise<void> {
    await this.cameraManager.requestSuspend();
    nav.open(ViewName.WARNING, WarningType.CAMERA_PAUSED);
  }

  /**
   * Resumes app from suspension and shows app window.
   */
  resume(): void {
    this.cameraManager.requestResume();
    nav.close(ViewName.WARNING, WarningType.CAMERA_PAUSED);
  }

  /**
   * Begins to take photo or recording with the current options, e.g. timer.
   * @param shutterType The shutter is triggered by which shutter type.
   * @return Promise resolved when take action completes.
   *     Returns null if CCA can't start take action.
   */
  beginTake(shutterType: metrics.ShutterType): Promise<void>|null {
    return this.cameraView.beginTake(shutterType);
  }
}

/**
 * Parse search params in URL.
 */
function parseSearchParams(): {
  intent: Intent|null,
  facing: Facing,
  mode: Mode|null,
  openFrom: string|null,
  autoTake: boolean
} {
  const url = new URL(window.location.href);
  const params = url.searchParams;

  const facing =
      checkEnumVariant(Facing, params.get('facing')) ?? Facing.NOT_SET;

  const mode = checkEnumVariant(Mode, params.get('mode'));

  const intent = (() => {
    if (params.get('intentId') === null) {
      return null;
    }
    assert(mode !== null);
    return Intent.create(url, mode);
  })();

  const autoTake = params.get('autoTake') === '1';
  const openFrom = params.get('openFrom');

  return {intent, facing, mode, autoTake, openFrom};
}

/**
 * Singleton of the App object.
 */
let instance: App|null = null;

/**
 * Creates the App object and starts camera stream.
 */
(async () => {
  if (instance !== null) {
    return;
  }

  const perfLogger = new PerfLogger();

  const {intent, facing, mode, autoTake, openFrom} = parseSearchParams();

  state.set(state.State.INTENT, intent !== null);

  addUnloadCallback(() => {
    // For SWA, we don't cancel the unhandled intent here since there is no
    // guarantee that asynchronous calls in unload listener can be executed
    // properly. Therefore, we moved the logic for canceling unhandled intent to
    // Chrome (CameraAppHelper).
    if (appWindow !== null) {
      appWindow.notifyClosed();
    }
  });

  metrics.initMetrics();
  if (appWindow !== null) {
    metrics.setMetricsEnabled(false);
  }

  // Setup listener for performance events.
  perfLogger.addListener(({event, duration, perfInfo}) => {
    metrics.sendPerfEvent({event, duration, perfInfo});

    // Setup for console perf logger.
    if (state.get(state.State.PRINT_PERFORMANCE_LOGS)) {
      // eslint-disable-next-line no-console
      console.log(
          '%c%s %s ms %s', 'color: #4E4F97; font-weight: bold;',
          event.padEnd(40), duration.toFixed(0).padStart(4),
          JSON.stringify(perfInfo));
    }

    // Setup for Tast tests logger.
    if (appWindow !== null) {
      appWindow.reportPerf({event, duration, perfInfo});
    }
  });

  state.addObserver(state.State.TAKING, (val, extras) => {
    // 'taking' state indicates either taking photo or video. Skips for
    // video-taking case since we only want to collect the metrics of
    // photo-taking.
    if (state.get(Mode.VIDEO)) {
      return;
    }
    const event = PerfEvent.PHOTO_TAKING;

    if (val) {
      perfLogger.start(event);
    } else {
      perfLogger.stop(event, extras);
    }
  });

  const states = Object.values(PerfEvent);
  for (const event of states) {
    state.addObserver(event, (val, extras) => {
      if (val) {
        perfLogger.start(event);
      } else {
        perfLogger.stop(event, extras);
      }
    });
  }

  instance = new App({perfLogger, intent, facing, mode});
  await instance.start(
      openFrom === 'assistant' ? metrics.LaunchType.ASSISTANT :
                                 metrics.LaunchType.DEFAULT);

  if (autoTake) {
    const takePromise = instance.beginTake(
        openFrom === 'assistant' ? metrics.ShutterType.ASSISTANT :
                                   metrics.ShutterType.UNKNOWN);
    if (takePromise === null) {
      toast.show(
          mode === Mode.VIDEO ? I18nString.ERROR_MSG_RECORD_START_FAILED :
                                I18nString.ERROR_MSG_TAKE_PHOTO_FAILED);
    } else {
      await takePromise;
    }
  }
})();
