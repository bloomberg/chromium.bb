// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Type aliases for the mojo API.
 */

import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import './mojom/shimless_rma.mojom-lite.js';

/**
 * Return type from state progression methods.
 * Convenience type as mojo-lite does not define types for method results and
 * this is used frequently.
 * @typedef {{
 *   state: !RmaState,
 *   canCancel: boolean,
 *   canGoBack: boolean,
 *   error: !RmadErrorCode
 * }}
 */
export let StateResult;

/**
 * @typedef {ash.shimlessRma.mojom.RmaState}
 */
export const RmaState = ash.shimlessRma.mojom.RmaState;

/**
 * @typedef {ash.shimlessRma.mojom.RmadErrorCode}
 */
export const RmadErrorCode = ash.shimlessRma.mojom.RmadErrorCode;

/**
 * @typedef {ash.shimlessRma.mojom.QrCode}
 */
export const QrCode = ash.shimlessRma.mojom.QrCode;

/**
 * @typedef {ash.shimlessRma.mojom.ComponentType}
 */
export const ComponentType = ash.shimlessRma.mojom.ComponentType;

/**
 * @typedef {ash.shimlessRma.mojom.ComponentRepairStatus}
 */
export const ComponentRepairStatus =
    ash.shimlessRma.mojom.ComponentRepairStatus;

/** @typedef {ash.shimlessRma.mojom.WriteProtectDisableCompleteState} */
export const WriteProtectDisableCompleteState =
    ash.shimlessRma.mojom.WriteProtectDisableCompleteState;

/**
 * @typedef {ash.shimlessRma.mojom.CalibrationSetupInstruction}
 */
export const CalibrationSetupInstruction =
    ash.shimlessRma.mojom.CalibrationSetupInstruction;

/**
 * @typedef {ash.shimlessRma.mojom.CalibrationOverallStatus}
 */
export const CalibrationOverallStatus =
    ash.shimlessRma.mojom.CalibrationOverallStatus;

/**
 * @typedef {ash.shimlessRma.mojom.CalibrationStatus}
 */
export const CalibrationStatus = ash.shimlessRma.mojom.CalibrationStatus;

/**
 * @typedef {ash.shimlessRma.mojom.CalibrationComponentStatus}
 */
export const CalibrationComponentStatus =
    ash.shimlessRma.mojom.CalibrationComponentStatus;

/**
 * @typedef {ash.shimlessRma.mojom.ProvisioningStatus}
 */
export const ProvisioningStatus = ash.shimlessRma.mojom.ProvisioningStatus;

/**
 * @typedef {ash.shimlessRma.mojom.FinalizationStatus}
 */
export const FinalizationStatus = ash.shimlessRma.mojom.FinalizationStatus;

/**
 * Type alias for OsUpdateOperation.
 * @typedef {ash.shimlessRma.mojom.OsUpdateOperation}
 */
export const OsUpdateOperation = ash.shimlessRma.mojom.OsUpdateOperation;

/**
 * @typedef {ash.shimlessRma.mojom.Component}
 */
export const Component = ash.shimlessRma.mojom.Component;

/**
 * Type alias for ErrorObserverRemote.
 * @typedef {ash.shimlessRma.mojom.ErrorObserverRemote}
 */
export const ErrorObserverRemote = ash.shimlessRma.mojom.ErrorObserverRemote;

/**
 * Type alias for OsUpdateObserverRemote.
 * @typedef {ash.shimlessRma.mojom.OsUpdateObserverRemote}
 */
export const OsUpdateObserverRemote =
    ash.shimlessRma.mojom.OsUpdateObserverRemote;

/**
 * Type alias for OsUpdateObserverReceiver.
 * @typedef {ash.shimlessRma.mojom.OsUpdateObserverReceiver}
 */
export const OsUpdateObserverReceiver =
    ash.shimlessRma.mojom.OsUpdateObserverReceiver;

/**
 * Type alias for OsUpdateObserverInterface.
 * @typedef {ash.shimlessRma.mojom.OsUpdateObserverInterface}
 */
export const OsUpdateObserverInterface =
    ash.shimlessRma.mojom.OsUpdateObserverInterface;

/**
 * Type alias for CalibrationObserverRemote.
 * @typedef {ash.shimlessRma.mojom.CalibrationObserverRemote}
 */
export const CalibrationObserverRemote =
    ash.shimlessRma.mojom.CalibrationObserverRemote;

/**
 * Type alias for CalibrationObserverReceiver.
 * @typedef {ash.shimlessRma.mojom.CalibrationObserverReceiver}
 */
export const CalibrationObserverReceiver =
    ash.shimlessRma.mojom.CalibrationObserverReceiver;

/**
 * Type alias for CalibrationObserverInterface.
 * @typedef {ash.shimlessRma.mojom.CalibrationObserverInterface}
 */
export const CalibrationObserverInterface =
    ash.shimlessRma.mojom.CalibrationObserverInterface;

/**
 * Type alias for ProvisioningObserverRemote.
 * @typedef {ash.shimlessRma.mojom.ProvisioningObserverRemote}
 */
export const ProvisioningObserverRemote =
    ash.shimlessRma.mojom.ProvisioningObserverRemote;

/**
 * Type alias for ProvisioningObserverReceiver.
 * @typedef {ash.shimlessRma.mojom.ProvisioningObserverReceiver}
 */
export const ProvisioningObserverReceiver =
    ash.shimlessRma.mojom.ProvisioningObserverReceiver;

/**
 * Type alias for ProvisioningObserverInterface.
 * @typedef {ash.shimlessRma.mojom.ProvisioningObserverInterface}
 */
export const ProvisioningObserverInterface =
    ash.shimlessRma.mojom.ProvisioningObserverInterface;

/**
 * Type alias for HardwareWriteProtectionStateObserverRemote.
 * @typedef {ash.shimlessRma.mojom.HardwareWriteProtectionStateObserverRemote}
 */
export const HardwareWriteProtectionStateObserverRemote =
    ash.shimlessRma.mojom.HardwareWriteProtectionStateObserverRemote;

/**
 * Type alias for HardwareWriteProtectionStateObserverReceiver.
 * @typedef {ash.shimlessRma.mojom.HardwareWriteProtectionStateObserverReceiver}
 */
export const HardwareWriteProtectionStateObserverReceiver =
    ash.shimlessRma.mojom.HardwareWriteProtectionStateObserverReceiver;

/**
 * Type alias for HardwareWriteProtectionStateObserverInterface.
 * @typedef {
 *    ash.shimlessRma.mojom.HardwareWriteProtectionStateObserverInterface
 * }
 */
export const HardwareWriteProtectionStateObserverInterface =
    ash.shimlessRma.mojom.HardwareWriteProtectionStateObserverInterface;

/**
 * Type alias for PowerCableStateObserverRemote.
 * @typedef {ash.shimlessRma.mojom.PowerCableStateObserverRemote}
 */
export const PowerCableStateObserverRemote =
    ash.shimlessRma.mojom.PowerCableStateObserverRemote;

/**
 * Type alias for HardwareVerificationStatusObserverRemote.
 * @typedef {ash.shimlessRma.mojom.HardwareVerificationStatusObserverRemote}
 */
export const HardwareVerificationStatusObserverRemote =
    ash.shimlessRma.mojom.HardwareVerificationStatusObserverRemote;

/**
 * Type alias for HardwareVerificationStatusObserverReceiver.
 * @typedef {ash.shimlessRma.mojom.HardwareVerificationStatusObserverReceiver}
 */
export const HardwareVerificationStatusObserverReceiver =
    ash.shimlessRma.mojom.HardwareVerificationStatusObserverReceiver;

/**
 * Type alias for HardwareVerificationStatusObserverInterface.
 * @typedef {
 *    ash.shimlessRma.mojom.HardwareVerificationStatusObserverInterface
 * }
 */
export const HardwareVerificationStatusObserverInterface =
    ash.shimlessRma.mojom.HardwareVerificationStatusObserverInterface;

/**
 * Type alias for FinalizationObserverRemote.
 * @typedef {ash.shimlessRma.mojom.FinalizationObserverRemote}
 */
export const FinalizationObserverRemote =
    ash.shimlessRma.mojom.FinalizationObserverRemote;

/**
 * Type alias for FinalizationObserverReceiver.
 * @typedef {ash.shimlessRma.mojom.FinalizationObserverReceiver}
 */
export const FinalizationObserverReceiver =
    ash.shimlessRma.mojom.FinalizationObserverReceiver;

/**
 * Type alias for FinalizationObserverInterface.
 * @typedef {
 *    ash.shimlessRma.mojom.FinalizationObserverInterface
 * }
 */
export const FinalizationObserverInterface =
    ash.shimlessRma.mojom.FinalizationObserverInterface;

/**
 * Type alias for the ShimlessRmaService.
 * @typedef {ash.shimlessRma.mojom.ShimlessRmaService}
 */
export const ShimlessRmaService = ash.shimlessRma.mojom.ShimlessRmaService;

/**
 * Type alias for the ShimlessRmaServiceInterface.
 * @typedef {ash.shimlessRma.mojom.ShimlessRmaServiceInterface}
 */
export const ShimlessRmaServiceInterface =
    ash.shimlessRma.mojom.ShimlessRmaServiceInterface;

/**
 * Type alias for NetworkConfigServiceInterface.
 * @typedef {chromeos.networkConfig.mojom.CrosNetworkConfigInterface}
 */
export const NetworkConfigServiceInterface =
    chromeos.networkConfig.mojom.CrosNetworkConfigInterface;

/**
 * Type alias for NetworkConfigServiceRemote.
 * @typedef {chromeos.networkConfig.mojom.CrosNetworkConfigRemote}
 */
export const NetworkConfigServiceRemote =
    chromeos.networkConfig.mojom.CrosNetworkConfigRemote;

/**
 * Type alias for Network
 * @typedef {chromeos.networkConfig.mojom.NetworkStateProperties}
 */
export const Network = chromeos.networkConfig.mojom.NetworkStateProperties;
