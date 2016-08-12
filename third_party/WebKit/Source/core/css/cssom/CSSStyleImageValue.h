// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CSSStyleImageValue_h
#define CSSStyleImageValue_h

#include "core/css/CSSImageValue.h"
#include "core/css/cssom/CSSResourceValue.h"
#include "core/fetch/ImageResource.h"
#include "core/style/StyleImage.h"


namespace blink {

class CORE_EXPORT CSSStyleImageValue : public CSSResourceValue {
    WTF_MAKE_NONCOPYABLE(CSSStyleImageValue);
    DEFINE_WRAPPERTYPEINFO();
public:
    virtual ~CSSStyleImageValue() { }

    StyleValueType type() const override { return ImageType; }

    double intrinsicWidth(bool& isNull);
    double intrinsicHeight(bool& isNull);
    double intrinsicRatio(bool& isNull);

    DEFINE_INLINE_VIRTUAL_TRACE()
    {
        visitor->trace(m_imageValue);
        CSSStyleValue::trace(visitor);
        CSSResourceValue::trace(visitor);
    }

protected:
    CSSStyleImageValue(const CSSImageValue* imageValue)
        : m_imageValue(imageValue)
    {
    }

    Member<const CSSImageValue> m_imageValue;

    virtual LayoutSize imageLayoutSize() const
    {
        DCHECK(!m_imageValue->isCachePending());
        return m_imageValue->cachedImage()->cachedImage()->imageSize(DoNotRespectImageOrientation, 1, ImageResource::IntrinsicSize);
    }

    virtual bool isCachePending() const { return m_imageValue->isCachePending(); }

    Resource::Status status() const override
    {
        if (isCachePending())
            return Resource::Status::NotStarted;
        return m_imageValue->cachedImage()->cachedImage()->getStatus();
    }
};

} // namespace blink

#endif // CSSResourceValue_h
