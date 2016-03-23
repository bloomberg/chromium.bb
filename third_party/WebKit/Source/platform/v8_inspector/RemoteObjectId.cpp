// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/v8_inspector/RemoteObjectId.h"

#include "platform/inspector_protocol/Parser.h"
#include "platform/inspector_protocol/Values.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

RemoteObjectIdBase::RemoteObjectIdBase() : m_injectedScriptId(0) { }

PassOwnPtr<protocol::DictionaryValue> RemoteObjectIdBase::parseInjectedScriptId(const String16& objectId)
{
    OwnPtr<protocol::Value> parsedValue = protocol::parseJSON(objectId);
    if (!parsedValue || parsedValue->type() != protocol::Value::TypeObject)
        return nullptr;

    OwnPtr<protocol::DictionaryValue> parsedObjectId = adoptPtr(protocol::DictionaryValue::cast(parsedValue.leakPtr()));
    bool success = parsedObjectId->getNumber("injectedScriptId", &m_injectedScriptId);
    if (success)
        return parsedObjectId.release();
    return nullptr;
}

RemoteObjectId::RemoteObjectId() : RemoteObjectIdBase(), m_id(0) { }

PassOwnPtr<RemoteObjectId> RemoteObjectId::parse(ErrorString* errorString, const String16& objectId)
{
    OwnPtr<RemoteObjectId> result = adoptPtr(new RemoteObjectId());
    OwnPtr<protocol::DictionaryValue> parsedObjectId = result->parseInjectedScriptId(objectId);
    if (!parsedObjectId) {
        *errorString = "Invalid remote object id";
        return nullptr;
    }

    bool success = parsedObjectId->getNumber("id", &result->m_id);
    if (!success) {
        *errorString = "Invalid remote object id";
        return nullptr;
    }
    return result.release();
}

RemoteCallFrameId::RemoteCallFrameId() : RemoteObjectIdBase(), m_frameOrdinal(0) { }

PassOwnPtr<RemoteCallFrameId> RemoteCallFrameId::parse(ErrorString* errorString, const String16& objectId)
{
    OwnPtr<RemoteCallFrameId> result = adoptPtr(new RemoteCallFrameId());
    OwnPtr<protocol::DictionaryValue> parsedObjectId = result->parseInjectedScriptId(objectId);
    if (!parsedObjectId) {
        *errorString = "Invalid call frame id";
        return nullptr;
    }

    bool success = parsedObjectId->getNumber("ordinal", &result->m_frameOrdinal);
    if (!success) {
        *errorString = "Invalid call frame id";
        return nullptr;
    }

    return result.release();
}

String16 RemoteCallFrameId::serialize(int injectedScriptId, int frameOrdinal)
{
    return "{\"ordinal\":" + String16::number(frameOrdinal) + ",\"injectedScriptId\":" + String16::number(injectedScriptId) + "}";
}

} // namespace blink
