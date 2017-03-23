// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PlatformInstrumentation_h
#define PlatformInstrumentation_h

#include "platform/PlatformExport.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/loader/fetch/FetchContext.h"

namespace blink {

class FetchContext;
class PlatformInstrumentationAgents;

namespace probe {

class PLATFORM_EXPORT ProbeBase {
  STACK_ALLOCATED()

 public:
  double captureStartTime() const;
  double captureEndTime() const;
  double duration() const;

 private:
  mutable double m_startTime = 0;
  mutable double m_endTime = 0;
};

inline PlatformInstrumentationAgents* instrumentingAgentsFor(
    FetchContext* context) {
  return context->instrumentingAgents();
}

}  // namespace probe

class PLATFORM_EXPORT PlatformInstrumentation {
 public:
  class LazyPixelRefTracker : TraceEvent::TraceScopedTrackableObject<void*> {
   public:
    LazyPixelRefTracker(void* instance)
        : TraceEvent::TraceScopedTrackableObject<void*>(CategoryName,
                                                        LazyPixelRef,
                                                        instance) {}
  };

  static const char ImageDecodeEvent[];
  static const char ImageResizeEvent[];
  static const char DrawLazyPixelRefEvent[];
  static const char DecodeLazyPixelRefEvent[];

  static const char ImageTypeArgument[];
  static const char CachedArgument[];

  static const char LazyPixelRef[];

  static void willDecodeImage(const String& imageType);
  static void didDecodeImage();
  static void didDrawLazyPixelRef(unsigned long long lazyPixelRefId);
  static void willDecodeLazyPixelRef(unsigned long long lazyPixelRefId);
  static void didDecodeLazyPixelRef();

 private:
  static const char CategoryName[];
};

inline void PlatformInstrumentation::willDecodeImage(const String& imageType) {
  TRACE_EVENT_BEGIN1(CategoryName, ImageDecodeEvent, ImageTypeArgument,
                     imageType.ascii());
}

inline void PlatformInstrumentation::didDecodeImage() {
  TRACE_EVENT_END0(CategoryName, ImageDecodeEvent);
}

inline void PlatformInstrumentation::didDrawLazyPixelRef(
    unsigned long long lazyPixelRefId) {
  TRACE_EVENT_INSTANT1(CategoryName, DrawLazyPixelRefEvent,
                       TRACE_EVENT_SCOPE_THREAD, LazyPixelRef, lazyPixelRefId);
}

inline void PlatformInstrumentation::willDecodeLazyPixelRef(
    unsigned long long lazyPixelRefId) {
  TRACE_EVENT_BEGIN1(CategoryName, DecodeLazyPixelRefEvent, LazyPixelRef,
                     lazyPixelRefId);
}

inline void PlatformInstrumentation::didDecodeLazyPixelRef() {
  TRACE_EVENT_END0(CategoryName, DecodeLazyPixelRefEvent);
}

}  // namespace blink

#include "platform/PlatformInstrumentationInl.h"

#endif  // PlatformInstrumentation_h
