// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CapabilitiesResponse, Cdd, DEFAULT_MAX_COPIES, Destination, DestinationCertificateStatus, DestinationConnectionStatus, DestinationOrigin, DestinationStore, DestinationType, GooglePromotedDestinationId, LocalDestinationInfo, MeasurementSystemUnitType, MediaSizeCapability, MediaSizeOption, NativeInitialSettings, VendorCapabilityValueType} from 'chrome://print/print_preview.js';
import {CrInputElement} from 'chrome://resources/cr_elements/cr_input/cr_input.m.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {WebUIListenerMixin} from 'chrome://resources/js/web_ui_listener_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {eventToPromise} from 'chrome://webui-test/test_util.js';

export function getDefaultInitialSettings(isPdf: boolean = false):
    NativeInitialSettings {
  return {
    isInKioskAutoPrintMode: false,
    isInAppKioskMode: false,
    pdfPrinterDisabled: false,
    thousandsDelimiter: ',',
    decimalDelimiter: '.',
    previewModifiable: !isPdf,
    documentTitle: 'title',
    documentHasSelection: true,
    shouldPrintSelectionOnly: false,
    previewIsFromArc: false,
    printerName: 'FooDevice',
    serializedAppStateStr: null,
    serializedDefaultDestinationSelectionRulesStr: null,
    destinationsManaged: false,
    uiLocale: 'en-us',
    unitType: MeasurementSystemUnitType.IMPERIAL,
    isDriveMounted: true,
  };
}

export function getCddTemplate(
    printerId: string, opt_printerName?: string): CapabilitiesResponse {
  const template: CapabilitiesResponse = {
    printer: {
      deviceName: printerId,
      printerName: opt_printerName || '',
    },
    capabilities: {
      version: '1.0',
      printer: {
        collate: {default: true},
        copies: {default: 1, max: DEFAULT_MAX_COPIES},
        color: {
          option: [
            {type: 'STANDARD_COLOR', is_default: true},
            {type: 'STANDARD_MONOCHROME'}
          ]
        },
        dpi: {
          option: [
            {horizontal_dpi: 200, vertical_dpi: 200, is_default: true},
            {horizontal_dpi: 100, vertical_dpi: 100},
          ]
        },
        duplex: {
          option: [
            {type: 'NO_DUPLEX', is_default: true}, {type: 'LONG_EDGE'},
            {type: 'SHORT_EDGE'}
          ]
        },
        page_orientation: {
          option: [
            {type: 'PORTRAIT', is_default: true}, {type: 'LANDSCAPE'},
            {type: 'AUTO'}
          ]
        },
        media_size: {
          option: [
            {
              name: 'NA_LETTER',
              width_microns: 215900,
              height_microns: 279400,
              is_default: true,
              custom_display_name: 'Letter',
            },
            {
              name: 'CUSTOM',
              width_microns: 215900,
              height_microns: 215900,
              custom_display_name: 'CUSTOM_SQUARE',
            }
          ]
        }
      }
    }
  };
  // <if expr="chromeos or lacros">
  template.capabilities!.printer.pin = {supported: true};
  // </if>
  return template;
}

/**
 * Gets a CDD template and adds some dummy vendor capabilities. For select
 * capabilities, the values of these options are arbitrary. These values are
 * provided and read by the destination, so there are no fixed options like
 * there are for margins or color.
 * @param opt_printerName Defaults to an empty string.
 */
export function getCddTemplateWithAdvancedSettings(
    numSettings: number, printerId: string,
    opt_printerName?: string): CapabilitiesResponse {
  const template = getCddTemplate(printerId, opt_printerName);
  if (numSettings < 1) {
    return template;
  }

  template.capabilities!.printer.vendor_capability = [{
    display_name: 'Print Area',
    id: 'printArea',
    type: 'SELECT',
    select_cap: {
      option: [
        {display_name: 'A4', value: 4, is_default: true},
        {display_name: 'A6', value: 6},
        {display_name: 'A7', value: 7},
      ],
    },
  }];

  if (numSettings < 2) {
    return template;
  }

  // Add new capability.
  template.capabilities!.printer.vendor_capability!.push({
    display_name: 'Paper Type',
    id: 'paperType',
    type: 'SELECT',
    select_cap: {
      option: [
        {display_name: 'Standard', value: 0, is_default: true},
        {display_name: 'Recycled', value: 1},
        {display_name: 'Special', value: 2}
      ]
    }
  });

  if (numSettings < 3) {
    return template;
  }

  template.capabilities!.printer.vendor_capability!.push({
    display_name: 'Watermark',
    id: 'watermark',
    type: 'TYPED_VALUE',
    typed_value_cap: {
      default: '',
    }
  });

  if (numSettings < 4) {
    return template;
  }

  template.capabilities!.printer.vendor_capability.push({
    display_name: 'Staple',
    id: 'finishings/4',
    type: 'TYPED_VALUE',
    typed_value_cap: {
      default: '',
      value_type: VendorCapabilityValueType.BOOLEAN,
    }
  });

  return template;
}

/**
 * Creates a destination with a certificate status tag.
 * @param id Printer id
 * @param name Printer display name
 * @param invalid Whether printer has an invalid certificate.
 */
export function createDestinationWithCertificateStatus(
    id: string, name: string, invalid: boolean) {
  const tags = {
    certificateStatus: invalid ? DestinationCertificateStatus.NO :
                                 DestinationCertificateStatus.UNKNOWN,
    account: 'foo@chromium.org',
  };
  const dest = new Destination(
      id, DestinationType.GOOGLE, DestinationOrigin.COOKIES, name,
      DestinationConnectionStatus.ONLINE, tags);
  return dest;
}

/**
 * @return The capabilities of the Save as PDF destination.
 */
export function getPdfPrinter(): {capabilities: Cdd} {
  return {
    capabilities: {
      version: '1.0',
      printer: {
        page_orientation: {
          option: [
            {type: 'AUTO', is_default: true}, {type: 'PORTRAIT'},
            {type: 'LANDSCAPE'}
          ]
        },
        color: {option: [{type: 'STANDARD_COLOR', is_default: true}]},
        media_size: {
          option: [{
            name: 'NA_LETTER',
            width_microns: 0,
            height_microns: 0,
            is_default: true
          }]
        }
      }
    }
  };
}

/**
 * Get the default media size for |device|.
 * @return The width and height of the default media.
 */
export function getDefaultMediaSize(device: CapabilitiesResponse):
    MediaSizeOption {
  const size = device.capabilities!.printer.media_size!.option!.find(
      opt => !!opt.is_default);
  return {
    width_microns: size!.width_microns,
    height_microns: size!.height_microns
  };
}

/**
 * Get the default page orientation for |device|.
 * @return The default orientation.
 */
export function getDefaultOrientation(device: CapabilitiesResponse): string {
  const options = device.capabilities!.printer.page_orientation!.option;
  return assert(options!.find(opt => !!opt.is_default)!.type!);
}

/**
 * Creates 5 local destinations, adds them to |localDestinations|.
 */
export function getDestinations(localDestinations: LocalDestinationInfo[]):
    Destination[] {
  const destinations: Destination[] = [];
  // <if expr="not chromeos and not lacros">
  const origin = DestinationOrigin.LOCAL;
  // </if>
  // <if expr="chromeos or lacros">
  const origin = DestinationOrigin.CROS;
  // </if>
  // Five destinations. FooDevice is the system default.
  [{deviceName: 'ID1', printerName: 'One'},
   {deviceName: 'ID2', printerName: 'Two'},
   {deviceName: 'ID3', printerName: 'Three'},
   {deviceName: 'ID4', printerName: 'Four'},
   {deviceName: 'FooDevice', printerName: 'FooName'}]
      .forEach(info => {
        const destination = new Destination(
            info.deviceName, DestinationType.LOCAL, origin, info.printerName,
            DestinationConnectionStatus.ONLINE);
        localDestinations.push(info);
        destinations.push(destination);
      });
  return destinations;
}

/**
 * Returns a media size capability with custom and localized names.
 */
export function getMediaSizeCapabilityWithCustomNames(): MediaSizeCapability {
  const customLocalizedMediaName = 'Vendor defined localized media name';
  const customMediaName = 'Vendor defined media name';

  return {
    option: [
      {
        name: 'CUSTOM',
        width_microns: 15900,
        height_microns: 79400,
        is_default: true,
        custom_display_name_localized:
            [{locale: navigator.language, value: customLocalizedMediaName}]
      },
      {
        name: 'CUSTOM',
        width_microns: 15900,
        height_microns: 79400,
        custom_display_name: customMediaName
      }
    ]
  };
}

/**
 * @param input The value to set for the input element.
 * @param parentElement The element that receives the input-change event.
 * @return Promise that resolves when the input-change event has fired.
 */
export function triggerInputEvent(
    inputElement: HTMLInputElement|CrInputElement, input: string,
    parentElement: HTMLElement): Promise<void> {
  inputElement.value = input;
  inputElement.dispatchEvent(
      new CustomEvent('input', {composed: true, bubbles: true}));
  return eventToPromise('input-change', parentElement);
}

const TestListenerElementBase = WebUIListenerMixin(PolymerElement);
class TestListenerElement extends TestListenerElementBase {
  static get is() {
    return 'test-listener-element';
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'test-listener-element': TestListenerElement;
  }
}

export function setupTestListenerElement(): void {
  customElements.define(TestListenerElement.is, TestListenerElement);
}

export function createDestinationStore(): DestinationStore {
  const testListenerElement = document.createElement('test-listener-element');
  document.body.appendChild(testListenerElement);
  return new DestinationStore(
      testListenerElement.addWebUIListener.bind(testListenerElement));
}

// <if expr="chromeos or lacros">
/**
 * @return The Google Drive destination.
 */
export function getGoogleDriveDestination(_account: string): Destination {
  return new Destination(
      'Save to Drive CrOS', DestinationType.LOCAL, DestinationOrigin.LOCAL,
      'Save to Google Drive', DestinationConnectionStatus.ONLINE);
}
// </if>
// <if expr="not chromeos and not lacros">
/**
 * @param account The user account the destination should be associated with.
 * @return The Google Drive destination.
 */
export function getGoogleDriveDestination(account: string): Destination {
  return getCloudDestination(
      GooglePromotedDestinationId.DOCS, GooglePromotedDestinationId.DOCS,
      account);
}
// </if>

/**
 * @param account The user account the destination should be associated with.
 */
export function getCloudDestination(
    id: string, name: string, account: string): Destination {
  return new Destination(
      id, DestinationType.GOOGLE, DestinationOrigin.COOKIES, name,
      DestinationConnectionStatus.ONLINE, {account: account});
}

/** @return The Save as PDF destination. */
export function getSaveAsPdfDestination(): Destination {
  return new Destination(
      GooglePromotedDestinationId.SAVE_AS_PDF, DestinationType.LOCAL,
      DestinationOrigin.LOCAL, loadTimeData.getString('printToPDF'),
      DestinationConnectionStatus.ONLINE);
}

/**
 * @param section The settings section that contains the select to toggle.
 * @param option The option to select.
 * @return Promise that resolves when the option has been selected and the
 *     process-select-change event has fired.
 */
export function selectOption(
    section: HTMLElement, option: string): Promise<void> {
  const select = section.shadowRoot!.querySelector('select')!;
  select.value = option;
  select.dispatchEvent(new CustomEvent('change'));
  return eventToPromise('process-select-change', section);
}
