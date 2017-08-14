/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScrollbarThemeClient_h
#define ScrollbarThemeClient_h

#include "platform/PlatformExport.h"
#include "platform/geometry/IntPoint.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/IntSize.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/wtf/Vector.h"

namespace blink {

class PLATFORM_EXPORT ScrollbarThemeClient {
 public:
  virtual int X() const = 0;
  virtual int Y() const = 0;
  virtual int Width() const = 0;
  virtual int Height() const = 0;
  virtual IntSize Size() const = 0;
  virtual IntPoint Location() const = 0;

  virtual void SetFrameRect(const IntRect&) = 0;
  virtual IntRect FrameRect() const = 0;

  virtual void Invalidate() = 0;
  virtual void InvalidateRect(const IntRect&) = 0;

  virtual ScrollbarOverlayColorTheme GetScrollbarOverlayColorTheme() const = 0;
  virtual void GetTickmarks(Vector<IntRect>&) const = 0;
  virtual bool IsScrollableAreaActive() const = 0;

  virtual IntPoint ConvertFromRootFrame(const IntPoint&) const = 0;
  virtual IntPoint ConvertFromRootFrameToParentView(const IntPoint&) const = 0;
  virtual IntPoint ConvertFromParentView(const IntPoint&) const = 0;

  virtual bool IsCustomScrollbar() const = 0;
  virtual ScrollbarOrientation Orientation() const = 0;
  virtual bool IsLeftSideVerticalScrollbar() const = 0;

  virtual int Value() const = 0;
  virtual float CurrentPos() const = 0;
  virtual int VisibleSize() const = 0;
  virtual int TotalSize() const = 0;
  virtual int Maximum() const = 0;
  virtual ScrollbarControlSize GetControlSize() const = 0;

  virtual ScrollbarPart PressedPart() const = 0;
  virtual ScrollbarPart HoveredPart() const = 0;

  virtual void StyleChanged() = 0;
  virtual void SetScrollbarsHidden(bool) = 0;

  virtual bool Enabled() const = 0;
  virtual void SetEnabled(bool) = 0;

  virtual bool IsOverlayScrollbar() const = 0;

  virtual float ElasticOverscroll() const = 0;
  virtual void SetElasticOverscroll(float) = 0;

 protected:
  virtual ~ScrollbarThemeClient() {}
};

}  // namespace blink

#endif  // ScollbarThemeClient_h
