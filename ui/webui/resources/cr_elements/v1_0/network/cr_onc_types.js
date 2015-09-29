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

/** @typedef {chrome.networkingPrivate.NetworkProperties} */
CrOnc.NetworkProperties;

/** @typedef {string|number|boolean|Object|Array<Object>} */
CrOnc.NetworkPropertyType;

/**
 * TODO(stevenjb): Eliminate this when we switch to using
 * chrome.networkingPrivate.ManagedProperties once defined.
 * @typedef {{
 *   Active: CrOnc.NetworkPropertyType,
 *   Effective: CrOnc.NetworkPropertyType,
 *   UserPolicy: CrOnc.NetworkPropertyType,
 *   DevicePolicy: CrOnc.NetworkPropertyType,
 *   UserSetting: CrOnc.NetworkPropertyType,
 *   SharedSetting: CrOnc.NetworkPropertyType,
 *   UserEditable: boolean,
 *   DeviceEditable: boolean
 * }}
 */
CrOnc.ManagedProperty;

/** @typedef {CrOnc.NetworkPropertyType|!CrOnc.ManagedProperty} */
CrOnc.ManagedNetworkStateProperty;

/** @typedef {chrome.networkingPrivate.SIMLockStatus} */
CrOnc.SIMLockStatus;

/** @typedef {chrome.networkingPrivate.APNProperties} */
CrOnc.APNProperties;

/** @typedef {chrome.networkingPrivate.CellularSimState} */
CrOnc.CellularSimState;

/** @typedef {chrome.networkingPrivate.IPConfigProperties} */
CrOnc.IPConfigProperties;

/** @typedef {chrome.networkingPrivate.ManualProxySettings} */
CrOnc.ManualProxySettings;

/** @typedef {chrome.networkingPrivate.ProxyLocation} */
CrOnc.ProxyLocation;

/** @typedef {chrome.networkingPrivate.ProxySettings} */
CrOnc.ProxySettings;

// Modified version of IPConfigProperties to store RoutingPrefix as a
// human-readable string instead of as a number.
/**
 * @typedef {{
 *   Gateway: (string|undefined),
 *   IPAddress: (string|undefined),
 *   NameServers: (!Array<string>|undefined),
 *   RoutingPrefix: (string|undefined),
 *   Type: (string|undefined),
 *   WebProxyAutoDiscoveryUrl: (string|undefined)
 * }}
 */
CrOnc.IPConfigUIProperties;

/** @typedef {chrome.networkingPrivate.PaymentPortal} */
CrOnc.PaymentPortal;

CrOnc.ActivationState = chrome.networkingPrivate.ActivationStateType;
CrOnc.ConnectionState = chrome.networkingPrivate.ConnectionStateType;
CrOnc.IPConfigType = chrome.networkingPrivate.IPConfigType;
CrOnc.ProxySettingsType = chrome.networkingPrivate.ProxySettingsType;
CrOnc.Type = chrome.networkingPrivate.NetworkType;

/** @enum {string} */
CrOnc.IPType = {
  IPV4: 'IPv4',
  IPV6: 'IPv6',
};

/** @enum {string} */
CrOnc.LockType = {
  NONE: '',
  PIN: 'sim-pin',
  PUK: 'sim-puk',
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

/** @enum {string} */
CrOnc.Source = {
  NONE: 'None',
  DEVICE: 'Device',
  DEVICE_POLICY: 'DevicePolicy',
  USER: 'User',
  USER_POLICY: 'UserPolicy',
};

/**
 * Helper function to retrieve the active ONC property value.
 * @param {!CrOnc.ManagedNetworkStateProperty|undefined} property The property,
 *     which may be a managed dictionary or the property itself.
 * @return {!CrOnc.NetworkPropertyType|undefined} The active property value
 *     if it exists, otherwise undefined.
 */
CrOnc.getActiveValue = function(property) {
  if (property == undefined)
    return undefined;

  // If this is not a dictionary, return the value.
  if (Array.isArray(property) || typeof property != 'object')
    return /** @type {!CrOnc.NetworkPropertyType} */ (property);

  // Otherwise return the Active value if it exists.
  if ('Active' in property)
    return property['Active'];

  // If no Active value is defined, return the effective value.
  if ('Effective' in property) {
    var effective = property.Effective;
    if (effective in property)
      return property[effective];
  }

  console.error('getActiveValue called on invalid ONC object: ' +
                JSON.stringify(property));
  return undefined;
};

/**
 * Calls getActiveValue with '{state.Type}.key', e.g. WiFi.AutoConnect.
 * @param {!CrOnc.NetworkProperties|!CrOnc.NetworkStateProperties|undefined}
 *     properties The ONC network properties or state properties.
 * @param {string} key The type property key, e.g. 'AutoConnect'.
 * @return {!CrOnc.ManagedNetworkStateProperty|undefined} The property value or
 *     dictionary if it exists, otherwise undefined.
 */
CrOnc.getActiveTypeValue = function(properties, key) {
  var typeDict = properties[properties.Type];
  if (!typeDict) {
    // An 'Ethernet' dictionary will only be present for EAP ethernet networks,
    // so don't log an error when Type == Ethernet.
    if (properties.Type != chrome.networkingPrivate.NetworkType.ETHERNET)
      console.error('Network properties missing for:', properties.GUID);
    return undefined;
  }
  return CrOnc.getActiveValue(typeDict[key]);
};

/**
 * Returns an IPConfigProperties object for |type|. For IPV4, these will be the
 * static properties if IPAddressConfigType is Static and StaticIPConfig is set.
 * @param {!CrOnc.NetworkProperties|undefined} properties The ONC properties.
 * @param {!CrOnc.IPType} type The IP Config type.
 * @return {CrOnc.IPConfigProperties|undefined} The IP Config object, or
 *     undefined if no properties for |type| are available.
 */
CrOnc.getIPConfigForType = function(properties, type) {
  'use strict';
  var result;
  var ipConfigs = properties.IPConfigs;
  if (ipConfigs) {
    for (let i = 0; i < ipConfigs.length; ++i) {
      let ipConfig = ipConfigs[i];
      if (ipConfig.Type == type) {
        result = ipConfig;
        break;
      }
    }
  }
  if (type != CrOnc.IPType.IPV4)
    return result;

  var staticIpConfig = properties.StaticIPConfig;
  if (!staticIpConfig)
    return result;

  // If there is no entry in IPConfigs for |type|, return the static config.
  if (!result)
    return staticIpConfig;

  // Otherwise, merge the appropriate static values into the result.
  if (staticIpConfig.IPAddress &&
      CrOnc.getActiveValue(properties.IPAddressConfigType) == 'Static') {
    result.Gateway = staticIpConfig.Gateway;
    result.IPAddress = staticIpConfig.IPAddress;
    result.RoutingPrefix = staticIpConfig.RoutingPrefix;
    result.Type = staticIpConfig.Type;
  }
  if (staticIpConfig.NameServers &&
      CrOnc.getActiveValue(properties.NameServersConfigType) == 'Static') {
    result.NameServers = staticIpConfig.NameServers;
  }
  return result;
};

/**
 * @param {!CrOnc.NetworkProperties|!CrOnc.NetworkStateProperties|undefined}
 *   properties The ONC network properties or state properties.
 * @return {boolean} True if |properties| is a Cellular network with a
 *   locked SIM.
 */
CrOnc.isSimLocked = function(properties) {
  if (!properties.Cellular)
    return false;
  var simLockStatus = properties.Cellular.SIMLockStatus;
  if (simLockStatus == undefined)
    return false;
  return simLockStatus.LockType == CrOnc.LockType.PIN ||
         simLockStatus.LockType == CrOnc.LockType.PUK;
};

/**
 * Modifies |config| to include the correct set of properties for configuring
 * a network IP Address and NameServer configuration for |state|. Existing
 * properties in |config| will be preserved unless invalid.
 * @param {!chrome.networkingPrivate.NetworkConfigProperties} config A partial
 *     ONC configuration.
 * @param {CrOnc.NetworkProperties|undefined} properties The ONC properties.
 */
CrOnc.setValidStaticIPConfig = function(config, properties) {
  if (!config.IPAddressConfigType) {
    var ipConfigType = /** @type {chrome.networkingPrivate.IPConfigType} */(
        CrOnc.getActiveValue(properties.IPAddressConfigType));
    config.IPAddressConfigType = ipConfigType || CrOnc.IPConfigType.DHCP;
  }
  if (!config.NameServersConfigType) {
    var nsConfigType = /** @type {chrome.networkingPrivate.IPConfigType} */(
        CrOnc.getActiveValue(properties.NameServersConfigType));
    config.NameServersConfigType = nsConfigType || CrOnc.IPConfigType.DHCP;
  }
  if (config.IPAddressConfigType != CrOnc.IPConfigType.STATIC &&
      config.NameServersConfigType != CrOnc.IPConfigType.STATIC) {
    if (config.hasOwnProperty('StaticIPConfig'))
      delete config.StaticIPConfig;
    return;
  }

  if (!config.hasOwnProperty('StaticIPConfig')) {
    config.StaticIPConfig =
        /** @type {chrome.networkingPrivate.IPConfigProperties} */({});
  }
  var staticIP = config.StaticIPConfig;
  var stateIPConfig = CrOnc.getIPConfigForType(properties, CrOnc.IPType.IPV4);
  if (config.IPAddressConfigType == 'Static') {
    staticIP.Gateway = staticIP.Gateway || stateIPConfig.Gateway || '';
    staticIP.IPAddress = staticIP.IPAddress || stateIPConfig.IPAddress || '';
    staticIP.RoutingPrefix =
        staticIP.RoutingPrefix || stateIPConfig.RoutingPrefix || 0;
    staticIP.Type = staticIP.Type || stateIPConfig.Type || CrOnc.IPType.IPV4;
  }
  if (config.NameServersConfigType == 'Static') {
    staticIP.NameServers =
        staticIP.NameServers || stateIPConfig.NameServers || [];
  }
};


/**
 * Sets the value of a property in an ONC dictionary.
 * @param {!chrome.networkingPrivate.NetworkConfigProperties} properties
 *     The ONC property dictionary to modify.
 * @param {string} key The property key which may be nested, e.g. 'Foo.Bar'.
 * @param {!CrOnc.NetworkPropertyType} value The property value to set.
 */
CrOnc.setProperty = function(properties, key, value) {
  while (true) {
    var index = key.indexOf('.');
    if (index < 0)
      break;
    var keyComponent = key.substr(0, index);
    if (!properties.hasOwnProperty(keyComponent))
      properties[keyComponent] = {};
    properties = properties[keyComponent];
    key = key.substr(index + 1);
  }
  properties[key] = value;
};

/**
 * Calls setProperty with '{state.Type}.key', e.g. WiFi.AutoConnect.
 * @param {!chrome.networkingPrivate.NetworkConfigProperties} properties The
 *     ONC properties to set. properties.Type must be set already.
 * @param {string} key The type property key, e.g. 'AutoConnect'.
 * @param {!CrOnc.NetworkPropertyType} value The property value to set.
 */
CrOnc.setTypeProperty = function(properties, key, value) {
  if (properties.Type == undefined) {
    console.error('Type not defined in properties: ', properties);
    return;
  }
  var typeKey = properties.Type + '.' + key;
  CrOnc.setProperty(properties, typeKey, value);
};

/**
 * Returns the routing prefix as a string for a given prefix length.
 * @param {number} prefixLength The ONC routing prefix length.
 * @return {string} The corresponding netmask.
 */
CrOnc.getRoutingPrefixAsNetmask = function(prefixLength) {
  'use strict';
  // Return the empty string for invalid inputs.
  if (prefixLength < 0 || prefixLength > 32)
    return '';
  var netmask = '';
  for (let i = 0; i < 4; ++i) {
    let remainder = 8;
    if (prefixLength >= 8) {
      prefixLength -= 8;
    } else {
      remainder = prefixLength;
      prefixLength = 0;
    }
    if (i > 0)
      netmask += '.';
    let value = 0;
    if (remainder != 0)
      value = ((2 << (remainder - 1)) - 1) << (8 - remainder);
    netmask += value.toString();
  }
  return netmask;
};

/**
 * Returns the routing prefix length as a number from the netmask string.
 * @param {string} netmask The netmask string, e.g. 255.255.255.0.
 * @return {number} The corresponding netmask or -1 if invalid.
 */
CrOnc.getRoutingPrefixAsLength = function(netmask) {
  'use strict';
  var prefixLength = 0;
  var tokens = netmask.split('.');
  if (tokens.length != 4)
    return -1;
  for (let i = 0; i < tokens.length; ++i) {
    let token = tokens[i];
    // If we already found the last mask and the current one is not
    // '0' then the netmask is invalid. For example, 255.224.255.0
    if (prefixLength / 8 != i) {
      if (token != '0')
        return -1;
    } else if (token == '255') {
      prefixLength += 8;
    } else if (token == '254') {
      prefixLength += 7;
    } else if (token == '252') {
      prefixLength += 6;
    } else if (token == '248') {
      prefixLength += 5;
    } else if (token == '240') {
      prefixLength += 4;
    } else if (token == '224') {
      prefixLength += 3;
    } else if (token == '192') {
      prefixLength += 2;
    } else if (token == '128') {
      prefixLength += 1;
    } else if (token == '0') {
      prefixLength += 0;
    } else {
      // mask is not a valid number.
      return -1;
    }
  }
  return prefixLength;
};
