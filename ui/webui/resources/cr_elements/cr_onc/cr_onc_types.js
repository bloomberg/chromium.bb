// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Typedefs for CrOncDataElement.data. Note: These 'types' define
 * a subset of ONC properties in the ONC data dictionary. The first letter is
 * capitalized to match the ONC spec and avoid an extra layer of translation.
 * See components/onc/docs/onc_spec.html for the complete spec.
 */

var CrOnc = {};

/** @typedef {string|!Object} */
CrOnc.ManagedStringType;

/**
 * @typedef {{NetworkTechnology: string, Strength: number}}
 */
CrOnc.CellularType;

/**
 * @typedef {{Security: string, Strength: number}}
 */
CrOnc.WiFiType;

/**
 * @typedef {{Strength: number}}
 */
CrOnc.WiMAXType;

/** @enum {string} */
CrOnc.Type = {
  CELLULAR: "Cellular",
  ETHERNET: "Ethernet",
  VPN: "VPN",
  WIFI: "WiFi",
  WIMAX: "WiMAX",
};

/**
 * @typedef {{
 *   Cellular: CrOnc.CellularType,
 *   ConnectionState: string,
 *   GUID: string,
 *   Name: CrOnc.ManagedStringType,
 *   Type: string,
 *   WiFi: CrOnc.WiFiType,
 *   WiMAX: CrOnc.WiMAXType
 * }}
 */
CrOnc.NetworkConfigType;
