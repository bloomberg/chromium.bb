// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file has two parts:
 *
 * 1. Typedefs for network properties. Note: These 'types' define a subset of
 * ONC properties in the ONC data dictionary. The first letter is capitalized to
 * match the ONC spec and avoid an extra layer of translation.
 * See components/onc/docs/onc_spec.html for the complete spec.
 * TODO(stevenjb): Replace with chrome.networkingPrivate.NetworkStateProperties
 * once that is fully defined.
 *
 * 2. Helper functions to facilitate extracting and setting ONC properties.
 */

var CrOnc = {};

/** @typedef {chrome.networkingPrivate.NetworkStateProperties} */
CrOnc.NetworkStateProperties;

/** @typedef {string|number|boolean|Array<Object>} */
CrOnc.NetworkStateProperty;

/** @typedef {{
 *    Active: CrOnc.NetworkStateProperty,
 *    Effective: CrOnc.NetworkStateProperty,
 *    UserPolicy: CrOnc.NetworkStateProperty,
 *    DevicePolicy: CrOnc.NetworkStateProperty,
 *    UserSetting: CrOnc.NetworkStateProperty,
 *    SharedSetting: CrOnc.NetworkStateProperty,
 *    UserEditable: boolean,
 *    DeviceEditable: boolean
 * }}
 */
CrOnc.ManagedProperty;

/** @typedef {CrOnc.NetworkStateProperty|!CrOnc.ManagedProperty} */
CrOnc.ManagedNetworkStateProperty;

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
  CDMA1XRTT: 'CDMA1XRTT',
  EDGE: 'EDGE',
  EVDO: 'EVDO',
  GPRS: 'GPRS',
  GSM: 'GSM',
  HSPA: 'HSPA',
  HSPA_PLUS: 'HSPAPlus',
  LTE: 'LTE',
  LTE_ADVANCED: 'LTEAdvanced',
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

/**
 * Helper function to retrieve the active ONC property value.
 * @param {!CrOnc.ManagedNetworkStateProperty} property The property, which may
 *     be a managed dictionary or the property itself.
 * @return {!CrOnc.NetworkStateProperty|undefined} The active property value
 *     if it exists, otherwise undefined.
 */
CrOnc.getActiveValue = function(property) {
  // If this is not a dictionary, return the value.
  if (Array.isArray(property) || typeof property != 'object')
    return /** @type {!CrOnc.NetworkStateProperty} */ (property);

  // Otherwise return the Active value if it exists.
  if ('Active' in property)
    return property['Active'];

  // If no Active value is defined, return the effective value.
  if ('Effective' in property) {
    var effective = property.Effective;
    if (effective in property)
      return property[effective];
  }

  console.error('getActiveValue called on invalid ONC object: ' + property);
  return undefined;
};

/**
 * Returns either a managed property dictionary or an unmanaged value associated
 * with a key.
 * @param {!CrOnc.NetworkStateProperties} state The ONC network state.
 * @param {string} key The property key which may be nested, e.g. 'Foo.Bar'.
 * @return {!CrOnc.ManagedNetworkStateProperty|undefined} The property value or
 *   dictionary if it exists, otherwise undefined.
 */
CrOnc.getProperty = function(state, key) {
  while (true) {
    var index = key.indexOf('.');
    if (index < 0)
      break;
    var keyComponent = key.substr(0, index);
    if (!state.hasOwnProperty(keyComponent))
      return undefined;
    state = state[keyComponent];
    key = key.substr(index + 1);
  }
  return state[key];
};

/**
 * Calls getProperty with '{state.Type}.key', e.g. WiFi.AutoConnect.
 * @param {!CrOnc.NetworkStateProperties} state The ONC network state.
 * @param {string} key The type property key, e.g. 'AutoConnect'.
 * @return {!CrOnc.ManagedNetworkStateProperty|undefined} The property value or
 *     dictionary if it exists, otherwise undefined.
 */
CrOnc.getTypeProperty = function(state, key) {
  var typeKey = state.Type + '.' + key;
  return CrOnc.getProperty(state, typeKey);
};

/**
 * Sets the value of a property in an ONC dictionary.
 * @param {!CrOnc.NetworkStateProperties} state The ONC network state to modify.
 * @param {string} key The property key which may be nested, e.g. 'Foo.Bar'.
 * @param {!CrOnc.NetworkStateProperty} value The property value to set.
 */
CrOnc.setProperty = function(state, key, value) {
  while (true) {
    var index = key.indexOf('.');
    if (index < 0)
      break;
    var keyComponent = key.substr(0, index);
    if (!state.hasOwnProperty(keyComponent))
      state[keyComponent] = {};
    state = state[keyComponent];
    key = key.substr(index + 1);
  }
  state[key] = value;
};

/**
 * Calls setProperty with '{state.Type}.key', e.g. WiFi.AutoConnect.
 * @param {!CrOnc.NetworkStateProperties} state The ONC network state.
 * @param {string} key The type property key, e.g. 'AutoConnect'.
 * @param {!CrOnc.NetworkStateProperty} value The property value to set.
 */
CrOnc.setTypeProperty = function(state, key, value) {
  var typeKey = state.Type + '.' + key;
  CrOnc.setProperty(state, typeKey, value);
};
