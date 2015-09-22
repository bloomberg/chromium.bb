// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef USBConfiguration_h
#define USBConfiguration_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "public/platform/modules/webusb/WebUSBDeviceInfo.h"

namespace blink {

class ExceptionState;
class USBDevice;
class USBInterface;

class USBConfiguration
    : public GarbageCollected<USBConfiguration>
    , public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static USBConfiguration* create(const USBDevice*, size_t configurationIndex);
    static USBConfiguration* create(const USBDevice*, size_t configurationValue, ExceptionState&);
    static USBConfiguration* createFromValue(const USBDevice*, uint8_t configurationValue);

    USBConfiguration(const USBDevice*, size_t configurationIndex);

    const WebUSBDeviceInfo::Configuration& info() const;

    uint8_t configurationValue() const;
    String configurationName() const;
    HeapVector<Member<USBInterface>> interfaces() const;

    DECLARE_TRACE();

private:
    Member<const USBDevice> m_device;
    const size_t m_configurationIndex;
};

} // namespace blink

#endif // USBConfiguration_h
