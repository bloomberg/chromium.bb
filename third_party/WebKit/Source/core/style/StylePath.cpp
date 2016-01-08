// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/style/StylePath.h"

#include "core/css/CSSPathValue.h"
#include "core/svg/SVGPathByteStream.h"
#include "core/svg/SVGPathUtilities.h"

namespace blink {

StylePath::StylePath(PassRefPtr<SVGPathByteStream> pathByteStream)
    : m_byteStream(pathByteStream)
{
    ASSERT(m_byteStream);
    buildPathFromByteStream(*m_byteStream, m_path);
}

StylePath::~StylePath()
{
}

PassRefPtr<StylePath> StylePath::create(PassRefPtr<SVGPathByteStream> pathByteStream)
{
    return adoptRef(new StylePath(pathByteStream));
}

StylePath* StylePath::emptyPath()
{
    DEFINE_STATIC_REF(StylePath, emptyPath, StylePath::create(SVGPathByteStream::create()));
    return emptyPath;
}

const SVGPathByteStream& StylePath::byteStream() const
{
    return *m_byteStream;
}

PassRefPtrWillBeRawPtr<CSSValue> StylePath::computedCSSValue() const
{
    return CSSPathValue::create(m_byteStream, const_cast<StylePath*>(this));
}

bool StylePath::equals(const StylePath& other) const
{
    return *m_byteStream == *other.m_byteStream;
}

} // namespace blink
