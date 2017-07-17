/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef ImageSource_h
#define ImageSource_h

#include <memory>
#include "platform/PlatformExport.h"
#include "platform/graphics/ColorBehavior.h"
#include "platform/graphics/DeferredImageDecoder.h"
#include "platform/graphics/ImageOrientation.h"
#include "platform/wtf/Forward.h"
#include "platform/wtf/Noncopyable.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkImage;

namespace blink {

class ImageOrientation;
class IntPoint;
class IntSize;
class SharedBuffer;

class PLATFORM_EXPORT ImageSource final {
  USING_FAST_MALLOC(ImageSource);
  WTF_MAKE_NONCOPYABLE(ImageSource);

 public:
  ImageSource();
  ~ImageSource();

  // Tells the ImageSource that the Image no longer cares about decoded frame
  // data except for the specified frame. Callers may pass WTF::kNotFound to
  // clear all frames.
  //
  // In response, the ImageSource should delete cached decoded data for other
  // frames where possible to keep memory use low. The expectation is that in
  // the future, the caller may call createFrameAtIndex() with an index larger
  // than the one passed to this function, and the implementation may then
  // make use of the preserved frame data here in decoding that frame.
  // By contrast, callers who call this function and then later ask for an
  // earlier frame may require more work to be done, e.g. redecoding the image
  // from the beginning.
  //
  // Implementations may elect to preserve more frames than the one requested
  // here if doing so is likely to save CPU time in the future, but will pay
  // an increased memory cost to do so.
  //
  // Returns the number of bytes of frame data actually cleared.
  size_t ClearCacheExceptFrame(size_t);

  PassRefPtr<SharedBuffer> Data();
  // Returns false when the decoder layer rejects the data.
  bool SetData(RefPtr<SharedBuffer> data, bool all_data_received);
  String FilenameExtension() const;

  bool IsSizeAvailable();
  bool HasColorProfile() const;
  IntSize size(
      RespectImageOrientationEnum = kDoNotRespectImageOrientation) const;
  IntSize FrameSizeAtIndex(
      size_t,
      RespectImageOrientationEnum = kDoNotRespectImageOrientation) const;

  bool GetHotSpot(IntPoint&) const;
  int RepetitionCount();

  size_t FrameCount() const;

  // Attempts to create the requested frame.
  sk_sp<SkImage> CreateFrameAtIndex(size_t);

  float FrameDurationAtIndex(size_t) const;
  bool FrameHasAlphaAtIndex(
      size_t) const;  // Whether or not the frame actually used any alpha.
  bool FrameIsReceivedAtIndex(
      size_t) const;  // Whether or not the frame is fully received.
  ImageOrientation OrientationAtIndex(size_t) const;  // EXIF image orientation

  // Returns the number of bytes in the decoded frame. May return 0 if the
  // frame has not yet begun to decode.
  size_t FrameBytesAtIndex(size_t) const;

 private:
  std::unique_ptr<DeferredImageDecoder> decoder_;
  bool all_data_received_ = false;
};

}  // namespace blink

#endif
