// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AcceleratedStaticBitmapImage_h
#define AcceleratedStaticBitmapImage_h

#include "gpu/command_buffer/common/mailbox.h"
#include "gpu/command_buffer/common/sync_token.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "third_party/skia/include/core/SkRefCnt.h"

#include <memory>

class GrContext;

namespace cc {
class SingleReleaseCallback;
}

namespace blink {

class PLATFORM_EXPORT AcceleratedStaticBitmapImage final : public StaticBitmapImage {
public:
    // SkImage with a texture backing that is assumed to be from the shared main thread context.
    static PassRefPtr<AcceleratedStaticBitmapImage> create(sk_sp<SkImage>);
    // Can specify the GrContext that created the texture backing the for the given SkImage. Ideally all callers would use this option.
    // The |mailbox| is a name for the texture backing the SkImage, allowing other contexts to use the same backing.
    static PassRefPtr<AcceleratedStaticBitmapImage> create(sk_sp<SkImage>, sk_sp<GrContext>, const gpu::Mailbox&, const gpu::SyncToken&);

    ~AcceleratedStaticBitmapImage() override;

    // StaticBitmapImage overrides.
    sk_sp<SkImage> imageForCurrentFrame() override;
    void copyToTexture(WebGraphicsContext3DProvider*, GLuint destTextureId, GLenum destInternalFormat, GLenum destType, bool flipY) override;

private:
    AcceleratedStaticBitmapImage(sk_sp<SkImage>);
    AcceleratedStaticBitmapImage(sk_sp<SkImage>, sk_sp<GrContext>, const gpu::Mailbox&, const gpu::SyncToken&);

    bool switchStorageToMailbox(WebGraphicsContext3DProvider*);
    GLuint switchStorageToSkImageForWebGL(WebGraphicsContext3DProvider*);
    GLuint switchStorageToSkImage(WebGraphicsContext3DProvider*);

    void ensureMailbox();

    // True when the |m_image| has a texture id backing it from the shared main thread context.
    bool m_imageIsForSharedMainThreadContext;

    // Keeps the context alive that the SkImage is associated with. Not used if the SkImage is coming from the shared main
    // thread context as we assume that context is kept alive elsewhere since it is globally shared in the process.
    sk_sp<GrContext> m_grContext;
    // True when the below mailbox and sync token are valid for getting at the texture backing the object's SkImage.
    bool m_hasMailbox = false;
    // A mailbox referring to the texture id backing the SkImage. The mailbox is valid as long as the SkImage is held alive.
    gpu::Mailbox m_mailbox;
    // This token must be waited for before using the mailbox.
    gpu::SyncToken m_syncToken;
};

} // namespace blink

#endif
