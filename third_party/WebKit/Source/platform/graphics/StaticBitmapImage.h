// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StaticBitmapImage_h
#define StaticBitmapImage_h

#include "platform/graphics/Image.h"

namespace blink {

class PLATFORM_EXPORT StaticBitmapImage final : public Image {
public:
    ~StaticBitmapImage() override;

    bool currentFrameIsComplete() override { return true; }

    static PassRefPtr<StaticBitmapImage> create(PassRefPtr<SkImage>);
    virtual void destroyDecodedData(bool destroyAll) { }
    virtual bool currentFrameKnownToBeOpaque(MetadataMode = UseCurrentMetadata);
    virtual IntSize size() const;
    void draw(SkCanvas*, const SkPaint&, const FloatRect& dstRect, const FloatRect& srcRect, RespectImageOrientationEnum, ImageClampingMode) override;

    PassRefPtr<SkImage> imageForCurrentFrame() override;

    bool originClean() const { return m_isOriginClean; }
    void setOriginClean(bool flag) { m_isOriginClean = flag; }
protected:
    StaticBitmapImage(PassRefPtr<SkImage>);

    RefPtr<SkImage> m_image;
    bool m_isOriginClean = true;
};

} // namespace blink

#endif
