/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Torch Mobile (Beijing) Co. Ltd. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ImageDataBuffer_h
#define ImageDataBuffer_h

#include <memory>
#include "base/memory/scoped_refptr.h"
#include "platform/PlatformExport.h"
#include "platform/geometry/FloatRect.h"
#include "platform/geometry/IntSize.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"
#include "platform/wtf/typed_arrays/Uint8ClampedArray.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace blink {

class ImageDataBuffer {
 public:
  ImageDataBuffer(const IntSize& size, const unsigned char* data)
      : data_(data), uses_pixmap_(false), size_(size) {}
  ImageDataBuffer(const SkPixmap& pixmap)
      : pixmap_(pixmap),
        uses_pixmap_(true),
        size_(IntSize(pixmap.width(), pixmap.height())) {}
  ImageDataBuffer(scoped_refptr<StaticBitmapImage>);

  static std::unique_ptr<ImageDataBuffer> PLATFORM_EXPORT
      Create(scoped_refptr<StaticBitmapImage>);
  String PLATFORM_EXPORT ToDataURL(const String& mime_type,
                                   const double& quality) const;
  bool PLATFORM_EXPORT EncodeImage(const String& mime_type,
                                   const double& quality,
                                   Vector<unsigned char>* encoded_image) const;
  const unsigned char* Pixels() const;
  const IntSize& size() const { return size_; }
  int Height() const { return size_.Height(); }
  int Width() const { return size_.Width(); }

 private:
  const unsigned char* data_;
  SkPixmap pixmap_;
  bool uses_pixmap_ = false;
  IntSize size_;
  scoped_refptr<StaticBitmapImage> image_bitmap_;
};

}  // namespace blink

#endif  // ImageDataBuffer_h
