/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#ifndef ScrollbarThemeMac_h
#define ScrollbarThemeMac_h

#include <AppKit/AppKit.h>

#include "platform/mac/NSScrollerImpDetails.h"
#include "platform/scroll/ScrollbarTheme.h"

typedef id ScrollbarPainter;

namespace blink {

class Pattern;

class PLATFORM_EXPORT ScrollbarThemeMac : public ScrollbarTheme {
 public:
  ~ScrollbarThemeMac() override;

  void RegisterScrollbar(ScrollbarThemeClient&) override;
  void UnregisterScrollbar(ScrollbarThemeClient&) override;
  void PreferencesChanged(float initial_button_delay,
                          float autoscroll_button_delay,
                          NSScrollerStyle preferred_scroller_style,
                          bool redraw,
                          WebScrollbarButtonsPlacement);

  bool SupportsControlTints() const override { return true; }

  // On Mac, the painting code itself animates the opacity so there's no need
  // to disable in order to make the scrollbars invisible. In fact,
  // disabling/enabling causes invalidations which can cause endless loops as
  // Mac queues up scrollbar paint timers.
  bool ShouldDisableInvisibleScrollbars() const override { return false; }

  double InitialAutoscrollTimerDelay() override;
  double AutoscrollTimerDelay() override;

  void PaintTickmarks(GraphicsContext&,
                      const Scrollbar&,
                      const IntRect&) override;

  bool ShouldRepaintAllPartsOnInvalidation() const override { return false; }
  ScrollbarPart InvalidateOnThumbPositionChange(
      const ScrollbarThemeClient&,
      float old_position,
      float new_position) const override;
  void UpdateEnabledState(const ScrollbarThemeClient&) override;
  int ScrollbarThickness(ScrollbarControlSize = kRegularScrollbar) override;
  bool UsesOverlayScrollbars() const override;
  void UpdateScrollbarOverlayColorTheme(const ScrollbarThemeClient&) override;
  WebScrollbarButtonsPlacement ButtonsPlacement() const override;

  void SetNewPainterForScrollbar(ScrollbarThemeClient&, ScrollbarPainter);
  ScrollbarPainter PainterForScrollbar(const ScrollbarThemeClient&) const;

  void PaintTrackBackground(GraphicsContext&,
                            const Scrollbar&,
                            const IntRect&) override;
  void PaintThumb(GraphicsContext&, const Scrollbar&, const IntRect&) override;

  float ThumbOpacity(const ScrollbarThemeClient&) const override;

  static NSScrollerStyle RecommendedScrollerStyle();

 protected:
  int MaxOverlapBetweenPages() override { return 40; }

  bool ShouldDragDocumentInsteadOfThumb(const ScrollbarThemeClient&,
                                        const WebMouseEvent&) override;
  int ScrollbarPartToHIPressedState(ScrollbarPart);

  virtual void UpdateButtonPlacement(WebScrollbarButtonsPlacement) {}

  IntRect TrackRect(const ScrollbarThemeClient&,
                    bool painting = false) override;
  IntRect BackButtonRect(const ScrollbarThemeClient&,
                         ScrollbarPart,
                         bool painting = false) override;
  IntRect ForwardButtonRect(const ScrollbarThemeClient&,
                            ScrollbarPart,
                            bool painting = false) override;

  bool HasButtons(const ScrollbarThemeClient&) override { return false; }
  bool HasThumb(const ScrollbarThemeClient&) override;

  int MinimumThumbLength(const ScrollbarThemeClient&) override;

  int TickmarkBorderWidth() override { return 1; }

  scoped_refptr<Pattern> overhang_pattern_;
};
}

#endif  // ScrollbarThemeMac_h
