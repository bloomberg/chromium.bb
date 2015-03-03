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

/** @enum {string} */
CrOnc.Type = {
  CELLULAR: 'Cellular',
  ETHERNET: 'Ethernet',
  VPN: 'VPN',
  WIFI: 'WiFi',
  WIMAX: 'WiMAX',
};

/** @enum {string} */
CrOnc.ConnectionState = {
  CONNECTED: 'Connected',
  CONNECTING: 'Connecting',
  NOT_CONNECTED: 'NotConnected',
};

/** @enum {string} */
CrOnc.NetworkTechnology = {
  EDGE: 'EDGE',
  EVDO: 'EVDO',
  GPRS: 'GPRS',
  GSM: 'GSM',
  HSPA: 'HSPA',
  HSPA_PLUS: 'HSPA+',
  LTE: 'LTE',
  LTE_ADVANCED: 'LTE Advanced',
  UMTS: 'UMTS',
  UNKNOWN: 'Unknown',
};

/** @enum {string} */
CrOnc.RoamingState = {
  HOME: 'Home',
  REQUIRED: 'Required',
  ROAMING: 'Roaming',
  UNKNOWN: 'Unknown',
};

/** @enum {string} */
CrOnc.Security = {
  NONE: 'None',
  WEP_8021X: 'WEP-8021X',
  WEP_PSK: 'WEP-PSK',
  WPA_EAP: 'WPA-EAP',
  WPA_PSK: 'WPA-PSK',
};

/** @typedef {string|!Object} */
CrOnc.ManagedStringType;

/**
 * @typedef {{
 *   NetworkTechnology: CrOnc.NetworkTechnology,
 *   RoamingState: CrOnc.RoamingState,
 *   Strength: number
 * }}
 */
CrOnc.CellularType;

/**
 * @typedef {{Security: CrOnc.Security, Strength: number}}
 */
CrOnc.WiFiType;

/**
 * @typedef {{Strength: number}}
 */
CrOnc.WiMAXType;

/**
 * @typedef {{
 *   Cellular: CrOnc.CellularType,
 *   ConnectionState: CrOnc.ConnectionState,
 *   GUID: string,
 *   Name: CrOnc.ManagedStringType,
 *   Type: CrOnc.Type,
 *   WiFi: CrOnc.WiFiType,
 *   WiMAX: CrOnc.WiMAXType
 * }}
 */
CrOnc.NetworkConfigType;
