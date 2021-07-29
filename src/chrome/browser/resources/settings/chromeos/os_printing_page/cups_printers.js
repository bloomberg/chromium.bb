// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-cups-printers' is a component for showing CUPS
 * Printer settings subpage (chrome://settings/cupsPrinters). It is used to
 * set up legacy & non-CloudPrint printers on ChromeOS by leveraging CUPS (the
 * unix printing system) and the many open source drivers built for CUPS.
 */
// TODO(xdai): Rename it to 'settings-cups-printers-page'.
import '//resources/cr_elements/cr_button/cr_button.m.js';
import '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import '//resources/cr_elements/cr_toast/cr_toast.m.js';
import '//resources/cr_elements/policy/cr_policy_pref_indicator.m.js';
import '//resources/js/action_link.js';
import '//resources/cr_elements/action_link_css.m.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import './cups_settings_add_printer_dialog.js';
import './cups_edit_printer_dialog.js';
import './cups_enterprise_printers.js';
import './cups_printer_shared_css.js';
import './cups_saved_printers.js';
import './cups_nearby_printers.js';
import '../localized_link/localized_link.js';
import '../../icons.js';

import {MojoInterfaceProvider, MojoInterfaceProviderImpl} from '//resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
import {NetworkListenerBehavior} from '//resources/cr_components/chromeos/network/network_listener_behavior.m.js';
import {OncMojo} from '//resources/cr_components/chromeos/network/onc_mojo.m.js';
import {assert, assertNotReached} from '//resources/js/assert.m.js';
import {addWebUIListener, removeWebUIListener, sendWithPromise, WebUIListener} from '//resources/js/cr.m.js';
import {focusWithoutInk} from '//resources/js/cr/ui/focus_without_ink.m.js';
import {loadTimeData} from '//resources/js/load_time_data.m.js';
import {WebUIListenerBehavior} from '//resources/js/web_ui_listener_behavior.m.js';
import {afterNextRender, flush, html, Polymer, TemplateInstanceBase, Templatizer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {Route, RouteObserverBehavior, Router} from '../../router.js';
import {DeepLinkingBehavior} from '../deep_linking_behavior.m.js';
import {routes} from '../os_route.m.js';

import {PrinterListEntry, PrinterType} from './cups_printer_types.js';
import {CupsPrinterInfo, CupsPrintersBrowserProxy, CupsPrintersBrowserProxyImpl, CupsPrintersList, ManufacturersInfo, ModelsInfo, PrinterMakeModel, PrinterPpdMakeModel, PrinterSetupResult, PrintServerResult} from './cups_printers_browser_proxy.js';
import {CupsPrintersEntryManager} from './cups_printers_entry_manager.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'settings-cups-printers',

  behaviors: [
    DeepLinkingBehavior,
    NetworkListenerBehavior,
    RouteObserverBehavior,
    WebUIListenerBehavior,
  ],

  properties: {
    /** @type {!Array<!CupsPrinterInfo>} */
    printers: {
      type: Array,
      notify: true,
    },

    prefs: Object,

    /** @type {?CupsPrinterInfo} */
    activePrinter: {
      type: Object,
      notify: true,
    },

    /** @private {?WebUIListener} */
    onPrintersChangedListener_: {
      type: Object,
      value: null,
    },

    /** @private {?WebUIListener} */
    onEnterprisePrintersChangedListener_: {
      type: Object,
      value: null,
    },

    searchTerm: {
      type: String,
    },

    /** This is also used as an attribute for css styling. */
    canAddPrinter: {
      type: Boolean,
      reflectToAttribute: true,
    },

    /**
     * @type {!Array<!PrinterListEntry>}
     * @private
     */
    savedPrinters_: {
      type: Array,
      value: () => [],
    },

    /**
     * @type {!Array<!PrinterListEntry>}
     * @private
     */
    enterprisePrinters_: {
      type: Array,
      value: () => [],
    },

    /** @private */
    attemptedLoadingPrinters_: {
      type: Boolean,
      value: false,
    },

    /** @private */
    showCupsEditPrinterDialog_: Boolean,

    /**@private */
    addPrinterResultText_: String,

    /**@private */
    nearbyPrintersAriaLabel_: {
      type: String,
      computed: 'getNearbyPrintersAriaLabel_(nearbyPrinterCount_)',
    },

    /**@private */
    savedPrintersAriaLabel_: {
      type: String,
      computed: 'getSavedPrintersAriaLabel_(savedPrinterCount_)',
    },

    /**@private */
    enterprisePrintersAriaLabel_: {
      type: String,
      computed: 'getEnterprisePrintersAriaLabel_(enterprisePrinterCount_)',
    },

    /**@private */
    nearbyPrinterCount_: {
      type: Number,
      value: 0,
    },

    /**@private */
    savedPrinterCount_: {
      type: Number,
      value: 0,
    },

    /** @private */
    enterprisePrinterCount_: {
      type: Number,
      value: 0,
    },

    /**
     * Used by DeepLinkingBehavior to focus this page's deep links.
     * @type {!Set<!chromeos.settings.mojom.Setting>}
     */
    supportedSettingIds: {
      type: Object,
      value: () => new Set([
        chromeos.settings.mojom.Setting.kAddPrinter,
        chromeos.settings.mojom.Setting.kSavedPrinters,
      ]),
    },
  },

  listeners: {
    'edit-cups-printer-details': 'onShowCupsEditPrinterDialog_',
    'show-cups-printer-toast': 'openResultToast_',
    'add-print-server-and-show-toast': 'addPrintServerAndShowResultToast_',
    'open-manufacturer-model-dialog-for-specified-printer':
        'openManufacturerModelDialogForSpecifiedPrinter_',
  },

  /** @private {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote} */
  networkConfig_: null,

  /** @private {CupsPrintersEntryManager} */
  entryManager_: null,

  /** @override */
  created() {
    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
    this.entryManager_ = CupsPrintersEntryManager.getInstance();
  },

  /** @override */
  attached() {
    this.networkConfig_
        .getNetworkStateList({
          filter: chromeos.networkConfig.mojom.FilterType.kActive,
          networkType: chromeos.networkConfig.mojom.NetworkType.kAll,
          limit: chromeos.networkConfig.mojom.NO_LIMIT,
        })
        .then((responseParams) => {
          this.onActiveNetworksChanged(responseParams.result);
        });
  },

  /** @override */
  ready() {
    this.updateCupsPrintersList_();
  },

  /**
   * Overridden from DeepLinkingBehavior.
   * @param {!chromeos.settings.mojom.Setting} settingId
   * @return {boolean}
   */
  beforeDeepLinkAttempt(settingId) {
    // Manually show the deep links for settings nested within elements.
    if (settingId !== chromeos.settings.mojom.Setting.kSavedPrinters) {
      // Continue with deep link attempt.
      return true;
    }

    afterNextRender(this, () => {
      const savedPrinters = this.$$('#savedPrinters');
      const printerEntry =
          savedPrinters && savedPrinters.$$('settings-cups-printers-entry');
      const deepLinkElement = printerEntry && printerEntry.$$('#moreActions');
      if (!deepLinkElement || deepLinkElement.hidden) {
        console.warn(`Element with deep link id ${settingId} not focusable.`);
        return;
      }
      this.showDeepLinkElement(deepLinkElement);
    });
    // Stop deep link attempt since we completed it manually.
    return false;
  },

  /**
   * RouteObserverBehavior
   * @param {!Route} route
   * @protected
   */
  currentRouteChanged(route) {
    if (route !== routes.CUPS_PRINTERS) {
      if (this.onPrintersChangedListener_) {
        removeWebUIListener(
            /** @type {WebUIListener} */ (this.onPrintersChangedListener_));
        this.onPrintersChangedListener_ = null;
      }
      this.entryManager_.removeWebUIListeners();
      return;
    }

    this.entryManager_.addWebUIListeners();
    this.onPrintersChangedListener_ = addWebUIListener(
        'on-saved-printers-changed', this.onSavedPrintersChanged_.bind(this));
    this.onEnterprisePrintersChangedListener_ = addWebUIListener(
        'on-enterprise-printers-changed',
        this.onEnterprisePrintersChanged_.bind(this));
    this.updateCupsPrintersList_();
    this.attemptDeepLink();
  },

  /**
   * CrosNetworkConfigObserver impl
   * @param {!Array<chromeos.networkConfig.mojom.NetworkStateProperties>}
   *     networks
   * @private
   */
  onActiveNetworksChanged(networks) {
    this.canAddPrinter = networks.some(function(network) {
      // Note: Check for kOnline rather than using
      // OncMojo.connectionStateIsConnected() since the latter could return true
      // for networks without connectivity (e.g., captive portals).
      return network.connectionState ===
          chromeos.networkConfig.mojom.ConnectionStateType.kOnline;
    });
  },

  /**
   * @param {!CustomEvent<!{
   *      resultCode: PrinterSetupResult,
   *      printerName: string
   * }>} event
   * @private
   */
  openResultToast_(event) {
    const printerName = event.detail.printerName;
    switch (event.detail.resultCode) {
      case PrinterSetupResult.SUCCESS:
        this.addPrinterResultText_ = loadTimeData.getStringF(
            'printerAddedSuccessfulMessage', printerName);
        break;
      case PrinterSetupResult.EDIT_SUCCESS:
        this.addPrinterResultText_ = loadTimeData.getStringF(
            'printerEditedSuccessfulMessage', printerName);
        break;
      case PrinterSetupResult.PRINTER_UNREACHABLE:
        this.addPrinterResultText_ =
            loadTimeData.getStringF('printerUnavailableMessage', printerName);
        break;
      default:
        assertNotReached();
    }

    this.$.errorToast.show();
  },

  /**
   * @param {!CustomEvent<!{
   *      printers: !CupsPrintersList
   * }>} event
   * @private
   */
  addPrintServerAndShowResultToast_: function(event) {
    this.entryManager_.addPrintServerPrinters(event.detail.printers);
    const length = event.detail.printers.printerList.length;
    if (length === 0) {
      this.addPrintServerResultText_ =
          loadTimeData.getString('printServerFoundZeroPrinters');
    } else if (length === 1) {
      this.addPrintServerResultText_ =
          loadTimeData.getString('printServerFoundOnePrinter');
    } else {
      this.addPrintServerResultText_ =
          loadTimeData.getStringF('printServerFoundManyPrinters', length);
    }
    this.$.printServerErrorToast.show();
  },

  /**
   * @param {!CustomEvent<{item: !CupsPrinterInfo}>} e
   * @private
   */
  openManufacturerModelDialogForSpecifiedPrinter_(e) {
    const item = e.detail.item;
    this.$.addPrinterDialog.openManufacturerModelDialogForSpecifiedPrinter(
        item);
  },

  /** @private */
  updateCupsPrintersList_() {
    CupsPrintersBrowserProxyImpl.getInstance().getCupsSavedPrintersList().then(
        this.onSavedPrintersChanged_.bind(this));

    CupsPrintersBrowserProxyImpl.getInstance()
        .getCupsEnterprisePrintersList()
        .then(this.onEnterprisePrintersChanged_.bind(this));
  },

  /**
   * @param {!CupsPrintersList} cupsPrintersList
   * @private
   */
  onSavedPrintersChanged_(cupsPrintersList) {
    this.savedPrinters_ = cupsPrintersList.printerList.map(
        printer => /** @type {!PrinterListEntry} */ (
            {printerInfo: printer, printerType: PrinterType.SAVED}));
    this.entryManager_.setSavedPrintersList(this.savedPrinters_);
    // Used to delay rendering nearby and add printer sections to prevent
    // "Add Printer" flicker when clicking "Printers" in settings page.
    this.attemptedLoadingPrinters_ = true;
  },

  /**
   * @param {!CupsPrintersList} cupsPrintersList
   * @private
   */
  onEnterprisePrintersChanged_(cupsPrintersList) {
    this.enterprisePrinters_ = cupsPrintersList.printerList.map(
        printer => /** @type {!PrinterListEntry} */ (
            {printerInfo: printer, printerType: PrinterType.ENTERPRISE}));
    this.entryManager_.setEnterprisePrintersList(this.enterprisePrinters_);
  },

  /** @private */
  onAddPrinterTap_() {
    this.$.addPrinterDialog.open();
  },

  /** @private */
  onAddPrinterDialogClose_() {
    focusWithoutInk(assert(this.$$('#addManualPrinterIcon')));
  },

  /** @private */
  onShowCupsEditPrinterDialog_() {
    this.showCupsEditPrinterDialog_ = true;
  },

  /** @private */
  onEditPrinterDialogClose_() {
    this.showCupsEditPrinterDialog_ = false;
  },

  /**
   * @param {string} searchTerm
   * @return {boolean} If the 'no-search-results-found' string should be shown.
   * @private
   */
  showNoSearchResultsMessage_(searchTerm) {
    if (!searchTerm || !this.printers.length) {
      return false;
    }
    searchTerm = searchTerm.toLowerCase();
    return !this.printers.some(printer => {
      return printer.printerName.toLowerCase().includes(searchTerm);
    });
  },

  /**
   * @param {boolean} connectedToNetwork Whether the device is connected to
         a network.
   * @param {boolean} userPrintersAllowed Whether users are allowed to
         configure their own native printers.
   * @return {boolean} Whether the 'Add Printer' button is active.
   * @private
   */
  addPrinterButtonActive_(connectedToNetwork, userPrintersAllowed) {
    return connectedToNetwork && userPrintersAllowed;
  },

  /**
   * @return {boolean} Whether |savedPrinters_| is empty.
   * @private
   */
  doesAccountHaveSavedPrinters_() {
    return !!this.savedPrinters_.length;
  },

  /**
   * @return {boolean} Whether |enterprisePrinters_| is empty.
   * @private
   */
  doesAccountHaveEnterprisePrinters_() {
    return !!this.enterprisePrinters_.length;
  },

  /** @private */
  getSavedPrintersAriaLabel_() {
    let printerLabel = '';
    if (this.savedPrinterCount_ === 0) {
      printerLabel = 'savedPrintersCountNone';
    } else if (this.savedPrinterCount_ === 1) {
      printerLabel = 'savedPrintersCountOne';
    } else {
      printerLabel = 'savedPrintersCountMany';
    }
    return loadTimeData.getStringF(printerLabel, this.savedPrinterCount_);
  },

  /** @private */
  getNearbyPrintersAriaLabel_() {
    let printerLabel = '';
    if (this.nearbyPrinterCount_ === 0) {
      printerLabel = 'nearbyPrintersCountNone';
    } else if (this.nearbyPrinterCount_ === 1) {
      printerLabel = 'nearbyPrintersCountOne';
    } else {
      printerLabel = 'nearbyPrintersCountMany';
    }
    return loadTimeData.getStringF(printerLabel, this.nearbyPrinterCount_);
  },

  /** @private */
  getEnterprisePrintersAriaLabel_() {
    let printerLabel = '';
    if (this.enterprisePrinterCount_ === 0) {
      printerLabel = 'enterprisePrintersCountNone';
    } else if (this.enterprisePrinterCount_ === 1) {
      printerLabel = 'enterprisePrintersCountOne';
    } else {
      printerLabel = 'enterprisePrintersCountMany';
    }
    return loadTimeData.getStringF(printerLabel, this.enterprisePrinterCount_);
  },
});
