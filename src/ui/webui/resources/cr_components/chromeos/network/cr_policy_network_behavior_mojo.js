// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior for policy controlled network properties.
 * Note: Many of these methods may be called from HTML, so they support
 * optional properties (which may be null|undefined).
 */

// clang-format off
// #import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
// #import 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-lite.js';
// #import {CrPolicyIndicatorType} from 'chrome://resources/cr_elements/policy/cr_policy_indicator_behavior.m.js';
// #import {OncMojo} from './onc_mojo.m.js';
// clang-format on

/** @polymerBehavior */
/* #export */ const CrPolicyNetworkBehaviorMojo = {
  /**
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {boolean} True if the property is controlled by network policy.
   */
  isNetworkPolicyControlled(property) {
    if (!property) {
      return false;
    }
    const mojom = chromeos.networkConfig.mojom;
    return property.policySource !== mojom.PolicySource.kNone &&
        property.policySource !== mojom.PolicySource.kActiveExtension;
  },

  /**
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {boolean} True if the property is controlled by an extension.
   */
  isExtensionControlled(property) {
    if (!property) {
      return false;
    }
    return property.policySource ===
        chromeos.networkConfig.mojom.PolicySource.kActiveExtension;
  },

  /**
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {boolean} True if the network property is controlled by a network
   *     policy or an extension.
   */
  isControlled(property) {
    if (!property) {
      return false;
    }
    return property.policySource !==
        chromeos.networkConfig.mojom.PolicySource.kNone;
  },

  /**
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {boolean} True if the network property is editable.
   */
  isEditable(property) {
    if (!property) {
      return false;
    }
    const mojom = chromeos.networkConfig.mojom;
    return property.policySource !== mojom.PolicySource.kUserPolicyEnforced &&
        property.policySource !== mojom.PolicySource.kDevicePolicyEnforced &&
        property.policySource !== mojom.PolicySource.kActiveExtension;
  },

  /**
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {boolean} True if the network property is enforced by a policy.
   */
  isNetworkPolicyEnforced(property) {
    if (!property) {
      return false;
    }
    const mojom = chromeos.networkConfig.mojom;
    return property.policySource === mojom.PolicySource.kUserPolicyEnforced ||
        property.policySource === mojom.PolicySource.kDevicePolicyEnforced;
  },

  /**
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {boolean} True if the network property is recommended by a policy.
   */
  isNetworkPolicyRecommended(property) {
    if (!property) {
      return false;
    }
    const mojom = chromeos.networkConfig.mojom;
    return property.policySource ===
        mojom.PolicySource.kUserPolicyRecommended ||
        property.policySource === mojom.PolicySource.kDevicePolicyRecommended;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedBoolean|
   *         !chromeos.networkConfig.mojom.ManagedInt32|
   *         !chromeos.networkConfig.mojom.ManagedString|
   *         !chromeos.networkConfig.mojom.ManagedStringList|
   *         !chromeos.networkConfig.mojom.ManagedApnList} property
   * @return {boolean|number|string|!Array<string>|
   *          !Array<!chromeos.networkConfig.mojom.ApnProperties>|null}
   *         |property.policyValue| if the property is policy-enforced or null
   *         otherwise.
   */
  getEnforcedPolicyValue(property) {
    if (!property || !this.isNetworkPolicyEnforced(property)) {
      return null;
    }
    return property.policyValue === undefined ? null : property.policyValue;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedBoolean|
   *         !chromeos.networkConfig.mojom.ManagedInt32|
   *         !chromeos.networkConfig.mojom.ManagedString|
   *         !chromeos.networkConfig.mojom.ManagedStringList|
   *         !chromeos.networkConfig.mojom.ManagedApnList} property
   * @return {boolean|number|string|!Array<string>|
   *          !Array<!chromeos.networkConfig.mojom.ApnProperties>|null}
   *         |property.policyValue| if the property is policy-recommended or
   *         null otherwise.
   */
  getRecommendedPolicyValue(property) {
    if (!property || !this.isNetworkPolicyRecommended(property)) {
      return null;
    }
    return property.policyValue === undefined ? null : property.policyValue;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.OncSource} source
   * @return {boolean}
   * @protected
   */
  isPolicySource(source) {
    return source === chromeos.networkConfig.mojom.OncSource.kDevicePolicy ||
        source === chromeos.networkConfig.mojom.OncSource.kUserPolicy;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.OncSource} source
   * @return {!CrPolicyIndicatorType}
   * @protected
   */
  getIndicatorTypeForSource(source) {
    if (source === chromeos.networkConfig.mojom.OncSource.kDevicePolicy) {
      return CrPolicyIndicatorType.DEVICE_POLICY;
    }
    if (source === chromeos.networkConfig.mojom.OncSource.kUserPolicy) {
      return CrPolicyIndicatorType.USER_POLICY;
    }
    return CrPolicyIndicatorType.NONE;
  },

  /**
   * Get policy indicator type for the setting at |path|.
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {CrPolicyIndicatorType}
   */
  getPolicyIndicatorType(property) {
    if (!property) {
      return CrPolicyIndicatorType.NONE;
    }
    const mojom = chromeos.networkConfig.mojom;
    if (property.policySource === mojom.PolicySource.kUserPolicyEnforced ||
        property.policySource === mojom.PolicySource.kUserPolicyRecommended) {
      return CrPolicyIndicatorType.USER_POLICY;
    }
    if (property.policySource === mojom.PolicySource.kDevicePolicyEnforced ||
        property.policySource === mojom.PolicySource.kDevicePolicyRecommended) {
      return CrPolicyIndicatorType.DEVICE_POLICY;
    }
    if (property.policySource === mojom.PolicySource.kActiveExtension) {
      return CrPolicyIndicatorType.EXTENSION;
    }
    return CrPolicyIndicatorType.NONE;
  },
};

/** @interface */
/* #export */ class CrPolicyNetworkBehaviorMojoInterface {
  /**
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {boolean}
   */
  isNetworkPolicyControlled(property) {}

  /**
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {boolean}
   */
  isExtensionControlled(property) {}

  /**
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {boolean}
   */
  isControlled(property) {}

  /**
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {boolean}
   */
  isEditable(property) {}

  /**
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {boolean}
   */
  isNetworkPolicyEnforced(property) {}

  /**
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {boolean}
   */
  isNetworkPolicyRecommended(property) {}

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedBoolean|
   *         !chromeos.networkConfig.mojom.ManagedInt32|
   *         !chromeos.networkConfig.mojom.ManagedString|
   *         !chromeos.networkConfig.mojom.ManagedStringList|
   *         !chromeos.networkConfig.mojom.ManagedApnList} property
   * @return {boolean|number|string|!Array<string>|
   *          !Array<!chromeos.networkConfig.mojom.ApnProperties>|null}
   */
  getEnforcedPolicyValue(property) {}

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedBoolean|
   *         !chromeos.networkConfig.mojom.ManagedInt32|
   *         !chromeos.networkConfig.mojom.ManagedString|
   *         !chromeos.networkConfig.mojom.ManagedStringList|
   *         !chromeos.networkConfig.mojom.ManagedApnList} property
   * @return {boolean|number|string|!Array<string>|
   *          !Array<!chromeos.networkConfig.mojom.ApnProperties>|null}
   */
  getRecommendedPolicyValue(property) {}

  /**
   * @param {!chromeos.networkConfig.mojom.OncSource} source
   * @return {boolean}
   * @protected
   */
  isPolicySource(source) {}

  /**
   * @param {!chromeos.networkConfig.mojom.OncSource} source
   * @return {!CrPolicyIndicatorType}
   * @protected
   */
  getIndicatorTypeForSource(source) {}

  /**
   * @param {?OncMojo.ManagedProperty|undefined} property
   * @return {CrPolicyIndicatorType}
   */
  getPolicyIndicatorType(property) {}
}
