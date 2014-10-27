// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "public/platform/WebTraceLocation.h"

#include "platform/TraceLocation.h"

namespace blink {

WebTraceLocation::WebTraceLocation(const TraceLocation& location)
    : m_location(location)
{
}

const char* WebTraceLocation::functionName() const
{
    return m_location.functionName();
}

const char* WebTraceLocation::fileName() const
{
    return m_location.fileName();
}

} // namespace blink
