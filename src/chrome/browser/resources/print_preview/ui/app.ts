// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import './print_preview_vars_css.js';
import '../strings.m.js';
import '../data/document_info.js';
import './sidebar.js';

import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {isMac, isWindows} from 'chrome://resources/js/cr.m.js';
import {FocusOutlineManager} from 'chrome://resources/js/cr/ui/focus_outline_manager.m.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';
import {hasKeyModifiers} from 'chrome://resources/js/util.m.js';
import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {html, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CloudPrintInterface, CloudPrintInterfaceErrorEventDetail, CloudPrintInterfaceEventType} from '../cloud_print_interface.js';
import {CloudPrintInterfaceImpl} from '../cloud_print_interface_impl.js';
import {Destination} from '../data/destination.js';
// <if expr="chromeos_ash or chromeos_lacros">
import {DestinationOrigin} from '../data/destination.js';
// </if>
import {getPrinterTypeForDestination, PrinterType} from '../data/destination_match.js';
import {DocumentSettings, PrintPreviewDocumentInfoElement} from '../data/document_info.js';
import {Margins} from '../data/margins.js';
import {MeasurementSystem} from '../data/measurement_system.js';
import {DuplexMode, PrintPreviewModelElement, whenReady} from '../data/model.js';
import {PrintableArea} from '../data/printable_area.js';
import {Size} from '../data/size.js';
import {Error, PrintPreviewStateElement, State} from '../data/state.js';
import {MetricsContext, PrintPreviewInitializationEvents} from '../metrics.js';
import {NativeInitialSettings, NativeLayer, NativeLayerImpl} from '../native_layer.js';
// <if expr="chromeos_ash or chromeos_lacros">
import {NativeLayerCros, NativeLayerCrosImpl} from '../native_layer_cros.js';

// </if>

import {DestinationState} from './destination_settings.js';
import {PreviewAreaState, PrintPreviewPreviewAreaElement} from './preview_area.js';
import {SettingsMixin} from './settings_mixin.js';
import {PrintPreviewSidebarElement} from './sidebar.js';

export interface PrintPreviewAppElement {
  $: {
    documentInfo: PrintPreviewDocumentInfoElement,
    model: PrintPreviewModelElement,
    previewArea: PrintPreviewPreviewAreaElement,
    sidebar: PrintPreviewSidebarElement,
    state: PrintPreviewStateElement,
  };
}

const PrintPreviewAppElementBase =
    WebUIListenerMixin(SettingsMixin(PolymerElement));

export class PrintPreviewAppElement extends PrintPreviewAppElementBase {
  static get is() {
    return 'print-preview-app';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      state: {
        type: Number,
        observer: 'onStateChanged_',
      },

      cloudPrintErrorMessage_: String,

      cloudPrintInterface_: Object,

      controlsManaged_: {
        type: Boolean,
        computed: 'computeControlsManaged_(destinationsManaged_, ' +
            'settingsManaged_, maxSheets_)',
      },

      destination_: Object,

      destinationsManaged_: {
        type: Boolean,
        value: false,
      },

      destinationState_: {
        type: Number,
        observer: 'onDestinationStateChange_',
      },

      documentSettings_: Object,

      error_: Number,

      margins_: Object,

      pageSize_: Object,

      previewState_: {
        type: String,
        observer: 'onPreviewStateChange_',
      },

      printableArea_: Object,

      settingsManaged_: {
        type: Boolean,
        value: false,
      },

      measurementSystem_: {
        type: Object,
        value: null,
      },

      maxSheets_: Number,
    };
  }

  state: State;
  private cloudPrintErrorMessage_: string;
  private cloudPrintInterface_: CloudPrintInterface;
  private controlsManaged_: boolean;
  private destination_: Destination;
  private destinationsManaged_: boolean;
  private destinationState_: DestinationState;
  private documentSettings_: DocumentSettings;
  private error_: Error;
  private margins_: Margins;
  private pageSize_: Size;
  private previewState_: PreviewAreaState;
  private printableArea_: PrintableArea;
  private settingsManaged_: boolean;
  private measurementSystem_: MeasurementSystem|null;
  private maxSheets_: number;

  private nativeLayer_: NativeLayer|null = null;
  // <if expr="chromeos_ash or chromeos_lacros">
  private nativeLayerCros_: NativeLayerCros|null = null;
  // </if>
  private tracker_: EventTracker = new EventTracker();
  private cancelled_: boolean = false;
  private printRequested_: boolean = false;
  private startPreviewWhenReady_: boolean = false;
  private showSystemDialogBeforePrint_: boolean = false;
  private openPdfInPreview_: boolean = false;
  private isInKioskAutoPrintMode_: boolean = false;
  private whenReady_: Promise<void>|null = null;
  private openDialogs_: CrDialogElement[] = [];

  constructor() {
    super();

    // Regular expression that captures the leading slash, the content and the
    // trailing slash in three different groups.
    const CANONICAL_PATH_REGEX = /(^\/)([\/-\w]+)(\/$)/;
    const path = location.pathname.replace(CANONICAL_PATH_REGEX, '$1$2');
    if (path !== '/') {  // There are no subpages in Print Preview.
      window.history.replaceState(undefined /* stateObject */, '', '/');
    }
  }

  ready() {
    super.ready();

    FocusOutlineManager.forDocument(document);
  }

  connectedCallback() {
    super.connectedCallback();

    document.documentElement.classList.remove('loading');
    this.nativeLayer_ = NativeLayerImpl.getInstance();
    // <if expr="chromeos_ash or chromeos_lacros">
    this.nativeLayerCros_ = NativeLayerCrosImpl.getInstance();
    // </if>
    this.addWebUIListener('cr-dialog-open', this.onCrDialogOpen_.bind(this));
    this.addWebUIListener('close', this.onCrDialogClose_.bind(this));
    this.addWebUIListener('print-failed', this.onPrintFailed_.bind(this));
    this.addWebUIListener(
        'print-preset-options', this.onPrintPresetOptions_.bind(this));
    this.tracker_.add(window, 'keydown', this.onKeyDown_.bind(this));
    this.$.previewArea.setPluginKeyEventCallback(this.onKeyDown_.bind(this));
    this.whenReady_ = whenReady();
    this.nativeLayer_.getInitialSettings().then(
        this.onInitialSettingsSet_.bind(this));
    MetricsContext.getInitialSettings().record(
        PrintPreviewInitializationEvents.FUNCTION_INITIATED);
  }

  disconnectedCallback() {
    super.disconnectedCallback();

    this.tracker_.removeAll();
    this.whenReady_ = null;
  }

  private onSidebarFocus_() {
    this.$.previewArea.hideToolbar();
  }

  /**
   * Consume escape and enter key presses and ctrl + shift + p. Delegate
   * everything else to the preview area.
   */
  private onKeyDown_(e: KeyboardEvent) {
    // Escape key closes the topmost dialog that is currently open within
    // Print Preview. If no such dialog exists, then the Print Preview dialog
    // itself is closed.
    if (e.key === 'Escape' && !hasKeyModifiers(e)) {
      // Don't close the Print Preview dialog if there is a child dialog open.
      if (this.openDialogs_.length !== 0) {
        // Manually cancel the dialog, since we call preventDefault() to prevent
        // views from closing the Print Preview dialog.
        const dialogToClose = this.openDialogs_[this.openDialogs_.length - 1];
        dialogToClose.cancel();
        e.preventDefault();
        return;
      }

      // On non-mac with toolkit-views, ESC key is handled by C++-side instead
      // of JS-side.
      if (isMac) {
        this.close_();
        e.preventDefault();
      }

      // <if expr="chromeos_ash or chromeos_lacros">
      if (this.destination_ &&
          this.destination_.origin === DestinationOrigin.CROS) {
        this.nativeLayerCros_!.recordPrinterStatusHistogram(
            this.destination_.printerStatusReason, false);
      }
      // </if>
      return;
    }

    // On Mac, Cmd+Period should close the print dialog.
    if (isMac && e.key === '.' && e.metaKey) {
      this.close_();
      e.preventDefault();
      return;
    }

    // Ctrl + Shift + p / Mac equivalent. Doesn't apply on Chrome OS.
    // On Linux/Windows, shift + p means that e.key will be 'P' with caps lock
    // off or 'p' with caps lock on.
    // On Mac, alt + p means that e.key will be unicode 03c0 (pi).
    // <if expr="not chromeos and not lacros">
    if (e.key === 'P' || e.key === 'p' || e.key === '\u03c0') {
      if ((isMac && e.metaKey && e.altKey && !e.shiftKey && !e.ctrlKey) ||
          (!isMac && e.shiftKey && e.ctrlKey && !e.altKey && !e.metaKey)) {
        // Don't use system dialog if the link isn't available.
        if (!this.$.sidebar.systemDialogLinkAvailable()) {
          return;
        }

        // Don't try to print with system dialog on Windows if the document is
        // not ready, because we send the preview document to the printer on
        // Windows.
        if (!isWindows || this.state === State.READY) {
          this.onPrintWithSystemDialog_();
        }
        e.preventDefault();
        return;
      }
    }
    // </if>

    if ((e.key === 'Enter' || e.key === 'NumpadEnter') &&
        this.state === State.READY && this.openDialogs_.length === 0) {
      const activeElementTag = (e.composedPath()[0] as HTMLElement).tagName;
      if (['CR-BUTTON', 'BUTTON', 'SELECT', 'A', 'CR-CHECKBOX'].includes(
              activeElementTag)) {
        return;
      }

      this.onPrintRequested_();
      e.preventDefault();
      return;
    }

    // Pass certain directional keyboard events to the PDF viewer.
    this.$.previewArea.handleDirectionalKeyEvent(e);
  }

  private onCrDialogOpen_(e: Event) {
    this.openDialogs_.push(e.composedPath()[0] as CrDialogElement);
  }

  private onCrDialogClose_(e: Event) {
    // Note: due to event re-firing in cr_dialog.js, this event will always
    // appear to be coming from the outermost child dialog.
    // TODO(rbpotter): Fix event re-firing so that the event comes from the
    // dialog that has been closed, and add an assertion that the removed
    // dialog matches e.composedPath()[0].
    if ((e.composedPath()[0] as HTMLElement).nodeName === 'CR-DIALOG') {
      this.openDialogs_.pop();
    }
  }

  private onInitialSettingsSet_(settings: NativeInitialSettings) {
    MetricsContext.getInitialSettings().record(
        PrintPreviewInitializationEvents.FUNCTION_SUCCESSFUL);
    if (!this.whenReady_) {
      // This element and its corresponding model were detached while waiting
      // for the callback. This can happen in tests; return early.
      return;
    }
    this.whenReady_.then(() => {
      // The cloud print interface should be initialized before initializing the
      // sidebar, so that cloud printers can be selected automatically.
      if (settings.cloudPrintURL) {
        this.initializeCloudPrint_(
            settings.cloudPrintURL, settings.isInAppKioskMode,
            settings.uiLocale);
      }
      this.$.documentInfo.init(
          settings.previewModifiable, settings.previewIsFromArc,
          settings.documentTitle, settings.documentHasSelection);
      this.$.model.setStickySettings(settings.serializedAppStateStr);
      this.$.model.setPolicySettings(settings.policies);
      this.measurementSystem_ = new MeasurementSystem(
          settings.thousandsDelimiter, settings.decimalDelimiter,
          settings.unitType);
      this.setSetting('selectionOnly', settings.shouldPrintSelectionOnly);
      this.$.sidebar.init(
          settings.isInAppKioskMode, settings.printerName,
          settings.serializedDefaultDestinationSelectionRulesStr,
          settings.pdfPrinterDisabled, settings.isDriveMounted || false);
      this.destinationsManaged_ = settings.destinationsManaged;
      this.isInKioskAutoPrintMode_ = settings.isInKioskAutoPrintMode;

      // This is only visible in the task manager.
      let title = document.head.querySelector('title');
      if (!title) {
        title = document.createElement('title');
        document.head.appendChild(title);
      }
      title.textContent = settings.documentTitle;
    });
  }

  /**
   * Called when Google Cloud Print integration is enabled.
   * @param cloudPrintUrl The URL to use for cloud print servers.
   * @param appKioskMode Whether the browser is in app kiosk mode.
   * @param uiLocale The UI locale.
   */
  private initializeCloudPrint_(
      cloudPrintUrl: string, appKioskMode: boolean, uiLocale: string) {
    assert(!this.cloudPrintInterface_);
    this.cloudPrintInterface_ = CloudPrintInterfaceImpl.getInstance();
    this.cloudPrintInterface_.configure(cloudPrintUrl, appKioskMode, uiLocale);
    this.tracker_.add(
        this.cloudPrintInterface_.getEventTarget(),
        CloudPrintInterfaceEventType.SUBMIT_DONE, this.close_.bind(this));
    this.tracker_.add(
        this.cloudPrintInterface_.getEventTarget(),
        CloudPrintInterfaceEventType.SUBMIT_FAILED,
        this.onCloudPrintError_.bind(this, appKioskMode));
  }

  /**
   * @return Whether any of the print preview settings or destinations
   *     are managed.
   */
  private computeControlsManaged_(): boolean {
    // If |this.maxSheets_| equals to 0, no sheets limit policy is present.
    return this.destinationsManaged_ || this.settingsManaged_ ||
        this.maxSheets_ > 0;
  }

  private onDestinationStateChange_() {
    switch (this.destinationState_) {
      case DestinationState.SELECTED:
      case DestinationState.SET:
        if (this.state !== State.NOT_READY &&
            this.state !== State.FATAL_ERROR) {
          this.$.state.transitTo(State.NOT_READY);
        }
        break;
      case DestinationState.UPDATED:
        if (!this.$.model.initialized()) {
          this.$.model.applyStickySettings();
        }

        this.$.model.applyDestinationSpecificPolicies();

        this.startPreviewWhenReady_ = true;
        this.$.state.transitTo(State.READY);
        break;
      case DestinationState.ERROR:
        let newState = State.ERROR;
        // <if expr="chromeos_ash or chromeos_lacros">
        if (this.error_ === Error.NO_DESTINATIONS) {
          newState = State.FATAL_ERROR;
        }
        // </if>
        this.$.state.transitTo(newState);
        break;
      default:
        break;
    }
  }

  /**
   * @param e Event containing the new sticky settings.
   */
  private onStickySettingChanged_(e: CustomEvent<string>) {
    this.nativeLayer_!.saveAppState(e.detail);
  }

  private onPreviewSettingChanged_() {
    if (this.state === State.READY) {
      this.$.previewArea.startPreview(false);
      this.startPreviewWhenReady_ = false;
    } else {
      this.startPreviewWhenReady_ = true;
    }
  }

  private onStateChanged_() {
    if (this.state === State.READY) {
      if (this.startPreviewWhenReady_) {
        this.$.previewArea.startPreview(false);
        this.startPreviewWhenReady_ = false;
      }
      if (this.isInKioskAutoPrintMode_ || this.printRequested_) {
        this.onPrintRequested_();
        // Reset in case printing fails.
        this.printRequested_ = false;
      }
    } else if (this.state === State.CLOSING) {
      this.remove();
      this.nativeLayer_!.dialogClose(this.cancelled_);
    } else if (this.state === State.HIDDEN) {
      if (this.destination_.isLocal &&
          getPrinterTypeForDestination(this.destination_) !==
              PrinterType.PDF_PRINTER) {
        // Only hide the preview for local, non PDF destinations.
        this.nativeLayer_!.hidePreview();
      }
    } else if (this.state === State.PRINTING) {
      const whenPrintDone =
          this.nativeLayer_!.print(this.$.model.createPrintTicket(
              this.destination_, this.openPdfInPreview_,
              this.showSystemDialogBeforePrint_));
      if (this.destination_.isLocal) {
        const onError = getPrinterTypeForDestination(this.destination_) ===
                PrinterType.PDF_PRINTER ?
            this.onFileSelectionCancel_.bind(this) :
            this.onPrintFailed_.bind(this);
        whenPrintDone.then(this.close_.bind(this), onError);
      } else {
        // Cloud print resolves when print data is returned to submit to cloud
        // print, or if print ticket cannot be read, no PDF data is found, or
        // PDF is oversized.
        whenPrintDone.then(
            data => this.onPrintToCloud_(data!),
            this.onPrintFailed_.bind(this));
      }
    }
  }

  private onPrintRequested_() {
    if (this.state === State.NOT_READY) {
      this.printRequested_ = true;
      return;
    }
    // <if expr="chromeos_ash or chromeos_lacros">
    if (this.destination_ &&
        this.destination_.origin === DestinationOrigin.CROS) {
      this.nativeLayerCros_!.recordPrinterStatusHistogram(
          this.destination_.printerStatusReason, true);
    }
    // </if>
    this.$.state.transitTo(
        this.$.previewArea.previewLoaded() ? State.PRINTING : State.HIDDEN);
  }

  private onCancelRequested_() {
    // <if expr="chromeos_ash or chromeos_lacros">
    if (this.destination_ &&
        this.destination_.origin === DestinationOrigin.CROS) {
      this.nativeLayerCros_!.recordPrinterStatusHistogram(
          this.destination_.printerStatusReason, false);
    }
    // </if>
    this.cancelled_ = true;
    this.$.state.transitTo(State.CLOSING);
  }

  /**
   * @param e The event containing the new validity.
   */
  private onSettingValidChanged_(e: CustomEvent<boolean>) {
    if (e.detail) {
      this.$.state.transitTo(State.READY);
    } else {
      this.error_ = Error.INVALID_TICKET;
      this.$.state.transitTo(State.ERROR);
    }
  }

  private onFileSelectionCancel_() {
    this.$.state.transitTo(State.READY);
  }

  /**
   * Called when the native layer has retrieved the data to print to Google
   * Cloud Print.
   * @param data The body to send in the HTTP request.
   */
  private onPrintToCloud_(data: string) {
    assert(
        this.cloudPrintInterface_ !== null,
        'Google Cloud Print is not enabled');
    this.cloudPrintInterface_.submit(
        this.destination_, this.$.model.createCloudJobTicket(this.destination_),
        this.documentSettings_.title, data);
  }

  // <if expr="not chromeos and not lacros">
  private onPrintWithSystemDialog_() {
    // <if expr="is_win">
    this.showSystemDialogBeforePrint_ = true;
    this.onPrintRequested_();
    // </if>
    // <if expr="not is_win">
    this.nativeLayer_!.showSystemDialog();
    this.$.state.transitTo(State.SYSTEM_DIALOG);
    // </if>
  }
  // </if>

  // <if expr="is_macosx">
  private onOpenPdfInPreview_() {
    this.openPdfInPreview_ = true;
    this.$.previewArea.setOpeningPdfInPreview();
    this.onPrintRequested_();
  }
  // </if>

  /**
   * Called when printing to a cloud, or extension printer fails.
   * @param httpError The HTTP error code, or -1 or a string describing
   *     the error, if not an HTTP error.
   */
  private onPrintFailed_(httpError: number|string) {
    console.warn('Printing failed with error code ' + httpError);
    this.error_ = Error.PRINT_FAILED;
    this.$.state.transitTo(State.FATAL_ERROR);
  }

  private onPreviewStateChange_() {
    switch (this.previewState_) {
      case PreviewAreaState.DISPLAY_PREVIEW:
      case PreviewAreaState.OPEN_IN_PREVIEW_LOADED:
        if (this.state === State.HIDDEN) {
          this.$.state.transitTo(State.PRINTING);
        }
        break;
      case PreviewAreaState.ERROR:
        if (this.state !== State.ERROR && this.state !== State.FATAL_ERROR) {
          this.$.state.transitTo(
              this.error_ === Error.INVALID_PRINTER ? State.ERROR :
                                                      State.FATAL_ERROR);
        }
        break;
      default:
        break;
    }
  }

  /**
   * Called when there was an error communicating with Google Cloud print.
   * Displays an error message in the print header.
   */
  private onCloudPrintError_(
      appKioskMode: boolean,
      event: CustomEvent<CloudPrintInterfaceErrorEventDetail>) {
    if (event.detail.status === 0 ||
        (event.detail.status === 403 && !appKioskMode)) {
      return;  // No internet connectivity or not signed in.
    }
    this.cloudPrintErrorMessage_ = event.detail.message;
    this.error_ = Error.CLOUD_PRINT_ERROR;
    this.$.state.transitTo(State.FATAL_ERROR);
    if (event.detail.status === 200) {
      console.warn(
          'Google Cloud Print Error: ' +
          `(${event.detail.errorCode}) ${event.detail.message}`);
    } else {
      console.warn(
          'Google Cloud Print Error: ' +
          `HTTP status ${event.detail.status}`);
    }
  }

  /**
   * Updates printing options according to source document presets.
   * @param disableScaling Whether the document disables scaling.
   * @param copies The default number of copies from the document.
   * @param duplex The default duplex setting from the document.
   */
  private onPrintPresetOptions_(
      disableScaling: boolean, copies: number, duplex: DuplexMode) {
    if (disableScaling) {
      this.$.documentInfo.updateIsScalingDisabled(true);
    }

    if (copies > 0 && this.getSetting('copies').available) {
      this.setSetting('copies', copies, true);
    }

    if (duplex === DuplexMode.UNKNOWN_DUPLEX_MODE) {
      return;
    }

    if (this.getSetting('duplex').available) {
      this.setSetting(
          'duplex',
          duplex === DuplexMode.LONG_EDGE || duplex === DuplexMode.SHORT_EDGE,
          true);
    }

    if (duplex !== DuplexMode.SIMPLEX &&
        this.getSetting('duplexShortEdge').available) {
      this.setSetting(
          'duplexShortEdge', duplex === DuplexMode.SHORT_EDGE, true);
    }
  }

  /**
   * @param e Contains the new preview request ID.
   */
  private onPreviewStart_(e: CustomEvent<number>) {
    this.$.documentInfo.inFlightRequestId = e.detail;
  }

  private close_() {
    this.$.state.transitTo(State.CLOSING);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'print-preview-app': PrintPreviewAppElement;
  }
}

customElements.define(PrintPreviewAppElement.is, PrintPreviewAppElement);
