// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/bluetooth/BluetoothUUID.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptRegexp.h"
#include "core/dom/ExceptionCode.h"
#include "platform/UUID.h"
#include "wtf/HashMap.h"
#include "wtf/HexNumber.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/WTFString.h"

namespace blink {

namespace {

typedef WTF::HashMap<String, unsigned> NameToAssignedNumberMap;

enum class GATTAttribute {
    Service,
    Characteristic,
    Descriptor
};

NameToAssignedNumberMap* getAssignedNumberToServiceNameMap()
{
    // TODO(ortuno): Add the rest of the names.
    AtomicallyInitializedStaticReference(NameToAssignedNumberMap, servicesMap, []() {
        NameToAssignedNumberMap* services = new NameToAssignedNumberMap();
        services->add("alert_notification", 0x1811);
        return services;
    }());

    return &servicesMap;
}

NameToAssignedNumberMap* getAssignedNumberForCharacteristicNameMap()
{
    // TODO(ortuno): Add the rest of the names.
    AtomicallyInitializedStaticReference(NameToAssignedNumberMap, characteristicsMap, []() {
        NameToAssignedNumberMap* characteristics = new NameToAssignedNumberMap();
        characteristics->add("aerobic_heart_rate_lower_limit", 0x2a7e);
        return characteristics;
    }());

    return &characteristicsMap;
}

NameToAssignedNumberMap* getAssignedNumberForDescriptorNameMap()
{
    // TODO(ortuno): Add the rest of the names.
    AtomicallyInitializedStaticReference(NameToAssignedNumberMap, descriptorsMap, []() {
        NameToAssignedNumberMap* descriptors = new NameToAssignedNumberMap();
        descriptors->add("characteristic_extended_properties", 0x2900);
        return descriptors;
    }());

    return &descriptorsMap;
}

String getUUIDForGATTAttribute(GATTAttribute attribute, StringOrUnsignedLong name, ExceptionState& exceptionState)
{
    // Implementation of BluetoothUUID.getService, BluetoothUUID.getCharacteristic
    // and BluetoothUUID.getDescriptor algorithms:
    // https://webbluetoothcg.github.io/web-bluetooth/#dom-bluetoothuuid-getservice
    // https://webbluetoothcg.github.io/web-bluetooth/#dom-bluetoothuuid-getcharacteristic
    // https://webbluetoothcg.github.io/web-bluetooth/#dom-bluetoothuuid-getdescriptor

    // If name is an unsigned long, return BluetoothUUID.cannonicalUUI(name) and
    // abort this steps.
    if (name.isUnsignedLong())
        return BluetoothUUID::canonicalUUID(name.getAsUnsignedLong());

    String nameStr = name.getAsString();

    // If name is a valid UUID, return name and abort these steps.
    if (isValidUUID(nameStr))
        return nameStr;

    // If name is in the corresponding attribute map return
    // BluetoothUUID.cannonicalUUID(alias).
    NameToAssignedNumberMap* map = nullptr;
    const char* attributeType = nullptr;
    switch (attribute) {
    case GATTAttribute::Service:
        map = getAssignedNumberToServiceNameMap();
        attributeType = "Service";
        break;
    case GATTAttribute::Characteristic:
        map = getAssignedNumberForCharacteristicNameMap();
        attributeType = "Characteristic";
        break;
    case GATTAttribute::Descriptor:
        map = getAssignedNumberForDescriptorNameMap();
        attributeType = "Descriptor";
        break;
    }

    if (map->contains(nameStr))
        return BluetoothUUID::canonicalUUID(map->get(nameStr));

    StringBuilder errorMessage;
    errorMessage.append("Invalid ");
    errorMessage.append(attributeType);
    errorMessage.append(" name: '");
    errorMessage.append(nameStr);
    errorMessage.append("'.");
    // Otherwise, throw a SyntaxError.
    exceptionState.throwDOMException(SyntaxError, errorMessage.toString());
    return String();
}

} // namespace

// static
String BluetoothUUID::getService(StringOrUnsignedLong name, ExceptionState& exceptionState)
{
    return getUUIDForGATTAttribute(GATTAttribute::Service, name, exceptionState);
}

// static
String BluetoothUUID::getCharacteristic(StringOrUnsignedLong name, ExceptionState& exceptionState)
{
    return getUUIDForGATTAttribute(GATTAttribute::Characteristic, name, exceptionState);
}

// static
String BluetoothUUID::getDescriptor(StringOrUnsignedLong name, ExceptionState& exceptionState)
{
    return getUUIDForGATTAttribute(GATTAttribute::Descriptor, name, exceptionState);
}

// static
String BluetoothUUID::canonicalUUID(unsigned alias)
{
    StringBuilder builder;
    builder.reserveCapacity(36 /* 36 chars or 128 bits, length of a UUID */);
    appendUnsignedAsHexFixedSize(
        alias,
        builder, 8 /* 8 chars or 32 bits, prefix length */,
        Lowercase);

    builder.append("-0000-1000-8000-00805f9b34fb");
    return builder.toString();
}

} // namespace blink
