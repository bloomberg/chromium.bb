/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebScrollbar_h
#define WebScrollbar_h

#include "WebPoint.h"
#include "WebRect.h"
#include "WebScrollbarOverlayColorTheme.h"
#include "WebSize.h"
#include "WebVector.h"

namespace blink {

// A const accessor interface for a WebKit scrollbar
class BLINK_PLATFORM_EXPORT WebScrollbar {
 public:
  enum Orientation { kHorizontal, kVertical };

  enum ScrollDirection { kScrollBackward, kScrollForward };

  enum ScrollGranularity {
    kScrollByLine,
    kScrollByPage,
    kScrollByDocument,
    kScrollByPixel
  };

  enum ScrollbarControlSize { kRegularScrollbar, kSmallScrollbar };

  enum ScrollbarPart {
    kNoPart = 0,
    kBackButtonStartPart = 1,
    kForwardButtonStartPart = 1 << 1,
    kBackTrackPart = 1 << 2,
    kThumbPart = 1 << 3,
    kForwardTrackPart = 1 << 4,
    kBackButtonEndPart = 1 << 5,
    kForwardButtonEndPart = 1 << 6,
    kScrollbarBGPart = 1 << 7,
    kTrackBGPart = 1 << 8,
    kAllParts = 0xffffffff
  };

  enum class ScrollingMode { kAuto, kAlwaysOff, kAlwaysOn, kLast = kAlwaysOn };

  virtual ~WebScrollbar() = default;

  // Return true if this is an overlay scrollbar.
  virtual bool IsOverlay() const = 0;

  // Gets the current value (i.e. position inside the region).
  virtual int Value() const = 0;

  virtual WebPoint Location() const = 0;
  virtual WebSize Size() const = 0;
  virtual bool Enabled() const = 0;
  virtual int Maximum() const = 0;
  virtual int TotalSize() const = 0;
  virtual bool IsScrollableAreaActive() const = 0;
  virtual void GetTickmarks(WebVector<WebRect>& tickmarks) const = 0;
  virtual ScrollbarControlSize GetControlSize() const = 0;
  virtual ScrollbarPart PressedPart() const = 0;
  virtual ScrollbarPart HoveredPart() const = 0;
  virtual WebScrollbarOverlayColorTheme ScrollbarOverlayColorTheme() const = 0;
  virtual bool IsCustomScrollbar() const = 0;
  virtual Orientation GetOrientation() const = 0;
  virtual bool IsLeftSideVerticalScrollbar() const = 0;
  virtual float ElasticOverscroll() const = 0;
  virtual void SetElasticOverscroll(float) = 0;
};

}  // namespace blink

#endif
