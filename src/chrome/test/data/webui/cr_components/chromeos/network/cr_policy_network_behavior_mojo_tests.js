// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Suite of tests for CrPolicyIndicatorBehavior. */

// clang-format off
// #import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
// #import 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-lite.js';
//
// #import {CrPolicyNetworkBehaviorMojo} from 'chrome://resources/cr_components/chromeos/network/cr_policy_network_behavior_mojo.m.js';
// #import {CrPolicyIndicatorType} from 'chrome://resources/cr_elements/policy/cr_policy_indicator_behavior.m.js';
// #import {Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
// clang-format on

suite('CrPolicyNetworkBehaviorMojo', function() {
  suiteSetup(async () => {
    await PolymerTest.importHtml('chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.html');
    await PolymerTest.importHtml('chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom.html');

    Polymer({
      is: 'test-behavior',

      behaviors: [CrPolicyNetworkBehaviorMojo],
    });
  });

  let mojom;
  let testBehavior;

  setup(function() {
    mojom = chromeos.networkConfig.mojom;
    PolymerTest.clearBody();
    testBehavior = document.createElement('test-behavior');
    document.body.appendChild(testBehavior);
  });

  test('active', function() {
    const property = {
      activeValue: 'foo',
      policySource: mojom.PolicySource.kNone,
    };
    assertFalse(testBehavior.isNetworkPolicyControlled(property));
    assertFalse(testBehavior.isControlled(property));
    assertFalse(testBehavior.isExtensionControlled(property));
    assertTrue(testBehavior.isEditable(property));
    assertFalse(testBehavior.isNetworkPolicyEnforced(property));
    assertFalse(testBehavior.isNetworkPolicyRecommended(property));
  });

  test('user_recommended', function() {
    const property = {
      activeValue: 'foo',
      policySource: mojom.PolicySource.kUserPolicyRecommended,
      policyValue: 'bar',
    };
    assertTrue(testBehavior.isNetworkPolicyControlled(property));
    assertTrue(testBehavior.isControlled(property));
    assertFalse(testBehavior.isExtensionControlled(property));
    assertTrue(testBehavior.isEditable(property));
    assertFalse(testBehavior.isNetworkPolicyEnforced(property));
    assertTrue(testBehavior.isNetworkPolicyRecommended(property));
    assertEquals(
        CrPolicyIndicatorType.USER_POLICY,
        testBehavior.getPolicyIndicatorType(property));
  });

  test('device_recommended', function() {
    const property = {
      activeValue: 'foo',
      policySource: mojom.PolicySource.kDevicePolicyRecommended,
      policyValue: 'bar',
    };
    assertTrue(testBehavior.isNetworkPolicyControlled(property));
    assertTrue(testBehavior.isControlled(property));
    assertFalse(testBehavior.isExtensionControlled(property));
    assertTrue(testBehavior.isEditable(property));
    assertFalse(testBehavior.isNetworkPolicyEnforced(property));
    assertTrue(testBehavior.isNetworkPolicyRecommended(property));
    assertEquals(
        CrPolicyIndicatorType.DEVICE_POLICY,
        testBehavior.getPolicyIndicatorType(property));
  });

  test('user_enforced', function() {
    const property = {
      activeValue: 'foo',
      policySource: mojom.PolicySource.kUserPolicyEnforced,
      policyValue: 'foo',
    };
    assertTrue(testBehavior.isNetworkPolicyControlled(property));
    assertTrue(testBehavior.isControlled(property));
    assertFalse(testBehavior.isExtensionControlled(property));
    assertFalse(testBehavior.isEditable(property));
    assertTrue(testBehavior.isNetworkPolicyEnforced(property));
    assertFalse(testBehavior.isNetworkPolicyRecommended(property));
    assertEquals(
        CrPolicyIndicatorType.USER_POLICY,
        testBehavior.getPolicyIndicatorType(property));
  });

  test('device_enforced', function() {
    const property = {
      activeValue: 'foo',
      policySource: mojom.PolicySource.kDevicePolicyEnforced,
      policyValue: 'foo',
    };
    assertTrue(testBehavior.isNetworkPolicyControlled(property));
    assertTrue(testBehavior.isControlled(property));
    assertFalse(testBehavior.isExtensionControlled(property));
    assertFalse(testBehavior.isEditable(property));
    assertTrue(testBehavior.isNetworkPolicyEnforced(property));
    assertFalse(testBehavior.isNetworkPolicyRecommended(property));
    assertEquals(
        CrPolicyIndicatorType.DEVICE_POLICY,
        testBehavior.getPolicyIndicatorType(property));
  });

  test('extension_controlled', function() {
    const property = {
      activeValue: 'foo',
      policySource: mojom.PolicySource.kActiveExtension,
    };
    assertFalse(testBehavior.isNetworkPolicyControlled(property));
    assertTrue(testBehavior.isControlled(property));
    assertTrue(testBehavior.isExtensionControlled(property));
    assertFalse(testBehavior.isEditable(property));
    assertFalse(testBehavior.isNetworkPolicyEnforced(property));
    assertFalse(testBehavior.isNetworkPolicyRecommended(property));

    assertEquals(
        CrPolicyIndicatorType.EXTENSION,
        testBehavior.getPolicyIndicatorType(property));
  });
});
