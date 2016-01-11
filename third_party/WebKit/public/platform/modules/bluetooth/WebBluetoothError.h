// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebBluetoothError_h
#define WebBluetoothError_h

namespace blink {

// Errors that can occur during Web Bluetooth execution, which are transformed
// to a DOMException in Source/modules/bluetooth/BluetoothError.cpp.
//
// These errors all produce constant message strings. If a particular message
// needs a dynamic component, we should add a separate enum so type-checking the IPC
// ensures the dynamic component is passed.
enum class WebBluetoothError {
    // AbortError:
    // InvalidModificationError:
    GATTInvalidAttributeLength,
    // InvalidStateError:
    ServiceNoLongerExists,
    CharacteristicNoLongerExists,
    // NetworkError:
    ConnectAlreadyInProgress,
    ConnectAttributeLengthInvalid,
    ConnectAuthCanceled,
    ConnectAuthFailed,
    ConnectAuthRejected,
    ConnectAuthTimeout,
    ConnectConnectionCongested,
    ConnectInsufficientEncryption,
    ConnectOffsetInvalid,
    ConnectReadNotPermitted,
    ConnectRequestNotSupported,
    ConnectUnknownError,
    ConnectUnknownFailure,
    ConnectUnsupportedDevice,
    ConnectWriteNotPermitted,
    DeviceNoLongerInRange,
    GATTNotPaired,
    GATTOperationInProgress,
    UntranslatedConnectErrorCode,
    // NotFoundError:
    NoBluetoothAdapter,
    ChosenDeviceVanished,
    ChooserCancelled,
    ChooserDeniedPermission,
    ServiceNotFound,
    CharacteristicNotFound,
    // NotSupportedError:
    GATTUnknownError,
    GATTUnknownFailure,
    GATTNotPermitted,
    GATTNotSupported,
    GATTUntranslatedErrorCode,
    // SecurityError:
    GATTNotAuthorized,
    RequestDeviceWithoutFrame,
    // SyntaxError:

    ENUM_MAX_VALUE = GATTNotAuthorized,
};

} // namespace blink

#endif // WebBluetoothError_h
