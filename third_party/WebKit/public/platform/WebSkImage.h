// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebSkImage_h
#define WebSkImage_h

#include "WebPrivatePtr.h"
#include "third_party/skia/include/core/SkImage.h"

#if INSIDE_BLINK
#include "wtf/Forward.h"
#endif

namespace blink {

// A container for an SkImage
class BLINK_PLATFORM_EXPORT WebSkImage {
public:
    WebSkImage();
    WebSkImage(const WebSkImage&);
    ~WebSkImage();

    bool isNull() const { return m_image.isNull(); }
    int width() const;
    int height() const;
    bool readPixels(const SkImageInfo& dstInfo,
        void* dstPixels,
        size_t dstRowBytes,
        int srcX,
        int srcY) const;

#if INSIDE_BLINK
    WebSkImage(const WTF::PassRefPtr<SkImage>&);
    WebSkImage& operator=(const WTF::PassRefPtr<SkImage>&);
#endif
private:
    WebPrivatePtr<SkImage> m_image;
};

} // namespace blink

#endif // WebSkImage_h
