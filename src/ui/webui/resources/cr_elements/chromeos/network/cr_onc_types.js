// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file has three parts:
 *
 * 1. A dictionary of strings for network element translations.
 *
 * 2. Typedefs for network properties. Note: These 'types' define a subset of
 * ONC properties in the ONC data dictionary. The first letter is capitalized to
 * match the ONC spec and avoid an extra layer of translation.
 * See components/onc/docs/onc_spec.html for the complete spec.
 * TODO(stevenjb): Replace with chrome.networkingPrivate.NetworkStateProperties
 * once that is fully defined.
 *
 * 3. Helper functions to facilitate extracting and setting ONC properties.
 */

/**
 * Strings required for networking elements. These must be set at runtime.
 * @type {{
 *   OncTypeCellular: string,
 *   OncTypeEthernet: string,
 *   OncTypeTether: string,
 *   OncTypeVPN: string,
 *   OncTypeWiFi: string,
 *   networkListItemConnected: string,
 *   networkListItemConnecting: string,
 *   networkListItemConnectingTo: string,
 *   networkListItemInitializing: string,
 *   networkListItemScanning: string,
 *   networkListItemNotConnected: string,
 *   networkListItemNoNetwork: string,
 *   vpnNameTemplate: string,
 * }}
 */
let CrOncStrings;

const CrOnc = {};

/** @typedef {chrome.networkingPrivate.NetworkStateProperties} */
CrOnc.NetworkStateProperties;

/** @typedef {chrome.networkingPrivate.ManagedProperties} */
CrOnc.NetworkProperties;

/** @typedef {string|number|boolean|Array<string>|Object|Array<Object>} */
CrOnc.NetworkPropertyType;

/**
 * Generic managed property type. This should match any of the basic managed
 * types in chrome.networkingPrivate, e.g. networkingPrivate.ManagedBoolean.
 * @typedef {{
 *   Active: (!CrOnc.NetworkPropertyType|undefined),
 *   Effective: (string|undefined),
 *   UserPolicy: (!CrOnc.NetworkPropertyType|undefined),
 *   DevicePolicy: (!CrOnc.NetworkPropertyType|undefined),
 *   UserSetting: (!CrOnc.NetworkPropertyType|undefined),
 *   SharedSetting: (!CrOnc.NetworkPropertyType|undefined),
 *   UserEditable: (boolean|undefined),
 *   DeviceEditable: (boolean|undefined)
 * }}
 */
CrOnc.ManagedProperty;

/** @typedef {chrome.networkingPrivate.APNProperties} */
CrOnc.APNProperties;

/** @typedef {chrome.networkingPrivate.CellularSimState} */
CrOnc.CellularSimState;

/** @typedef {chrome.networkingPrivate.DeviceStateProperties} */
CrOnc.DeviceStateProperties;

/** @typedef {chrome.networkingPrivate.IPConfigProperties} */
CrOnc.IPConfigProperties;

/** @typedef {chrome.networkingPrivate.ManualProxySettings} */
CrOnc.ManualProxySettings;

/** @typedef {chrome.networkingPrivate.ProxyLocation} */
CrOnc.ProxyLocation;

/** @typedef {chrome.networkingPrivate.ProxySettings} */
CrOnc.ProxySettings;

/** @typedef {chrome.networkingPrivate.PaymentPortal} */
CrOnc.PaymentPortal;

/** @enum {string} */
CrOnc.ActivationState = chrome.networkingPrivate.ActivationStateType;

/** @enum {string} */
CrOnc.ConnectionState = chrome.networkingPrivate.ConnectionStateType;

/** @enum {string} */
CrOnc.DeviceState = chrome.networkingPrivate.DeviceStateType;

/** @enum {string} */
CrOnc.IPConfigType = chrome.networkingPrivate.IPConfigType;

/** @enum {string} */
CrOnc.ProxySettingsType = chrome.networkingPrivate.ProxySettingsType;

/** @enum {string} */
CrOnc.Type = chrome.networkingPrivate.NetworkType;

/** @enum {string} */
CrOnc.Authentication = {
  NONE: 'None',
  WEP_8021X: '8021X',
};

/** @enum {string} */
CrOnc.IPsecAuthenticationType = {
  CERT: 'Cert',
  PSK: 'PSK',
};

/** @enum {string} */
CrOnc.IPType = {
  IPV4: 'IPv4',
  IPV6: 'IPv6',
};

/** @enum {string} */
CrOnc.EAPType = {
  LEAP: 'LEAP',
  PEAP: 'PEAP',
  EAP_TLS: 'EAP-TLS',
  EAP_TTLS: 'EAP-TTLS',
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
  ACTIVE_EXTENSION: 'ActiveExtension',
};

/** @enum {string} */
CrOnc.UserAuthenticationType = {
  NONE: 'None',
  OTP: 'OTP',
  PASSWORD: 'Password',
  PASSWORD_AND_OTP: 'PasswordAndOTP',
};

/** @enum {string} */
CrOnc.VPNType = {
  L2TP_IPSEC: 'L2TP-IPsec',
  OPEN_VPN: 'OpenVPN',
  THIRD_PARTY_VPN: 'ThirdPartyVPN',
  ARCVPN: 'ARCVPN',
};

/**
 * Helper function to retrieve the active ONC property value from a managed
 * dictionary.
 * @param {!CrOnc.ManagedProperty|undefined} property The managed dictionary
 *     for the property if it exists or undefined.
 * @return {!CrOnc.NetworkPropertyType|undefined} The active property value
 *     if it exists, otherwise undefined.
 */
CrOnc.getActiveValue = function(property) {
  if (property == undefined) {
    return undefined;
  }

  if (typeof property != 'object' || Array.isArray(property)) {
    console.error(
        'getActiveValue called on non object: ' + JSON.stringify(property));
    return undefined;
  }

  // Return the Active value if it exists.
  if ('Active' in property) {
    return property['Active'];
  }

  // If no Active value is defined, return the effective value.
  if ('Effective' in property) {
    const effective = property.Effective;
    if (effective in property) {
      return property[effective];
    }
  }

  // If no Effective value, return the UserSetting or SharedSetting.
  if ('UserSetting' in property) {
    return property['UserSetting'];
  }
  if ('SharedSetting' in property) {
    return property['SharedSetting'];
  }

  // Effective, UserEditable or DeviceEditable properties may not have a value
  // set.
  if ('Effective' in property || 'UserEditable' in property ||
      'DeviceEditable' in property) {
    return undefined;
  }

  console.error(
      'getActiveValue called on invalid ONC object: ' +
      JSON.stringify(property));
  return undefined;
};

/**
 * Return the active value for a managed or unmanaged string property.
 * @param {!CrOnc.ManagedProperty|string|undefined} property
 * @return {string}
 */
CrOnc.getStateOrActiveString = function(property) {
  if (property === undefined) {
    return '';
  }
  if (typeof property == 'string') {
    return property;
  }
  return /** @type {string} */ (CrOnc.getActiveValue(property));
};

/**
 * Return if the property is simple, i.e. doesn't contain any nested
 * dictionaries.
 * @param property {!Object|undefined}
 * @return {boolean}
 */
CrOnc.isSimpleProperty = function(property) {
  const requiredProperties = [
    'Active', 'Effective', 'UserSetting', 'SharedSetting', 'UserEditable',
    'DeviceEditable'
  ];
  for (const prop of requiredProperties) {
    if (prop in property) {
      return true;
    }
  }
  return false;
};

/**
 * Converts a managed ONC dictionary into an unmanaged dictionary (i.e. a
 * dictionary of active values).
 * @param {!Object|undefined} properties A managed ONC dictionary
 * @return {!Object|undefined} An unmanaged version of |properties|.
 */
CrOnc.getActiveProperties = function(properties) {
  'use strict';
  if (!properties) {
    return undefined;
  }
  const result = {};
  const keys = Object.keys(properties);
  for (let i = 0; i < keys.length; ++i) {
    const k = keys[i];
    const property = properties[k];
    let propertyValue;
    if (typeof property === 'object') {
      if (CrOnc.isSimpleProperty(property)) {
        propertyValue = CrOnc.getActiveValue(property);
      } else {
        propertyValue = CrOnc.getActiveProperties(property);
      }
    } else {
      propertyValue = property;
    }
    result[k] = propertyValue;
  }
  return result;
};

/**
 * Determines whether the provided properties represent a connecting/connected
 * network.
 * @param {!CrOnc.NetworkProperties|undefined} properties
 * @return {boolean} Whether the properties provided indicate that the network
 *     is connecting or connected.
 */
CrOnc.isConnectingOrConnected = function(properties) {
  if (!properties) {
    return false;
  }

  const connectionState = properties.ConnectionState;
  return connectionState == CrOnc.ConnectionState.CONNECTED ||
      connectionState == CrOnc.ConnectionState.CONNECTING;
};

/**
 * Gets the SignalStrength value from |properties| based on properties.Type.
 * @param {!CrOnc.NetworkProperties|!CrOnc.NetworkStateProperties|undefined}
 *     properties The ONC network properties or state properties.
 * @return {number} The signal strength value if it exists or 0.
 */
CrOnc.getSignalStrength = function(properties) {
  const type = properties.Type;
  if (type == CrOnc.Type.CELLULAR && properties.Cellular) {
    return properties.Cellular.SignalStrength || 0;
  }
  if (type == CrOnc.Type.TETHER && properties.Tether) {
    return properties.Tether.SignalStrength || 0;
  }
  if (type == CrOnc.Type.WI_FI && properties.WiFi) {
    return properties.WiFi.SignalStrength || 0;
  }
  return 0;
};

/**
 * Gets the Managed AutoConnect dictionary from |properties| based on
 * properties.Type.
 * @param {!CrOnc.NetworkProperties|undefined}
 *     properties The ONC network properties or state properties.
 * @return {!chrome.networkingPrivate.ManagedBoolean|undefined} The AutoConnect
 *     managed dictionary or undefined.
 */
CrOnc.getManagedAutoConnect = function(properties) {
  const type = properties.Type;
  if (type == CrOnc.Type.CELLULAR && properties.Cellular) {
    return properties.Cellular.AutoConnect;
  }
  if (type == CrOnc.Type.VPN && properties.VPN) {
    return properties.VPN.AutoConnect;
  }
  if (type == CrOnc.Type.WI_FI && properties.WiFi) {
    return properties.WiFi.AutoConnect;
  }
  return undefined;
};

/**
 * Gets the AutoConnect value from |properties| based on properties.Type.
 * @param {!CrOnc.NetworkProperties|undefined}
 *     properties The ONC network properties properties.
 * @return {boolean} The AutoConnect value if it exists or false.
 */
CrOnc.getAutoConnect = function(properties) {
  const autoconnect = CrOnc.getManagedAutoConnect(properties);
  return !!CrOnc.getActiveValue(autoconnect);
};

/**
 * @param {!CrOnc.NetworkProperties|!CrOnc.NetworkStateProperties|undefined}
 *     properties The ONC network properties or state properties.
 * @return {string} The name to display for |network|.
 */
CrOnc.getNetworkName = function(properties) {
  if (!properties) {
    return '';
  }
  const name = CrOnc.getStateOrActiveString(properties.Name);
  const type = CrOnc.getStateOrActiveString(properties.Type);
  if (!name) {
    assert(CrOncStrings);
    return CrOncStrings['OncType' + type];
  }
  if (type == 'VPN' && properties.VPN) {
    const vpnType = CrOnc.getStateOrActiveString(properties.VPN.Type);
    if (vpnType == 'ThirdPartyVPN' && properties.VPN.ThirdPartyVPN) {
      const providerName = properties.VPN.ThirdPartyVPN.ProviderName;
      if (providerName) {
        return CrOncStrings.vpnNameTemplate.replace('$1', providerName)
            .replace('$2', name);
      }
    }
  }
  return name;
};

/**
 * @param {!CrOnc.NetworkProperties|!CrOnc.NetworkStateProperties|undefined}
 *     properties The ONC network properties or state properties.
 * @return {string} The name to display for |network|.
 */
CrOnc.getEscapedNetworkName = function(properties) {
  return HTMLEscape(CrOnc.getNetworkName(properties));
};

/**
 * @param {!CrOnc.NetworkProperties|!CrOnc.NetworkStateProperties|undefined}
 *   properties The ONC network properties or state properties.
 * @return {boolean} True if |properties| is a Cellular network with a
 *   locked SIM.
 */
CrOnc.isSimLocked = function(properties) {
  if (!properties.Cellular) {
    return false;
  }
  const simLockStatus = properties.Cellular.SIMLockStatus;
  if (simLockStatus == undefined) {
    return false;
  }
  return simLockStatus.LockType == CrOnc.LockType.PIN ||
      simLockStatus.LockType == CrOnc.LockType.PUK;
};

/**
 * Sets the value of a property in an ONC dictionary.
 * @param {!chrome.networkingPrivate.NetworkConfigProperties} properties
 *     The ONC property dictionary to modify.
 * @param {string} key The property key which may be nested, e.g. 'Foo.Bar'.
 * @param {!CrOnc.NetworkPropertyType|undefined} value The property value to
 *     set. If undefined the property will be removed.
 */
CrOnc.setProperty = function(properties, key, value) {
  while (true) {
    const index = key.indexOf('.');
    if (index < 0) {
      break;
    }
    const keyComponent = key.substr(0, index);
    if (!properties.hasOwnProperty(keyComponent)) {
      properties[keyComponent] = {};
    }
    properties = properties[keyComponent];
    key = key.substr(index + 1);
  }
  if (value === undefined) {
    delete properties[key];
  } else {
    properties[key] = value;
  }
};

/**
 * Calls setProperty with '{state.Type}.key', e.g. WiFi.AutoConnect.
 * @param {!chrome.networkingPrivate.NetworkConfigProperties} properties The
 *     ONC properties to set. properties.Type must be set already.
 * @param {string} key The type property key, e.g. 'AutoConnect'.
 * @param {!CrOnc.NetworkPropertyType|undefined} value The property value to
 *     set. If undefined the property will be removed.
 */
CrOnc.setTypeProperty = function(properties, key, value) {
  if (properties.Type == undefined) {
    console.error(
        'Type not defined in properties: ' + JSON.stringify(properties));
    return;
  }
  const typeKey = properties.Type + '.' + key;
  CrOnc.setProperty(properties, typeKey, value);
};

/**
 * Returns a valid CrOnc.Type, or undefined.
 * @param {string} typeStr
 * @return {!CrOnc.Type|undefined}
 */
CrOnc.getValidType = function(typeStr) {
  if (Object.values(CrOnc.Type).indexOf(typeStr) >= 0) {
    return /** @type {!CrOnc.Type} */ (typeStr);
  }
  return undefined;
};

/**
 * Returns whether |properties| has a Cellular or Tether network type.
 * @param {!CrOnc.NetworkProperties|!CrOnc.NetworkStateProperties|undefined}
 *     properties The ONC property dictionary to be checked.
 * @return {boolean}
 */
CrOnc.isMobileNetwork = function(properties) {
  if (!properties) {
    return false;
  }

  const type = properties.Type;
  return type == CrOnc.Type.CELLULAR || type == CrOnc.Type.TETHER;
};
