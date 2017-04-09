/*
 * Copyright (c) 2009, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ScrollbarThemeAura_h
#define ScrollbarThemeAura_h

#include "base/gtest_prod_util.h"
#include "platform/scroll/ScrollbarTheme.h"

namespace blink {

class PLATFORM_EXPORT ScrollbarThemeAura : public ScrollbarTheme {
 public:
  int ScrollbarThickness(ScrollbarControlSize) override;

 protected:
  bool HasButtons(const ScrollbarThemeClient&) override { return true; }
  bool HasThumb(const ScrollbarThemeClient&) override;

  IntRect BackButtonRect(const ScrollbarThemeClient&,
                         ScrollbarPart,
                         bool painting = false) override;
  IntRect ForwardButtonRect(const ScrollbarThemeClient&,
                            ScrollbarPart,
                            bool painting = false) override;
  IntRect TrackRect(const ScrollbarThemeClient&,
                    bool painting = false) override;
  int MinimumThumbLength(const ScrollbarThemeClient&) override;

  void PaintTrackBackground(GraphicsContext&,
                            const Scrollbar&,
                            const IntRect&) override;
  void PaintTrackPiece(GraphicsContext&,
                       const Scrollbar&,
                       const IntRect&,
                       ScrollbarPart) override;
  void PaintButton(GraphicsContext&,
                   const Scrollbar&,
                   const IntRect&,
                   ScrollbarPart) override;
  void PaintThumb(GraphicsContext&, const Scrollbar&, const IntRect&) override;

  bool ShouldRepaintAllPartsOnInvalidation() const override;
  ScrollbarPart InvalidateOnThumbPositionChange(
      const ScrollbarThemeClient&,
      float old_position,
      float new_position) const override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ScrollbarThemeAuraTest, ButtonSizeHorizontal);
  FRIEND_TEST_ALL_PREFIXES(ScrollbarThemeAuraTest, ButtonSizeVertical);
  FRIEND_TEST_ALL_PREFIXES(ScrollbarThemeAuraTest, NoButtonsReturnsSize0);

  virtual bool HasScrollbarButtons(ScrollbarOrientation) const;
  IntSize ButtonSize(const ScrollbarThemeClient&);
};

}  // namespace blink

#endif
