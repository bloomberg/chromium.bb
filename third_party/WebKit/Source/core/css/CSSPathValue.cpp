// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/CSSPathValue.h"

#include "core/svg/SVGPathUtilities.h"

namespace blink {

PassRefPtrWillBeRawPtr<CSSPathValue> CSSPathValue::create(const String& pathString)
{
    OwnPtr<SVGPathByteStream> byteStream = SVGPathByteStream::create();
    buildByteStreamFromString(pathString, *byteStream);
    return CSSPathValue::create(byteStream.release());
}

CSSPathValue::CSSPathValue(PassOwnPtr<SVGPathByteStream> pathByteStream)
    : CSSValue(PathClass)
    , m_pathByteStream(pathByteStream)
{
    ASSERT(m_pathByteStream);
    buildPathFromByteStream(*m_pathByteStream, m_path);
}

CSSPathValue* CSSPathValue::emptyPathValue()
{
    DEFINE_STATIC_LOCAL(RefPtrWillBePersistent<CSSPathValue>, empty, (CSSPathValue::create(SVGPathByteStream::create())));
    return empty.get();
}

String CSSPathValue::customCSSText() const
{
    return "path('" + pathString() + "')";
}

bool CSSPathValue::equals(const CSSPathValue& other) const
{
    return *m_pathByteStream == *other.m_pathByteStream;
}

DEFINE_TRACE_AFTER_DISPATCH(CSSPathValue)
{
    CSSValue::traceAfterDispatch(visitor);
}

String CSSPathValue::pathString() const
{
    return buildStringFromByteStream(*m_pathByteStream);
}

} // namespace blink
