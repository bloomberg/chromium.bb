// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StaticBitmapImage_h
#define StaticBitmapImage_h

#include "platform/graphics/Image.h"

namespace blink {

class PLATFORM_EXPORT StaticBitmapImage : public Image {
public:
    virtual ~StaticBitmapImage();

    static PassRefPtr<Image> create(PassRefPtr<SkImage>);
    virtual void destroyDecodedData(bool destroyAll) { }
    virtual bool currentFrameKnownToBeOpaque();
    virtual IntSize size() const;
    virtual void draw(GraphicsContext*, const FloatRect& dstRect, const FloatRect& srcRect, CompositeOperator, blink::WebBlendMode);


    PassRefPtr<SkImage> skImage() OVERRIDE { return m_image; }

protected:
    StaticBitmapImage(PassRefPtr<SkImage>);

    RefPtr<SkImage> m_image;
};

} // namespace

#endif
