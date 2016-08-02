// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AcceleratedStaticBitmapImage_h
#define AcceleratedStaticBitmapImage_h

#include "platform/graphics/StaticBitmapImage.h"
#include "public/platform/WebExternalTextureMailbox.h"

namespace blink {

class PLATFORM_EXPORT AcceleratedStaticBitmapImage final : public StaticBitmapImage {
public:
    ~AcceleratedStaticBitmapImage() override { };

    IntSize size() const override;
    PassRefPtr<SkImage> imageForCurrentFrame() override;
    static PassRefPtr<AcceleratedStaticBitmapImage> create(PassRefPtr<SkImage>);
    static PassRefPtr<AcceleratedStaticBitmapImage> create(WebExternalTextureMailbox&);
    void copyToTexture(WebGraphicsContext3DProvider*, GLuint, GLenum, GLenum, bool) override;
    bool hasMailbox() override { return m_mailbox.textureSize.width != 0 && m_mailbox.textureSize.height != 0; }

private:
    AcceleratedStaticBitmapImage(PassRefPtr<SkImage>);
    AcceleratedStaticBitmapImage(WebExternalTextureMailbox&);
    bool switchStorageToMailbox(WebGraphicsContext3DProvider*);
    GLuint switchStorageToSkImageForWebGL(WebGraphicsContext3DProvider*);
    GLuint switchStorageToSkImage(WebGraphicsContext3DProvider*);

    WebExternalTextureMailbox m_mailbox;
};

} // namespace blink

#endif
