// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/shared_vars_css.m.js';
import 'chrome://resources/js/util.m.js';
import 'chrome://resources/polymer/v3_0/iron-iconset-svg/iron-iconset-svg.js';
import 'chrome://resources/polymer/v3_0/iron-icon/iron-icon.js';
import './destination_dropdown_cros.js';
import './destination_select_css.js';
import './icons.js';
import './print_preview_shared_css.js';
import './throbber_css.js';
import '../strings.m.js';

import {assert} from 'chrome://resources/js/assert.m.js';
import {I18nBehavior} from 'chrome://resources/js/i18n_behavior.m.js';
import {html, mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CloudOrigins, Destination, DestinationOrigin, GooglePromotedDestinationId, PDF_DESTINATION_KEY, RecentDestination, SAVE_TO_DRIVE_CROS_DESTINATION_KEY} from '../data/destination.js';
import {ERROR_STRING_KEY_MAP, getPrinterStatusIcon, PrinterStatusReason} from '../data/printer_status_cros.js';

import {SelectMixin, SelectMixinInterface} from './select_mixin.js';

const PrintPreviewDestinationSelectCrosElementBase =
    mixinBehaviors([I18nBehavior], SelectMixin(PolymerElement)) as
    {new (): I18nBehavior & SelectMixinInterface & PolymerElement};

export class PrintPreviewDestinationSelectCrosElement extends
    PrintPreviewDestinationSelectCrosElementBase {
  static get is() {
    return 'print-preview-destination-select-cros';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      activeUser: String,

      dark: Boolean,

      destination: Object,

      disabled: Boolean,

      driveDestinationKey: String,

      loaded: Boolean,

      noDestinations: Boolean,

      pdfPrinterDisabled: Boolean,

      recentDestinationList: {
        type: Array,
        observer: 'onRecentDestinationListChanged_',
      },

      pdfDestinationKey_: {
        type: String,
        value: PDF_DESTINATION_KEY,
      },

      statusText_: {
        type: String,
        computed:
            'computeStatusText_(destination, destination.printerStatusReason)',
        observer: 'onStatusTextSet_',
      },

      destinationIcon_: {
        type: String,
        computed: 'computeDestinationIcon_(' +
            'selectedValue, destination, destination.printerStatusReason)',
      },

      isCurrentDestinationCrosLocal_: {
        type: Boolean,
        computed: 'computeIsCurrentDestinationCrosLocal_(destination)',
        reflectToAttribute: true,
      },
    };
  }

  destination: Destination;
  pdfPrinterDisabled: boolean;
  recentDestinationList: Destination[];
  private pdfDestinationKey_: string;
  private statusText_: string;
  private destinationIcon_: string;
  private isCurrentDestinationCrosLocal_: boolean;

  focus() {
    this.shadowRoot!.querySelector(
                        'print-preview-destination-dropdown-cros')!.focus();
  }

  /** Sets the select to the current value of |destination|. */
  updateDestination() {
    this.selectedValue = this.destination.key;
  }

  /**
   * Returns the iconset and icon for the selected printer. If printer details
   * have not yet been retrieved from the backend, attempts to return an
   * appropriate icon early based on the printer's sticky information.
   * @return The iconset and icon for the current selection.
   */
  private computeDestinationIcon_(): string {
    if (!this.selectedValue) {
      return '';
    }

    // If the destination matches the selected value, pull the icon from the
    // destination.
    if (this.destination && this.destination.key === this.selectedValue) {
      if (this.isCurrentDestinationCrosLocal_) {
        return getPrinterStatusIcon(
            this.destination.printerStatusReason,
            this.destination.isEnterprisePrinter);
      }

      return this.destination.icon;
    }

    // Check for the Docs or Save as PDF ids first.
    const keyParams = this.selectedValue.split('/');
    if (keyParams[0] === GooglePromotedDestinationId.SAVE_TO_DRIVE_CROS) {
      return 'print-preview:save-to-drive';
    }

    if (keyParams[0] === GooglePromotedDestinationId.DOCS) {
      return 'print-preview:save-to-drive';
    }

    if (keyParams[0] === GooglePromotedDestinationId.SAVE_AS_PDF) {
      return 'cr:insert-drive-file';
    }

    // Otherwise, must be in the recent list.
    const recent = this.recentDestinationList.find(d => {
      return d.key === this.selectedValue;
    });
    if (recent && recent.icon) {
      return recent.icon;
    }

    // The key/recent destinations don't have information about what icon to
    // use, so just return the generic print icon for now. It will be updated
    // when the destination is set.
    return 'print-preview:print';
  }

  private fireSelectedOptionChange_(value: string) {
    this.dispatchEvent(new CustomEvent(
        'selected-option-change',
        {bubbles: true, composed: true, detail: value}));
  }

  onProcessSelectChange(value: string) {
    this.fireSelectedOptionChange_(value);
  }

  private onDropdownValueSelected_(e: CustomEvent<HTMLButtonElement>) {
    const selectedItem = e.detail;
    if (!selectedItem || selectedItem.value === this.destination.key) {
      return;
    }

    this.fireSelectedOptionChange_(selectedItem.value);
  }

  /**
   * Send a printer status request for any new destination in the dropdown.
   */
  private onRecentDestinationListChanged_() {
    for (const destination of this.recentDestinationList) {
      if (!destination || destination.origin !== DestinationOrigin.CROS) {
        continue;
      }

      destination.requestPrinterStatus().then(
          destinationKey => this.onPrinterStatusReceived_(destinationKey));
    }
  }

  /**
   * Check if the printer is currently in the dropdown then update its status
   *    icon if it's present.
   */
  private onPrinterStatusReceived_(destinationKey: string) {
    const indexFound = this.recentDestinationList.findIndex(destination => {
      return destination.key === destinationKey;
    });
    if (indexFound === -1) {
      return;
    }

    // Use notifyPath to trigger the matching printer located in the dropdown to
    // recalculate its status icon.
    this.notifyPath(`recentDestinationList.${indexFound}.printerStatusReason`);

    // If |destinationKey| matches the currently selected printer, use
    // notifyPath to trigger the destination to recalculate its status icon and
    // error status text.
    if (this.destination && this.destination.key === destinationKey) {
      this.notifyPath(`destination.printerStatusReason`);
    }
  }

  /**
   * @return An error status for the current destination. If no error
   *     status exists, an empty string.
   */
  private computeStatusText_(): string {
    // |destination| can be either undefined, or null here.
    if (!this.destination) {
      return '';
    }

    // Cloudprint destinations contain their own status text.
    if (CloudOrigins.some(origin => origin === this.destination.origin)) {
      if (this.destination.shouldShowInvalidCertificateError) {
        return this.i18n('noLongerSupportedFragment');
      }
      if (this.destination.connectionStatusText) {
        return this.destination.connectionStatusText;
      }
    }

    // Non-local printers do not show an error status.
    if (this.destination.origin !== DestinationOrigin.CROS) {
      return '';
    }

    const printerStatusReason = this.destination.printerStatusReason;
    if (printerStatusReason === null ||
        printerStatusReason === PrinterStatusReason.NO_ERROR ||
        printerStatusReason === PrinterStatusReason.UNKNOWN_REASON) {
      return '';
    }

    return this.getErrorString_(printerStatusReason);
  }

  private onStatusTextSet_() {
    this.shadowRoot!.querySelector('#statusText')!.innerHTML = this.statusText_;
  }

  private getErrorString_(printerStatusReason: PrinterStatusReason): string {
    const errorStringKey = ERROR_STRING_KEY_MAP.get(printerStatusReason);
    return errorStringKey ? this.i18n(errorStringKey) : '';
  }

  /**
   * True when the currently selected destination is a CrOS local printer.
   */
  private computeIsCurrentDestinationCrosLocal_(): boolean {
    return this.destination &&
        this.destination.origin === DestinationOrigin.CROS;
  }

  /**
   * Return the options currently visible to the user for testing purposes.
   */
  getVisibleItemsForTest(): NodeListOf<Element> {
    return this.shadowRoot!.querySelector('#dropdown')!.shadowRoot!
        .querySelectorAll('.list-item:not([hidden])');
  }
}

customElements.define(
    PrintPreviewDestinationSelectCrosElement.is,
    PrintPreviewDestinationSelectCrosElement);
