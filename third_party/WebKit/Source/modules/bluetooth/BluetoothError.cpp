// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/bluetooth/BluetoothError.h"

#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"

namespace blink {

DOMException* BluetoothError::take(ScriptPromiseResolver*, const WebBluetoothError& webError)
{
    switch (webError) {
#define MAP_ERROR(enumeration, name, message) \
    case WebBluetoothError::enumeration:      \
        return DOMException::create(name, message)

        // InvalidModificationErrors:
        MAP_ERROR(GATTInvalidAttributeLength, InvalidModificationError, "GATT Error: invalid attribute length.");

        // InvalidStateErrors:
        MAP_ERROR(ServiceNoLongerExists, InvalidStateError, "GATT Service no longer exists.");
        MAP_ERROR(CharacteristicNoLongerExists, InvalidStateError, "GATT Characteristic no longer exists.");

        // NetworkErrors:
        MAP_ERROR(ConnectAlreadyInProgress, NetworkError, "Connection already in progress.");
        MAP_ERROR(ConnectAttributeLengthInvalid, NetworkError, "Write operation exceeds the maximum length of the attribute.");
        MAP_ERROR(ConnectAuthCanceled, NetworkError, "Authentication canceled.");
        MAP_ERROR(ConnectAuthFailed, NetworkError, "Authentication failed.");
        MAP_ERROR(ConnectAuthRejected, NetworkError, "Authentication rejected.");
        MAP_ERROR(ConnectAuthTimeout, NetworkError, "Authentication timeout.");
        MAP_ERROR(ConnectConnectionCongested, NetworkError, "Remote device connection is congested.");
        MAP_ERROR(ConnectInsufficientEncryption, NetworkError, "Insufficient encryption for a given operation");
        MAP_ERROR(ConnectOffsetInvalid, NetworkError, "Read or write operation was requested with an invalid offset.");
        MAP_ERROR(ConnectReadNotPermitted, NetworkError, "GATT read operation is not permitted.");
        MAP_ERROR(ConnectRequestNotSupported, NetworkError, "The given request is not supported.");
        MAP_ERROR(ConnectUnknownError, NetworkError, "Unknown error when connecting to the device.");
        MAP_ERROR(ConnectUnknownFailure, NetworkError, "Connection failed for unknown reason.");
        MAP_ERROR(ConnectUnsupportedDevice, NetworkError, "Unsupported device.");
        MAP_ERROR(ConnectWriteNotPermitted, NetworkError, "GATT write operation is not permitted.");
        MAP_ERROR(DeviceNoLongerInRange, NetworkError, "Bluetooth Device is no longer in range.");
        MAP_ERROR(GATTNotPaired, NetworkError, "GATT Error: Not paired.");
        MAP_ERROR(GATTOperationInProgress, NetworkError, "GATT operation already in progress.");
        MAP_ERROR(UntranslatedConnectErrorCode, NetworkError, "Unknown ConnectErrorCode.");

        // NotFoundErrors:
        MAP_ERROR(NoBluetoothAdapter, NotFoundError, "Bluetooth adapter not available.");
        MAP_ERROR(ChosenDeviceVanished, NotFoundError, "User selected a device that doesn't exist anymore.");
        MAP_ERROR(ChooserCancelled, NotFoundError, "User cancelled the requestDevice() chooser.");
        MAP_ERROR(ChooserDeniedPermission, NotFoundError, "User denied the browser permission to scan for Bluetooth devices.");
        MAP_ERROR(ServiceNotFound, NotFoundError, "Service not found in device.");
        MAP_ERROR(CharacteristicNotFound, NotFoundError, "Characteristic not found in device.");

        // NotSupportedErrors:
        MAP_ERROR(GATTUnknownError, NotSupportedError, "GATT Error Unknown.");
        MAP_ERROR(GATTUnknownFailure, NotSupportedError, "GATT operation failed for unknown reason.");
        MAP_ERROR(GATTNotPermitted, NotSupportedError, "GATT operation not permitted.");
        MAP_ERROR(GATTNotSupported, NotSupportedError, "GATT Error: Not supported.");
        MAP_ERROR(GATTUntranslatedErrorCode, NotSupportedError, "GATT Error: Unknown GattErrorCode.");

        // SecurityErrors:
        MAP_ERROR(GATTNotAuthorized, SecurityError, "GATT operation not authorized.");
        MAP_ERROR(RequestDeviceWithUniqueOrigin, SecurityError, "requestDevice() called from sandboxed or otherwise unique origin.");
        MAP_ERROR(RequestDeviceWithoutFrame, SecurityError, "No window to show the requestDevice() dialog.");

#undef MAP_ERROR
    }

    ASSERT_NOT_REACHED();
    return DOMException::create(UnknownError);
}

} // namespace blink
