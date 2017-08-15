/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef ScrollbarThemeOverlay_h
#define ScrollbarThemeOverlay_h

#include "platform/graphics/Color.h"
#include "platform/scroll/ScrollbarTheme.h"

namespace blink {

// This scrollbar theme is used to get overlay scrollbar for platforms other
// than Mac. Mac's overlay scrollbars are in ScrollbarThemeMac*.
class PLATFORM_EXPORT ScrollbarThemeOverlay : public ScrollbarTheme {
 public:
  enum HitTestBehavior { kAllowHitTest, kDisallowHitTest };

  ScrollbarThemeOverlay(int thumb_thickness,
                        int scrollbar_margin,
                        HitTestBehavior);
  ScrollbarThemeOverlay(int thumb_thickness,
                        int scrollbar_margin,
                        HitTestBehavior,
                        Color);
  ~ScrollbarThemeOverlay() override {}

  bool ShouldRepaintAllPartsOnInvalidation() const override;

  ScrollbarPart InvalidateOnThumbPositionChange(
      const ScrollbarThemeClient&,
      float old_position,
      float new_position) const override;

  int ScrollbarThickness(ScrollbarControlSize) override;
  int ScrollbarMargin() const override;
  bool UsesOverlayScrollbars() const override;
  double OverlayScrollbarFadeOutDelaySeconds() const override;
  double OverlayScrollbarFadeOutDurationSeconds() const override;

  int ThumbLength(const ScrollbarThemeClient&) override;

  bool HasButtons(const ScrollbarThemeClient&) override { return false; }
  bool HasThumb(const ScrollbarThemeClient&) override;

  IntRect BackButtonRect(const ScrollbarThemeClient&,
                         ScrollbarPart,
                         bool painting = false) override;
  IntRect ForwardButtonRect(const ScrollbarThemeClient&,
                            ScrollbarPart,
                            bool painting = false) override;
  IntRect TrackRect(const ScrollbarThemeClient&,
                    bool painting = false) override;
  int ThumbThickness(const ScrollbarThemeClient&) override;
  int ThumbThickness() { return thumb_thickness_; }

  void PaintThumb(GraphicsContext&, const Scrollbar&, const IntRect&) override;

  ScrollbarPart HitTestWithParentPoint(const ScrollbarThemeClient&,
                                       const IntPoint&) override;

  bool UsesNinePatchThumbResource() const override;
  IntSize NinePatchThumbCanvasSize(const ScrollbarThemeClient&) const override;
  IntRect NinePatchThumbAperture(const ScrollbarThemeClient&) const override;

  int MinimumThumbLength(const ScrollbarThemeClient&) override;

  void SetHitTestEnabledForTesting(bool);

  static ScrollbarThemeOverlay& MobileTheme();

 private:
  int thumb_thickness_;
  int scrollbar_margin_;
  HitTestBehavior allow_hit_test_;
  Color color_;
  const bool use_solid_color_;
};

}  // namespace blink

#endif
