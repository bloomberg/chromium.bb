// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_dialog/cr_dialog.m.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';
import 'chrome://resources/cr_elements/icons.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-lite.js';
import 'chrome://resources/polymer/v3_0/iron-collapse/iron-collapse.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './file_path.mojom-lite.js';
import './color_mode_select.js';
import './file_type_select.js';
import './loading_page.js';
import './multi_page_checkbox.js';
import './multi_page_scan.js';
import './page_size_select.js';
import './resolution_select.js';
import './scan_done_section.js';
import './scan_preview.js';
import './scan_to_select.js';
import './scanner_select.js';
import './scanning_fonts_css.js';
import './scanning_shared_css.js';
import './source_select.js';

import {CrContainerShadowBehavior} from 'chrome://resources/cr_elements/cr_container_shadow_behavior.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {afterNextRender, html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getScanService} from './mojo_interface_provider.js';
import {AppState, MAX_NUM_SAVED_SCANNERS, ScannerArr, ScannerCapabilitiesResponse, ScannerInfo, ScannerSetting, ScanSettings, StartMultiPageScanResponse} from './scanning_app_types.js';
import {colorModeFromString, fileTypeFromString, getScannerDisplayName, pageSizeFromString, tokenToString} from './scanning_app_util.js';
import {ScanningBrowserProxy, ScanningBrowserProxyImpl, SelectedPath} from './scanning_browser_proxy.js';

/**
 * URL for the Scanning help page.
 * @const {string}
 */
const HELP_PAGE_LINK = 'http://support.google.com/chromebook?p=chrome_scanning';

/**
 * @fileoverview
 * 'scanning-app' is used to interact with connected scanners.
 */
Polymer({
  is: 'scanning-app',

  _template: html`{__html_template__}`,

  behaviors: [CrContainerShadowBehavior, I18nBehavior],

  /**
   * Receives scan job notifications.
   * @private {?ash.scanning.mojom.ScanJobObserverReceiver}
   */
  scanJobObserverReceiver_: null,

  /** @private {?ash.scanning.mojom.ScanServiceInterface} */
  scanService_: null,

  /** @private {?ash.scanning.mojom.MultiPageScanControllerRemote} */
  multiPageScanController_: null,

  /** @private {!Map<string, !ScannerInfo>} */
  scannerInfoMap_: new Map(),

  /** @private {?ScanningBrowserProxy}*/
  browserProxy_: null,

  properties: {
    /** @private {!ScannerArr} */
    scanners_: {
      type: Array,
      value: () => [],
    },

    /** @type {string} */
    selectedScannerId: {
      type: String,
      observer: 'onSelectedScannerIdChange_',
    },

    /** @private {?ash.scanning.mojom.ScannerCapabilities} */
    capabilities_: Object,

    /** @type {string} */
    selectedSource: String,

    /** @type {string} */
    selectedFileType: String,

    /** @type {string} */
    selectedFilePath: String,

    /** @type {string} */
    selectedColorMode: String,

    /** @type {string} */
    selectedPageSize: String,

    /** @type {string} */
    selectedResolution: String,

    /**
     * Used to indicate where scanned files are saved when a scan is complete.
     * @type {string}
     */
    selectedFolder: String,

    /**
     * Map of a ScanSource's name to its corresponding SourceType. Used for
     * fetching the SourceType setting for scan job metrics.
     * @private {!Map<string, !ash.scanning.mojom.SourceType>}
     */
    sourceTypeMap_: {
      type: Object,
      value() {
        return new Map();
      },
    },

    /**
     * Used to determine when certain parts of the app should be shown or hidden
     * and enabled or disabled.
     * @private {!AppState}
     */
    appState_: {
      type: Number,
      value: AppState.GETTING_SCANNERS,
      observer: 'onAppStateChange_',
    },

    /**
     * The object URLs of the scanned images.
     * @private {!Array<string>}
     */
    objectUrls_: {
      type: Array,
      value: () => [],
    },

    /**
     * Used to display which page is being scanned during a scan.
     * @private {number}
     */
    pageNumber_: {
      type: Number,
      value: 1,
    },

    /**
     * Used to display a page's scan progress during a scan.
     * @private {number}
     */
    progressPercent_: {
      type: Number,
      value: 0,
    },

    /** @private {!Array<ash.scanning.mojom.PageSize>} */
    selectedSourcePageSizes_: {
      type: Array,
      value: () => [],
      computed: 'computePageSizes_(selectedSource)',
    },

    /**
     * Determines whether settings should be disabled based on the current app
     * state. Settings should be disabled until after the selected scanner's
     * capabilities are fetched since the capabilities determine what options
     * are available in the settings. They should also be disabled while
     * scanning since settings cannot be changed while a scan is in progress.
     * @private {boolean}
     */
    settingsDisabled_: {
      type: Boolean,
      value: true,
    },

    /** @private {boolean} */
    scannersLoaded_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showDoneSection_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showCancelButton_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    cancelButtonDisabled_: {
      type: Boolean,
      value: false,
    },

    /**
     * The file paths of the scanned pages of a successful scan job.
     * @private {!Array<!mojoBase.mojom.FilePath>}
     */
    scannedFilePaths_: {
      type: Array,
      value: () => [],
    },

    /**
     * The key to retrieve the appropriate string to display in the toast.
     * @private {string}
     */
    toastMessageKey_: {
      type: String,
      observer: 'onToastMessageKeyChange_',
    },

    /** @private {boolean} */
    showToastInfoIcon_: {
      type: Boolean,
      value: false,
    },

    /** @private {boolean} */
    showToastHelpLink_: {
      type: Boolean,
      value: false,
    },

    /**
     * Indicates whether the More settings section is expanded.
     * @private {boolean}
     */
    opened_: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
    },

    /**
     * Determines the arrow icon direction.
     * @private {string}
     */
    arrowIconDirection_: {
      type: String,
      computed: 'computeArrowIconDirection_(opened_)',
    },

    /**
     * Used to track the number of times a user changes scan settings before
     * initiating a scan. This gets reset to 1 when the user selects a different
     * scanner (selecting a different scanner is treated as a setting change).
     * @private {number}
     */
    numScanSettingChanges_: {
      type: Number,
      value: 0,
    },

    /**
     * Indicates the result of the scan job. Set to kSuccess when the scan job
     * succeeds.
     * @private {!ash.scanning.mojom.ScanResult}
     */
    scanResult_: {
      type: Number,
      value: ash.scanning.mojom.ScanResult.kSuccess,
    },

    /**
     * The key to retrieve the appropriate string to display in an error dialog
     * when a scan job fails.
     * @private {string}
     */
    scanFailedDialogTextKey_: {
      type: String,
      computed: 'computeScanFailedDialogTextKey_(scanResult_)',
    },

    /** @private {boolean} */
    scanAppStickySettingsEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('scanAppStickySettingsEnabled');
      }
    },

    /** @private {!ScanSettings} */
    savedScanSettings_: {
      type: Object,
      value() {
        return {
          lastUsedScannerName: '',
          scanToPath: '',
          scanners: [],
        };
      },
    },

    /** @private {string} */
    lastUsedScannerId_: String,

    /**
     * Used to track the number of completed scans during a single session of
     * the Scan app being open. This value is recorded whenever the app window
     * is closed or refreshed.
     * @private {number}
     */
    numCompletedScansInSession_: {
      type: Number,
      value: 0,
    },

    /** @private {boolean} */
    scanAppMultiPageScanEnabled_: {
      type: Boolean,
      value: function() {
        return loadTimeData.getBoolean('scanAppMultiPageScanEnabled');
      }
    },

    /** {boolean} */
    multiPageScanChecked: {
      type: Boolean,
      observer: 'onMultiPageScanCheckedChange_',
    },

    /** @private {boolean} */
    showMultiPageCheckbox_: {
      type: Boolean,
      computed: 'computeShowMultiPageCheckbox_(showScanSettings_, ' +
          'selectedSource, selectedFileType, scanAppMultiPageScanEnabled_)',
      reflectToAttribute: true,
    },

    /** @private {string} */
    scanButtonText_: String,

    /** @private {boolean} */
    showScanSettings_: {
      type: Boolean,
      value: true,
    },

    /** @private {boolean} */
    showMultiPageScan_: {
      type: Boolean,
      value: false,
    },
  },

  observers:
      ['scanSettingsChange_(selectedSource, selectedFileType, ' +
       'selectedFilePath, selectedColorMode, selectedPageSize, ' +
       'selectedResolution)'],

  /** @override */
  created() {
    this.scanService_ = getScanService();
    this.browserProxy_ = ScanningBrowserProxyImpl.getInstance();
    this.browserProxy_.initialize();
    this.browserProxy_.getMyFilesPath().then(
        /* @type {string} */ (myFilesPath) => {
          this.selectedFilePath = myFilesPath;
        });
    if (this.scanAppStickySettingsEnabled_) {
      this.browserProxy_.getScanSettings().then(
          /* @type {string} */ (scanSettings) => {
            if (!scanSettings) {
              return;
            }

            this.savedScanSettings_ =
                /** @type {!ScanSettings} */ (JSON.parse(scanSettings));
          });
    }
  },

  /** @override */
  ready() {
    window.addEventListener('beforeunload', event => {
      this.browserProxy_.recordNumCompletedScans(
          this.numCompletedScansInSession_);

      // When the user tries to close the app while a scan is in progress,
      // show the 'Leave site' dialog.
      if (this.appState_ === AppState.SCANNING) {
        event.preventDefault();
        event.returnValue = '';
      }
    });

    this.scanService_.getScanners().then(
        /*@type {!{scanners: !ScannerArr}}*/ (response) => {
          this.onScannersReceived_(response);
        });
  },

  /** @override */
  detached() {
    if (this.scanJobObserverReceiver_) {
      this.scanJobObserverReceiver_.$.close();
    }
  },

  /**
   * Overrides ash.scanning.mojom.ScanJobObserverInterface.
   * @param {number} pageNumber
   * @param {number} progressPercent
   */
  onPageProgress(pageNumber, progressPercent) {
    assert(
        this.appState_ === AppState.SCANNING ||
        this.appState_ === AppState.MULTI_PAGE_SCANNING ||
        this.appState_ === AppState.CANCELING);

    // The Scan app increments |this.pageNumber_| itself during a multi-page
    // scan.
    if (!this.multiPageScanChecked) {
      this.pageNumber_ = pageNumber;
    }
    this.progressPercent_ = progressPercent;
  },

  /**
   * Overrides ash.scanning.mojom.ScanJobObserverInterface.
   * @param {!Array<number>} pageData
   */
  onPageComplete(pageData) {
    assert(
        this.appState_ === AppState.SCANNING ||
        this.appState_ === AppState.MULTI_PAGE_SCANNING ||
        this.appState_ === AppState.CANCELING);
    const blob = new Blob([Uint8Array.from(pageData)], {'type': 'image/png'});
    this.push('objectUrls_', URL.createObjectURL(blob));
    if (this.multiPageScanChecked) {
      this.setAppState_(AppState.MULTI_PAGE_NEXT_ACTION);
    }
  },

  /**
   * Overrides ash.scanning.mojom.ScanJobObserverInterface.
   * @param {!ash.scanning.mojom.ScanResult} result
   * @param {!Array<!mojoBase.mojom.FilePath>} scannedFilePaths
   */
  onScanComplete(result, scannedFilePaths) {
    this.scanResult_ = result;
    if (this.scanResult_ !== ash.scanning.mojom.ScanResult.kSuccess ||
        this.objectUrls_.length == 0) {
      this.$.scanFailedDialog.showModal();
      return;
    }

    ++this.numCompletedScansInSession_;
    this.scannedFilePaths_ = scannedFilePaths;
    this.setAppState_(AppState.DONE);
  },

  /**
   * Overrides ash.scanning.mojom.ScanJobObserverInterface.
   * @param {boolean} success
   */
  onCancelComplete(success) {
    // If the cancel request fails, continue showing the scan progress page.
    if (!success) {
      this.setAppState_(AppState.SCANNING);
      this.showToast_('cancelFailedToastText');
      return;
    }

    this.showToast_('scanCanceledToastText');
    this.setAppState_(AppState.READY);
  },

  /**
   * Overrides ash.scanning.mojom.ScanJobObserverInterface.
   * @param {!ash.scanning.mojom.ScanResult} result
   */
  onMultiPageScanFail(result) {},

  /**
   * @param {string} selectedSource
   * @return {!Array<ash.scanning.mojom.PageSize>}
   * @private
   */
  computePageSizes_(selectedSource) {
    for (const source of this.capabilities_.sources) {
      if (source.name === selectedSource) {
        return source.pageSizes;
      }
    }

    return [];
  },

  /**
   * @param {!ash.scanning.mojom.ScannerCapabilities} capabilities
   * @private
   */
  onCapabilitiesReceived_(capabilities) {
    this.capabilities_ = capabilities;
    this.capabilities_.sources.forEach(
        (source) => this.sourceTypeMap_.set(source.name, source.type));
    this.selectedFileType = ash.scanning.mojom.FileType.kPdf.toString();

    this.setAppState_(
        this.scanAppStickySettingsEnabled_ &&
                this.areSavedScanSettingsAvailable_() ?
            AppState.SETTING_SAVED_SETTINGS :
            AppState.READY);
  },

  /**
   * @param {!{scanners: !ScannerArr}} response
   * @private
   */
  onScannersReceived_(response) {
    if (response.scanners.length === 0) {
      this.setAppState_(AppState.NO_SCANNERS);
      return;
    }

    for (const scanner of response.scanners) {
      this.setScannerInfo_(scanner);
      if (this.isLastUsedScanner_(scanner)) {
        this.lastUsedScannerId_ = tokenToString(scanner.id);
      }
    }

    this.setAppState_(AppState.GOT_SCANNERS);
    this.scanners_ = response.scanners;
  },

  /** @private */
  onSelectedScannerIdChange_() {
    assert(this.isSelectedScannerKnown_());

    // If |selectedScannerId| is changed when the app's in a non-READY state,
    // that change was triggered by the app's initial load so it's not counted.
    this.numScanSettingChanges_ = this.appState_ === AppState.READY ? 1 : 0;
    this.setAppState_(AppState.GETTING_CAPS);

    this.getSelectedScannerCapabilities_().then(
        /*@type {!ScannerCapabilitiesResponse}*/ (response) => {
          this.onCapabilitiesReceived_(response.capabilities);
        });
  },

  /** @private */
  onScanClick_() {
    // Force hide the toast if user attempts a new scan before the toast times
    // out.
    this.$.toast.hide();

    if (!this.selectedScannerId || !this.selectedSource ||
        !this.selectedFileType || !this.selectedColorMode ||
        !this.selectedPageSize || !this.selectedResolution) {
      this.showToast_('startScanFailedToast');
      return;
    }

    if (!this.scanJobObserverReceiver_) {
      this.scanJobObserverReceiver_ =
          new ash.scanning.mojom.ScanJobObserverReceiver(
              /**
               * @type {!ash.scanning.mojom.ScanJobObserverInterface}
               */
              (this));
    }

    const settings = this.getScanSettings_();
    if (this.multiPageScanChecked) {
      this.scanService_
          .startMultiPageScan(
              this.getSelectedScannerToken_(), settings,
              this.scanJobObserverReceiver_.$.bindNewPipeAndPassRemote())
          .then(
              /*@type {!StartMultiPageScanResponse}*/
              (response) => {
                this.onStartMultiPageScanResponse_(response);
              });
    } else {
      this.scanService_
          .startScan(
              this.getSelectedScannerToken_(), settings,
              this.scanJobObserverReceiver_.$.bindNewPipeAndPassRemote())
          .then(
              /*@type {!{success: boolean}}*/ (response) => {
                this.onStartScanResponse_(response);
              });
    }

    if (this.scanAppStickySettingsEnabled_) {
      this.saveScanSettings_();
    }

    const scanJobSettingsForMetrics = {
      sourceType: this.sourceTypeMap_.get(this.selectedSource),
      fileType: settings.fileType,
      colorMode: settings.colorMode,
      pageSize: settings.pageSize,
      resolution: settings.resolutionDpi,
    };
    this.browserProxy_.recordScanJobSettings(scanJobSettingsForMetrics);

    this.browserProxy_.recordNumScanSettingChanges(this.numScanSettingChanges_);
    this.numScanSettingChanges_ = 0;
  },

  /** @private */
  onDoneClick_() {
    this.setAppState_(AppState.READY);
  },

  /**
   * @param {!{success: boolean}} response
   * @private
   */
  onStartScanResponse_(response) {
    if (!response.success) {
      this.showToast_('startScanFailedToast');
      return;
    }

    this.setAppState_(AppState.SCANNING);
    this.pageNumber_ = 1;
    this.progressPercent_ = 0;
  },

  /**
   * @param {!StartMultiPageScanResponse} response
   * @private
   */
  onStartMultiPageScanResponse_(response) {
    if (!response.controller) {
      this.showToast_('startScanFailedToast');
      return;
    }

    this.setAppState_(AppState.SCANNING);

    assert(!this.multiPageScanController_);
    this.multiPageScanController_ = response.controller;
    this.pageNumber_ = 1;
    this.progressPercent_ = 0;
  },

  /** @private */
  onScanNextPage_() {
    this.multiPageScanController_
        .scanNextPage(this.getSelectedScannerToken_(), this.getScanSettings_())
        .then(
            /*@type {!{success: boolean}}*/ (response) => {
              this.onScanNextPageResponse_(response);
            });
  },

  /** @private */
  onCompleteMultiPageScan_() {
    this.multiPageScanController_.completeMultiPageScan();
    this.multiPageScanController_.$.close();
    this.multiPageScanController_ = null;
  },

  /**
   * @param {!{success: boolean}} response
   * @private
   */
  onScanNextPageResponse_(response) {
    if (!response.success) {
      this.showToast_('startScanFailedToast');
      return;
    }

    this.setAppState_(AppState.MULTI_PAGE_SCANNING);
    ++this.pageNumber_;
    this.progressPercent_ = 0;
  },

  /** @private */
  toggleClicked_() {
    this.$$('#collapse').toggle();
  },

  /**
   * @return {string}
   * @private
   */
  computeArrowIconDirection_() {
    return this.opened_ ? 'cr:expand-less' : 'cr:expand-more';
  },

  /**
   * @return {string}
   * @private
   */
  getFileSavedText_() {
    const fileSavedText =
        this.pageNumber_ > 1 ? 'fileSavedTextPlural' : 'fileSavedText';
    return this.i18n(fileSavedText);
  },

  /** @private */
  onCancelClick_() {
    assert(this.appState_ === AppState.SCANNING);
    this.setAppState_(AppState.CANCELING);
    this.scanService_.cancelScan();
  },

  /**
   * Revokes and removes all of the object URLs.
   * @private
   */
  clearObjectUrls_() {
    for (const url of this.objectUrls_) {
      URL.revokeObjectURL(url);
    }
    this.objectUrls_ = [];
  },

  /**
   * Sets the app state if the state transition is allowed.
   * @param {!AppState} newState
   * @private
   */
  setAppState_(newState) {
    switch (newState) {
      case (AppState.GETTING_SCANNERS):
        assert(
            this.appState_ === AppState.GETTING_SCANNERS ||
            this.appState_ === AppState.NO_SCANNERS);
        break;
      case (AppState.GOT_SCANNERS):
        assert(this.appState_ === AppState.GETTING_SCANNERS);
        break;
      case (AppState.GETTING_CAPS):
        assert(
            this.appState_ === AppState.GOT_SCANNERS ||
            this.appState_ === AppState.READY);
        break;
      case (AppState.SETTING_SAVED_SETTINGS):
        assert(this.appState_ === AppState.GETTING_CAPS);
        break;
      case (AppState.READY):
        assert(
            this.appState_ === AppState.GETTING_CAPS ||
            this.appState_ === AppState.SETTING_SAVED_SETTINGS ||
            this.appState_ === AppState.SCANNING ||
            this.appState_ === AppState.DONE ||
            this.appState_ === AppState.CANCELING);
        this.clearObjectUrls_();
        break;
      case (AppState.SCANNING):
        assert(
            this.appState_ === AppState.READY ||
            this.appState_ === AppState.CANCELING);
        break;
      case (AppState.DONE):
        assert(
            this.appState_ === AppState.SCANNING ||
            this.appState_ === AppState.CANCELING ||
            this.appState_ === AppState.MULTI_PAGE_NEXT_ACTION);
        break;
      case (AppState.CANCELING):
        assert(this.appState_ === AppState.SCANNING);
        break;
      case (AppState.NO_SCANNERS):
        assert(this.appState_ === AppState.GETTING_SCANNERS);
        break;
      case (AppState.MULTI_PAGE_NEXT_ACTION):
        assert(
            this.appState_ === AppState.SCANNING ||
            this.appState_ === AppState.MULTI_PAGE_SCANNING);
        break;
    }

    this.appState_ = newState;
  },

  /** @private */
  onAppStateChange_() {
    this.scannersLoaded_ = this.appState_ !== AppState.GETTING_SCANNERS &&
        this.appState_ !== AppState.NO_SCANNERS;
    this.settingsDisabled_ = this.appState_ !== AppState.READY;
    this.showCancelButton_ = this.appState_ === AppState.SCANNING ||
        this.appState_ === AppState.CANCELING;
    this.cancelButtonDisabled_ = this.appState_ === AppState.CANCELING;
    this.showDoneSection_ = this.appState_ === AppState.DONE;
    this.showMultiPageScan_ =
        this.appState_ === AppState.MULTI_PAGE_NEXT_ACTION ||
        this.appState_ === AppState.MULTI_PAGE_SCANNING;
    this.showScanSettings_ = !this.showDoneSection_ && !this.showMultiPageScan_;

    // Need to wait for elements to render after updating their disabled and
    // hidden attributes before they can be focused.
    afterNextRender(this, () => {
      if (this.appState_ === AppState.SETTING_SAVED_SETTINGS) {
        this.setScanSettingsFromSavedSettings_();
        this.setAppState_(AppState.READY);
      } else if (this.appState_ === AppState.READY) {
        this.$$('#scannerSelect').$$('#scannerSelect').focus();
      } else if (this.appState_ === AppState.SCANNING) {
        this.$$('#cancelButton').focus();
      } else if (
          this.appState_ === AppState.DONE ||
          this.appState_ === AppState.MULTI_PAGE_NEXT_ACTION) {
        this.$$('#scanPreview').$$('#previewDiv').focus();
      }
    });
  },

  /**
   * @param {string} toastMessageKey
   * @private
   */
  showToast_(toastMessageKey) {
    this.toastMessageKey_ = toastMessageKey;
    this.$.toast.show();
  },

  /** @private */
  onToastMessageKeyChange_() {
    this.showToastInfoIcon_ = this.toastMessageKey_ !== 'scanCanceledToastText';
    this.showToastHelpLink_ =
        this.toastMessageKey_ !== 'scanCanceledToastText' &&
        this.toastMessageKey_ !== 'fileNotFoundToastText';
  },

  /** @private */
  onFileNotFound_() {
    this.showToast_('fileNotFoundToastText');
  },

  /** @private */
  onScanFailedDialogOkClick_() {
    this.$.scanFailedDialog.close();
    this.setAppState_(AppState.READY);
  },

  /** @private */
  onScanFailedDialogGetHelpClick_() {
    this.$.scanFailedDialog.close();
    this.setAppState_(AppState.READY);
    window.open(HELP_PAGE_LINK);
  },

  /**
   * @return {number}
   * @private
   */
  getNumFilesSaved_() {
    return this.selectedFileType ===
            ash.scanning.mojom.FileType.kPdf.toString() ?
        1 :
        this.pageNumber_;
  },

  /** @private */
  onRetryClick_() {
    this.setAppState_(AppState.GETTING_SCANNERS);
    this.scanService_.getScanners().then(
        /*@type {!{scanners: !ScannerArr}}*/ (response) => {
          this.onScannersReceived_(response);
        });
  },

  /** @private */
  onLearnMoreClick_() {
    window.open(HELP_PAGE_LINK);
  },

  /**
   * Increments the counter for the number of scan setting changes before a
   * scan is initiated.
   * @private
   */
  scanSettingsChange_() {
    // The user can only change scan settings when the app is in READY state. If
    // a setting is changed while the app's in a non-READY state, that change
    // was triggered by the scanner's capabilities loading so it's not counted.
    if (this.appState_ !== AppState.READY) {
      return;
    }

    ++this.numScanSettingChanges_;
  },

  /**
   * @return {string}
   * @private
   */
  computeScanFailedDialogTextKey_() {
    switch (this.scanResult_) {
      case (ash.scanning.mojom.ScanResult.kDeviceBusy):
        return 'scanFailedDialogDeviceBusyText';
      case (ash.scanning.mojom.ScanResult.kAdfJammed):
        return 'scanFailedDialogAdfJammedText';
      case (ash.scanning.mojom.ScanResult.kAdfEmpty):
        return 'scanFailedDialogAdfEmptyText';
      case (ash.scanning.mojom.ScanResult.kFlatbedOpen):
        return 'scanFailedDialogFlatbedOpenText';
      case (ash.scanning.mojom.ScanResult.kIoError):
        return 'scanFailedDialogIoErrorText';
      default:
        return 'scanFailedDialogUnknownErrorText';
    }
  },

  /** @private */
  setScanSettingsFromSavedSettings_() {
    if (!this.areSavedScanSettingsAvailable_()) {
      return;
    }

    this.setScanToPathFromSavedSettings_();

    const scannerSettings = this.getSelectedScannerSavedSettings_();
    if (!scannerSettings) {
      return;
    }

    this.setSelectedSourceTypeIfAvailable_(scannerSettings.sourceName);
    this.setSelectedFileTypeIfAvailable_(scannerSettings.fileType);
    this.setSelectedColorModeIfAvailable_(scannerSettings.colorMode);
    this.setSelectedPageSizeIfAvailable_(scannerSettings.pageSize);
    this.setSelectedResolutionIfAvailable_(scannerSettings.resolutionDpi);
  },

  /**
   * @param {!ash.scanning.mojom.Scanner} scanner
     @return {!ScannerInfo}
   * @private
   */
  createScannerInfo_(scanner) {
    return {
      token: scanner.id,
      displayName: getScannerDisplayName(scanner),
    };
  },

  /**
   * @param {!ash.scanning.mojom.Scanner} scanner
   * @private
   */
  setScannerInfo_(scanner) {
    this.scannerInfoMap_.set(
        tokenToString(scanner.id), this.createScannerInfo_(scanner));
  },

  /**
   * @param {!ash.scanning.mojom.Scanner} scanner
   * @return {boolean}
   * @private
   */
  isLastUsedScanner_(scanner) {
    return this.savedScanSettings_.lastUsedScannerName ===
        getScannerDisplayName(scanner);
  },

  /**
   * @return {boolean}
   * @private
   */
  isSelectedScannerKnown_() {
    return this.scannerInfoMap_.has(this.selectedScannerId);
  },

  /**
   * @return {!mojoBase.mojom.UnguessableToken}
   * @private
   */
  getSelectedScannerToken_() {
    return this.scannerInfoMap_.get(this.selectedScannerId).token;
  },

  /**
   * @return {string}
   * @private
   */
  getSelectedScannerDisplayName_() {
    return this.scannerInfoMap_.get(this.selectedScannerId).displayName;
  },

  /**
   * @return {!Promise<!ScannerCapabilitiesResponse>}
   * @private
   */
  getSelectedScannerCapabilities_() {
    return this.scanService_.getScannerCapabilities(
        this.getSelectedScannerToken_());
  },

  /**
   * @return {!ScannerSetting|undefined}
   * @private
   */
  getSelectedScannerSavedSettings_() {
    const selectedScannerDisplayName = this.getSelectedScannerDisplayName_();
    return this.savedScanSettings_.scanners.find(
        scanner => scanner.name === selectedScannerDisplayName);
  },

  /**
   * Validates that the file path from saved settings still exists on the local
   * filesystem then sets the proper display name for the 'Scan to' dropdown. If
   * the file path no longer exists, leave the default 'Scan to' path.
   * @private
   */
  setScanToPathFromSavedSettings_() {
    this.browserProxy_.ensureValidFilePath(this.savedScanSettings_.scanToPath)
        .then(
            /* @type {!SelectedPath} */ (selectedPath) => {
              const baseName = selectedPath.baseName;
              const filePath = selectedPath.filePath;
              if (!baseName || !filePath) {
                return;
              }

              this.selectedFolder = baseName;
              this.selectedFilePath = filePath;
            });
  },

  /** @private */
  saveScanSettings_() {
    assert(this.scanAppStickySettingsEnabled_);

    const scannerName = this.getSelectedScannerDisplayName_();
    this.savedScanSettings_.lastUsedScannerName = scannerName;
    this.savedScanSettings_.scanToPath = this.selectedFilePath;

    // Search the scan settings array for the currently selected scanner. If
    // found, replace it with the new scan settings. If not, add it to the list.
    const newScannerSetting = this.createScannerSettingForSelectedScanner_();
    const scannerIndex = this.savedScanSettings_.scanners.findIndex(
        scanner => scanner.name === scannerName);
    if (scannerIndex === -1) {
      this.savedScanSettings_.scanners.push(newScannerSetting);
    } else {
      this.savedScanSettings_.scanners[scannerIndex] = newScannerSetting;
    }

    if (this.savedScanSettings_.scanners.length > MAX_NUM_SAVED_SCANNERS) {
      this.evictScannersFromScanSettings_();
    }

    this.browserProxy_.saveScanSettings(
        JSON.stringify(this.savedScanSettings_));
  },

  /**
   * Sort the saved settings scanners array so the oldest scanners are moved to
   * the back then dropped.
   * @private
   */
  evictScannersFromScanSettings_() {
    this.savedScanSettings_.scanners.sort((firstScanner, secondScanner) => {
      return new Date(secondScanner.lastScanDate) -
          new Date(firstScanner.lastScanDate);
    });
    this.savedScanSettings_.scanners.splice(MAX_NUM_SAVED_SCANNERS);
  },

  /**
   * @return {!ScannerSetting}
   * @private
   */
  createScannerSettingForSelectedScanner_() {
    return /** @type {!ScannerSetting} */ ({
      name: this.getSelectedScannerDisplayName_(),
      lastScanDate: new Date(),
      sourceName: this.selectedSource,
      fileType: fileTypeFromString(this.selectedFileType),
      colorMode: colorModeFromString(this.selectedColorMode),
      pageSize: pageSizeFromString(this.selectedPageSize),
      resolutionDpi: Number(this.selectedResolution),
    });
  },

  /**
   * @return {boolean}
   * @private
   */
  areSavedScanSettingsAvailable_() {
    return this.savedScanSettings_.scanners.length !== 0;
  },

  /**
   * @return {boolean}
   * @private
   */
  computeShowMultiPageCheckbox_() {
    return this.scanAppMultiPageScanEnabled_ && this.showScanSettings_ &&
        this.isPDFSelected_() && this.isFlatbedSelected_();
  },

  /**
   * @return {boolean}
   * @private
   */
  isPDFSelected_() {
    return !!this.selectedFileType &&
        fileTypeFromString(this.selectedFileType) ===
        ash.scanning.mojom.FileType.kPdf;
  },

  /**
   * @return {boolean}
   * @private
   */
  isFlatbedSelected_() {
    return !!this.selectedSource &&
        this.sourceTypeMap_.get(this.selectedSource) ===
        ash.scanning.mojom.SourceType.kFlatbed;
  },

  /** @private */
  onMultiPageScanCheckedChange_() {
    assert(!this.multiPageScanChecked || this.scanAppMultiPageScanEnabled_);
    const nextPageNum = this.multiPageScanChecked ? 1 : 0;
    this.browserProxy_.getPluralString('scanButtonText', nextPageNum)
        .then(
            /* @type {string} */ (pluralString) => {
              this.scanButtonText_ = pluralString;
            });
  },

  /**
   * @return {!ash.scanning.mojom.ScanSettings}
   * @private
   */
  getScanSettings_() {
    const fileType = fileTypeFromString(this.selectedFileType);
    const colorMode = colorModeFromString(this.selectedColorMode);
    const pageSize = pageSizeFromString(this.selectedPageSize);
    const resolution = Number(this.selectedResolution);
    return {
      sourceName: this.selectedSource,
      scanToPath: {path: this.selectedFilePath},
      fileType: fileType,
      colorMode: colorMode,
      pageSize: pageSize,
      resolutionDpi: resolution,
    };
  },

  /**
   * @param {string} sourceName
   * @private
   */
  setSelectedSourceTypeIfAvailable_(sourceName) {
    if (this.capabilities_.sources.find(source => source.name === sourceName)) {
      this.selectedSource = sourceName;
    }
  },

  /**
   * @param {!ash.scanning.mojom.FileType} fileType
   * @private
   */
  setSelectedFileTypeIfAvailable_(fileType) {
    if (Object.values(ash.scanning.mojom.FileType).includes(fileType)) {
      this.selectedFileType = fileType.toString();
    }
  },

  /**
   * @param {!ash.scanning.mojom.ColorMode} colorMode
   * @private
   */
  setSelectedColorModeIfAvailable_(colorMode) {
    if (this.capabilities_.colorModes.includes(colorMode)) {
      this.selectedColorMode = colorMode.toString();
    }
  },

  /**
   * @param {!ash.scanning.mojom.PageSize} pageSize
   * @private
   */
  setSelectedPageSizeIfAvailable_(pageSize) {
    if (this.selectedSourcePageSizes_.includes(pageSize)) {
      this.selectedPageSize = pageSize.toString();
    }
  },

  /**
   * @param {number} resolution
   * @private
   */
  setSelectedResolutionIfAvailable_(resolution) {
    if (this.capabilities_.resolutions.includes(resolution)) {
      this.selectedResolution = resolution.toString();
    }
  },
});
