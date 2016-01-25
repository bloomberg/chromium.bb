// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/CSSPathValue.h"

#include "core/style/StylePath.h"
#include "core/svg/SVGPathUtilities.h"

namespace blink {

PassRefPtrWillBeRawPtr<CSSPathValue> CSSPathValue::create(PassRefPtr<SVGPathByteStream> pathByteStream, StylePath* cachedPath)
{
    return adoptRefWillBeNoop(new CSSPathValue(pathByteStream, cachedPath));
}

PassRefPtrWillBeRawPtr<CSSPathValue> CSSPathValue::create(const String& pathString)
{
    RefPtr<SVGPathByteStream> byteStream = SVGPathByteStream::create();
    buildByteStreamFromString(pathString, *byteStream);
    return CSSPathValue::create(byteStream.release());
}

CSSPathValue::CSSPathValue(PassRefPtr<SVGPathByteStream> pathByteStream, StylePath* cachedPath)
    : CSSValue(PathClass)
    , m_pathByteStream(pathByteStream)
    , m_cachedPath(cachedPath)
{
    ASSERT(m_pathByteStream);
}

CSSPathValue::~CSSPathValue()
{
}

namespace {

PassRefPtrWillBeRawPtr<CSSPathValue> createPathValue()
{
    RefPtr<SVGPathByteStream> pathByteStream = SVGPathByteStream::create();
    // Need to be registered as LSan ignored, as it will be reachable and
    // separately referred to by emptyPathValue() callers.
    LEAK_SANITIZER_IGNORE_OBJECT(pathByteStream.get());
    return CSSPathValue::create(pathByteStream.release());
}

} // namespace

CSSPathValue* CSSPathValue::emptyPathValue()
{
    DEFINE_STATIC_LOCAL(RefPtrWillBePersistent<CSSPathValue>, empty, (createPathValue()));
    return empty.get();
}

StylePath* CSSPathValue::cachedPath()
{
    if (!m_cachedPath)
        m_cachedPath = StylePath::create(m_pathByteStream);
    return m_cachedPath.get();
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
