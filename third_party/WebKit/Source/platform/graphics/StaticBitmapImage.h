// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StaticBitmapImage_h
#define StaticBitmapImage_h

#include "platform/graphics/Image.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace blink {

class WebGraphicsContext3DProvider;

class PLATFORM_EXPORT StaticBitmapImage : public Image {
public:
    ~StaticBitmapImage() override { };

    bool currentFrameIsComplete() override { return true; }

    static PassRefPtr<StaticBitmapImage> create(PassRefPtr<SkImage>);
    void destroyDecodedData() override { }
    bool currentFrameKnownToBeOpaque(MetadataMode = UseCurrentMetadata) override;
    IntSize size() const override;
    void draw(SkCanvas*, const SkPaint&, const FloatRect& dstRect, const FloatRect& srcRect, RespectImageOrientationEnum, ImageClampingMode) override;

    PassRefPtr<SkImage> imageForCurrentFrame() override;

    bool originClean() const { return m_isOriginClean; }
    void setOriginClean(bool flag) { m_isOriginClean = flag; }
    bool isPremultiplied() const { return m_isPremultiplied; }
    void setPremultiplied(bool flag) { m_isPremultiplied = flag; }
    bool isTextureBacked() override;
    virtual void copyToTexture(WebGraphicsContext3DProvider*, GLuint, GLenum, GLenum, bool) { NOTREACHED(); }
    virtual bool hasMailbox() { return false; }

protected:
    StaticBitmapImage(PassRefPtr<SkImage>);
    StaticBitmapImage() { } // empty constructor for derived class.
    RefPtr<SkImage> m_image;

private:
    bool m_isOriginClean = true;
    // The premultiply info is stored here because the SkImage API
    // doesn't expose this info.
    bool m_isPremultiplied = true;
};

} // namespace blink

#endif
