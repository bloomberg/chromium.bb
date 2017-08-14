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

#ifndef WebScrollbarThemeClientImpl_h
#define WebScrollbarThemeClientImpl_h

#include "platform/PlatformExport.h"
#include "platform/scroll/ScrollbarThemeClient.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"
#include "public/platform/WebScrollbar.h"

namespace blink {

// Adapts a WebScrollbar to the ScrollbarThemeClient interface
class PLATFORM_EXPORT WebScrollbarThemeClientImpl
    : public ScrollbarThemeClient {
  DISALLOW_NEW();
  WTF_MAKE_NONCOPYABLE(WebScrollbarThemeClientImpl);

 public:
  // Caller must retain ownership of this pointer and ensure that its lifetime
  // exceeds this instance.
  WebScrollbarThemeClientImpl(WebScrollbar&);
  ~WebScrollbarThemeClientImpl() override;

  // Implement ScrollbarThemeClient interface
  int X() const override;
  int Y() const override;
  int Width() const override;
  int Height() const override;
  IntSize Size() const override;
  IntPoint Location() const override;
  void SetFrameRect(const IntRect&) override;
  IntRect FrameRect() const override;
  void Invalidate() override;
  void InvalidateRect(const IntRect&) override;
  ScrollbarOverlayColorTheme GetScrollbarOverlayColorTheme() const override;
  void GetTickmarks(Vector<IntRect>&) const override;
  bool IsScrollableAreaActive() const override;
  IntPoint ConvertFromRootFrame(const IntPoint&) const override;
  IntPoint ConvertFromRootFrameToParentView(const IntPoint&) const override;
  IntPoint ConvertFromParentView(const IntPoint&) const override;
  bool IsCustomScrollbar() const override;
  ScrollbarOrientation Orientation() const override;
  bool IsLeftSideVerticalScrollbar() const override;
  int Value() const override;
  float CurrentPos() const override;
  int VisibleSize() const override;
  int TotalSize() const override;
  int Maximum() const override;
  ScrollbarControlSize GetControlSize() const override;
  ScrollbarPart PressedPart() const override;
  ScrollbarPart HoveredPart() const override;
  void StyleChanged() override;
  void SetScrollbarsHidden(bool) override;
  bool Enabled() const override;
  void SetEnabled(bool) override;
  bool IsOverlayScrollbar() const override;
  float ElasticOverscroll() const override;
  void SetElasticOverscroll(float) override;

 private:
  WebScrollbar& scrollbar_;
};

}  // namespace blink

#endif  // WebScrollbarThemeClientImpl_h
