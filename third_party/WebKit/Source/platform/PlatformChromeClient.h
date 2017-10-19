/*
 * Copyright (C) 2008 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef PlatformChromeClient_h
#define PlatformChromeClient_h

#include "platform/PlatformExport.h"
#include "platform/PlatformFrameView.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class IntRect;

class PLATFORM_EXPORT PlatformChromeClient
    : public GarbageCollectedFinalized<PlatformChromeClient> {
  WTF_MAKE_NONCOPYABLE(PlatformChromeClient);

 public:
  PlatformChromeClient() {}
  virtual ~PlatformChromeClient() {}
  virtual void Trace(blink::Visitor* visitor) {}

  // Requests the host invalidate the contents.
  virtual void InvalidateRect(const IntRect& update_rect) = 0;

  // Converts the rect from the viewport coordinates to screen coordinates.
  virtual IntRect ViewportToScreen(const IntRect&,
                                   const PlatformFrameView*) const = 0;

  // Converts the scalar value from the window coordinates to the viewport
  // scale.
  virtual float WindowToViewportScalar(const float) const = 0;

  virtual void ScheduleAnimation(const PlatformFrameView*) = 0;

  virtual bool IsPopup() { return false; }
};

}  // namespace blink

#endif  // PlatformChromeClient_h
