// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PlatformInstrumentation_h
#define PlatformInstrumentation_h

#include "platform/PlatformExport.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class PLATFORM_EXPORT PlatformInstrumentation {
 public:
  class LazyPixelRefTracker : TraceEvent::TraceScopedTrackableObject<void*> {
   public:
    LazyPixelRefTracker(void* instance)
        : TraceEvent::TraceScopedTrackableObject<void*>(kCategoryName,
                                                        kLazyPixelRef,
                                                        instance) {}
  };

  static const char kImageDecodeEvent[];
  static const char kImageResizeEvent[];
  static const char kDrawLazyPixelRefEvent[];
  static const char kDecodeLazyPixelRefEvent[];

  static const char kImageTypeArgument[];
  static const char kCachedArgument[];

  static const char kLazyPixelRef[];

  static void WillDecodeImage(const String& image_type);
  static void DidDecodeImage();
  static void DidDrawLazyPixelRef(unsigned long long lazy_pixel_ref_id);
  static void WillDecodeLazyPixelRef(unsigned long long lazy_pixel_ref_id);
  static void DidDecodeLazyPixelRef();

 private:
  static const char kCategoryName[];
};

inline void PlatformInstrumentation::WillDecodeImage(const String& image_type) {
  TRACE_EVENT_BEGIN1(kCategoryName, kImageDecodeEvent, kImageTypeArgument,
                     image_type.Ascii());
}

inline void PlatformInstrumentation::DidDecodeImage() {
  TRACE_EVENT_END0(kCategoryName, kImageDecodeEvent);
}

inline void PlatformInstrumentation::DidDrawLazyPixelRef(
    unsigned long long lazy_pixel_ref_id) {
  TRACE_EVENT_INSTANT1(kCategoryName, kDrawLazyPixelRefEvent,
                       TRACE_EVENT_SCOPE_THREAD, kLazyPixelRef,
                       lazy_pixel_ref_id);
}

inline void PlatformInstrumentation::WillDecodeLazyPixelRef(
    unsigned long long lazy_pixel_ref_id) {
  TRACE_EVENT_BEGIN1(kCategoryName, kDecodeLazyPixelRefEvent, kLazyPixelRef,
                     lazy_pixel_ref_id);
}

inline void PlatformInstrumentation::DidDecodeLazyPixelRef() {
  TRACE_EVENT_END0(kCategoryName, kDecodeLazyPixelRefEvent);
}

}  // namespace blink

#endif  // PlatformInstrumentation_h
