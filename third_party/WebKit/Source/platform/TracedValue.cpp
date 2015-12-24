// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/TracedValue.h"

#include "base/trace_event/trace_event_argument.h"
#include "wtf/text/StringUTF8Adaptor.h"

namespace blink {

PassRefPtr<TracedValue> TracedValue::create()
{
    return adoptRef(new TracedValue());
}

TracedValue::TracedValue()
    : m_tracedValue(new base::trace_event::TracedValue)
{
}

TracedValue::~TracedValue()
{
}

void TracedValue::setInteger(const char* name, int value)
{
    m_tracedValue->SetIntegerWithCopiedName(name, value);
}

void TracedValue::setDouble(const char* name, double value)
{
    m_tracedValue->SetDoubleWithCopiedName(name, value);
}

void TracedValue::setBoolean(const char* name, bool value)
{
    m_tracedValue->SetBooleanWithCopiedName(name, value);
}

void TracedValue::setString(const char* name, const String& value)
{
    StringUTF8Adaptor adaptor(value);
    m_tracedValue->SetStringWithCopiedName(name, adaptor.asStringPiece());
}

void TracedValue::beginDictionary(const char* name)
{
    m_tracedValue->BeginDictionaryWithCopiedName(name);
}

void TracedValue::beginArray(const char* name)
{
    m_tracedValue->BeginArrayWithCopiedName(name);
}

void TracedValue::endDictionary()
{
    m_tracedValue->EndDictionary();
}

void TracedValue::pushInteger(int value)
{
    m_tracedValue->AppendInteger(value);
}

void TracedValue::pushDouble(double value)
{
    m_tracedValue->AppendDouble(value);
}

void TracedValue::pushBoolean(bool value)
{
    m_tracedValue->AppendBoolean(value);
}

void TracedValue::pushString(const String& value)
{
    StringUTF8Adaptor adaptor(value);
    m_tracedValue->AppendString(adaptor.asStringPiece());
}

void TracedValue::beginArray()
{
    m_tracedValue->BeginArray();
}

void TracedValue::beginDictionary()
{
    m_tracedValue->BeginDictionary();
}

void TracedValue::endArray()
{
    m_tracedValue->EndArray();
}

String TracedValue::asTraceFormat() const
{
    std::string out;
    m_tracedValue->AppendAsTraceFormat(&out);
    return String::fromUTF8(out.c_str());
}

}
