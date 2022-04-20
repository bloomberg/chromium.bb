// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertNotReached} from 'chrome://resources/js/assert_ts.js';
import {isChromeOS, isLacros} from 'chrome://resources/js/cr.m.js';

import {Destination, DestinationOptionalParams, DestinationOrigin, PrinterType} from './destination.js';
// <if expr="chromeos_ash or chromeos_lacros">
import {DestinationProvisionalType} from './destination.js';
// </if>

type ObjectMap = {
  [k: string]: any,
};

export type LocalDestinationInfo = {
  deviceName: string,
  printerName: string,
  printerDescription?: string,
  cupsEnterprisePrinter?: boolean,
  printerOptions?: ObjectMap,
};

export type ExtensionDestinationInfo = {
  extensionId: string,
  extensionName: string,
  id: string,
  name: string,
  description?: string,
  provisional?: boolean,
};

/**
 * @param type The type of printer to parse.
 * @param printer Information about the printer.
 *       Type expected depends on |type|:
 *       For LOCAL_PRINTER => LocalDestinationInfo
 *       For EXTENSION_PRINTER => ExtensionDestinationInfo
 * @return Destination, or null if an invalid value is provided for |type|.
 */
export function parseDestination(
    type: PrinterType,
    printer: (LocalDestinationInfo|ExtensionDestinationInfo)): Destination {
  if (type === PrinterType.LOCAL_PRINTER || type === PrinterType.PDF_PRINTER) {
    return parseLocalDestination(printer as LocalDestinationInfo);
  }
  if (type === PrinterType.EXTENSION_PRINTER) {
    return parseExtensionDestination(printer as ExtensionDestinationInfo);
  }
  assertNotReached('Unknown printer type ' + type);
}

/**
 * @param destinationInfo Information describing a local print destination.
 * @return Parsed local print destination.
 */
function parseLocalDestination(destinationInfo: LocalDestinationInfo):
    Destination {
  const options: DestinationOptionalParams = {
    description: destinationInfo.printerDescription,
    isEnterprisePrinter: destinationInfo.cupsEnterprisePrinter,
    location: '',
  };
  const locationOptions = new Set(['location', 'printer-location']);
  if (destinationInfo.printerOptions) {
    // The only printer option currently used by Print Preview's UI is location.
    for (const printerOption of Object.keys(destinationInfo.printerOptions)) {
      if (locationOptions.has(printerOption)) {
        options.location = destinationInfo.printerOptions[printerOption] || '';
      }
    }
  }
  return new Destination(
      destinationInfo.deviceName,
      (isChromeOS || isLacros) ? DestinationOrigin.CROS :
                                 DestinationOrigin.LOCAL,
      destinationInfo.printerName, options);
}

/**
 * Parses an extension destination from an extension supplied printer
 * description.
 */
export function parseExtensionDestination(
    destinationInfo: ExtensionDestinationInfo): Destination {
  // <if expr="chromeos_ash or chromeos_lacros">
  const provisionalType = destinationInfo.provisional ?
      DestinationProvisionalType.NEEDS_USB_PERMISSION :
      DestinationProvisionalType.NONE;
  // </if>

  return new Destination(
      destinationInfo.id, DestinationOrigin.EXTENSION, destinationInfo.name, {
        description: destinationInfo.description || '',
        extensionId: destinationInfo.extensionId,
        extensionName: destinationInfo.extensionName || '',
        // <if expr="chromeos_ash or chromeos_lacros">
        provisionalType: provisionalType,
        // </if>
      });
}
