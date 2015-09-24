// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebUSBDevice_h
#define WebUSBDevice_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebPassOwnPtr.h"
#include "public/platform/WebVector.h"

namespace blink {

struct WebUSBDeviceInfo;
struct WebUSBError;
struct WebUSBTransferInfo;

using WebUSBDeviceOpenCallbacks = WebCallbacks<void, const WebUSBError&>;
using WebUSBDeviceCloseCallbacks = WebCallbacks<void, const WebUSBError&>;
using WebUSBDeviceGetConfigurationCallbacks = WebCallbacks<uint8_t, const WebUSBError&>;
using WebUSBDeviceSetConfigurationCallbacks = WebCallbacks<void, const WebUSBError&>;
using WebUSBDeviceClaimInterfaceCallbacks = WebCallbacks<void, const WebUSBError&>;
using WebUSBDeviceReleaseInterfaceCallbacks = WebCallbacks<void, const WebUSBError&>;
using WebUSBDeviceResetCallbacks = WebCallbacks<void, const WebUSBError&>;
using WebUSBDeviceSetInterfaceAlternateSettingCallbacks = WebCallbacks<void, const WebUSBError&>;
using WebUSBDeviceClearHaltCallbacks = WebCallbacks<void, const WebUSBError&>;
using WebUSBDeviceTransferCallbacks = WebCallbacks<WebPassOwnPtr<WebUSBTransferInfo>, const WebUSBError&>;

// TODO(rockot): Eliminate these aliases once they're no longer used outside of
// Blink code.
using WebUSBDeviceControlTransferCallbacks = WebUSBDeviceTransferCallbacks;
using WebUSBDeviceBulkTransferCallbacks = WebUSBDeviceTransferCallbacks;
using WebUSBDeviceInterruptTransferCallbacks = WebUSBDeviceTransferCallbacks;

class WebUSBDevice {
public:
    enum class TransferDirection {
        In,
        Out,
    };

    enum class RequestType {
        Standard,
        Class,
        Vendor,
    };

    enum class RequestRecipient {
        Device,
        Interface,
        Endpoint,
        Other,
    };

    struct ControlTransferParameters {
        TransferDirection direction;
        RequestType type;
        RequestRecipient recipient;
        uint8_t request;
        uint16_t value;
        uint16_t index;
    };

    virtual ~WebUSBDevice() { }

    virtual const WebUSBDeviceInfo& info() const = 0;

    // Opens the device.
    // Ownership of the WebUSBDeviceOpenCallbacks is transferred to the client.
    virtual void open(WebUSBDeviceOpenCallbacks*) = 0;

    // Closes the device.
    // Ownership of the WebUSBDeviceCloseCallbacks is transferred to the client.
    virtual void close(WebUSBDeviceCloseCallbacks*) = 0;

    // Gets the active configuration of the device.
    // Ownership of the WebUSBDeviceGetConfigurationCallbacks is transferred to the client.
    virtual void getConfiguration(WebUSBDeviceGetConfigurationCallbacks*) = 0;

    // Sets the active configuration for the device.
    // Ownership of the WebUSBDeviceSetConfigurationCallbacks is transferred to the client.
    virtual void setConfiguration(uint8_t configurationValue, WebUSBDeviceSetConfigurationCallbacks*) = 0;

    // Claims an interface in the active configuration.
    // Ownership of the WebUSBDeviceClaimInterfaceCallbacks is transferred to the client.
    virtual void claimInterface(uint8_t interfaceNumber, WebUSBDeviceClaimInterfaceCallbacks*) = 0;

    // Releases a claimed interface.
    // Ownership of the WebUSBDeviceReleaseInterfaceCallbacks is transferred to the client.
    virtual void releaseInterface(uint8_t interfaceNumber, WebUSBDeviceReleaseInterfaceCallbacks*) = 0;

    // Sets the alternate setting of an interface.
    // Ownership of the WebUSBDeviceSetInterfaceAlternateSettingCallbacks is transferred to the client.
    virtual void setInterface(uint8_t interfaceNumber, uint8_t alternateSetting, WebUSBDeviceSetInterfaceAlternateSettingCallbacks*) = 0;

    // Clears the halt condition on a specific endpoint.
    // Ownership of the WebUSBDeviceClearHaltCallbacks is transferred to the client.
    virtual void clearHalt(uint8_t endpointNumber, WebUSBDeviceClearHaltCallbacks*) = 0;

    // Initiates a control transfer.
    // Ownership of the WebUSBDeviceControlTransferCallbacks is transferred to the client.
    virtual void controlTransfer(const ControlTransferParameters&, uint8_t* data, size_t dataSize, unsigned timeout, WebUSBDeviceControlTransferCallbacks*) = 0;

    // Initiates a bulk or interrupt transfer.
    // Ownership of the WebUSBDeviceBulkTransferCallbacks is transferred to the client.
    virtual void transfer(TransferDirection, uint8_t endpointNumber, uint8_t* data, size_t dataSize, unsigned timeout, WebUSBDeviceBulkTransferCallbacks*) = 0;

    // Resets the device.
    // Ownership of the WebUSBDeviceResetCallbacks is transferred to the client.
    virtual void reset(WebUSBDeviceResetCallbacks*) = 0;
};

} // namespace blink

#endif // WebUSBDevice_h
