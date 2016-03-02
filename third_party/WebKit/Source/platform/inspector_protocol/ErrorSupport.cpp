// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/inspector_protocol/ErrorSupport.h"

#include "wtf/text/StringBuilder.h"

namespace blink {
namespace protocol {

ErrorSupport::ErrorSupport() : m_errorString(nullptr) { }
ErrorSupport::ErrorSupport(String* errorString) : m_errorString(errorString) { }
ErrorSupport::~ErrorSupport()
{
    if (m_errorString && hasErrors())
        *m_errorString = "Internal error(s): " + errors();
}

void ErrorSupport::setName(const String& name)
{
    ASSERT(m_path.size());
    m_path[m_path.size() - 1] = name;
}

void ErrorSupport::push()
{
    m_path.append(String());
}

void ErrorSupport::pop()
{
    m_path.removeLast();
}

void ErrorSupport::addError(const String& error)
{
    StringBuilder builder;
    for (size_t i = 0; i < m_path.size(); ++i) {
        if (i)
            builder.append(".");
        builder.append(m_path[i]);
    }
    builder.append(": ");
    builder.append(error);
    m_errors.append(builder.toString());
}

bool ErrorSupport::hasErrors()
{
    return m_errors.size();
}

String ErrorSupport::errors()
{
    StringBuilder builder;
    for (size_t i = 0; i < m_errors.size(); ++i) {
        if (i)
            builder.append("; ");
        builder.append(m_errors[i]);
    }
    return builder.toString();
}

} // namespace protocol
} // namespace blink
