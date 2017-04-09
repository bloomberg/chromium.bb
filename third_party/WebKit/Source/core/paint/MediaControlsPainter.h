/*
 * Copyright (C) 2009 Apple Inc.
 * Copyright (C) 2009 Google Inc.
 * All rights reserved.
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

#ifndef MediaControlsPainter_h
#define MediaControlsPainter_h

#include "wtf/Allocator.h"

namespace blink {

struct PaintInfo;

class ComputedStyle;
class IntRect;
class LayoutObject;

class MediaControlsPainter {
  STATIC_ONLY(MediaControlsPainter);

 public:
  static bool PaintMediaMuteButton(const LayoutObject&,
                                   const PaintInfo&,
                                   const IntRect&);
  static bool PaintMediaPlayButton(const LayoutObject&,
                                   const PaintInfo&,
                                   const IntRect&);
  static bool PaintMediaToggleClosedCaptionsButton(const LayoutObject&,
                                                   const PaintInfo&,
                                                   const IntRect&);
  static bool PaintMediaSlider(const LayoutObject&,
                               const PaintInfo&,
                               const IntRect&);
  static bool PaintMediaSliderThumb(const LayoutObject&,
                                    const PaintInfo&,
                                    const IntRect&);
  static bool PaintMediaVolumeSlider(const LayoutObject&,
                                     const PaintInfo&,
                                     const IntRect&);
  static bool PaintMediaVolumeSliderThumb(const LayoutObject&,
                                          const PaintInfo&,
                                          const IntRect&);
  static bool PaintMediaFullscreenButton(const LayoutObject&,
                                         const PaintInfo&,
                                         const IntRect&);
  static bool PaintMediaOverlayPlayButton(const LayoutObject&,
                                          const PaintInfo&,
                                          const IntRect&);
  static bool PaintMediaCastButton(const LayoutObject&,
                                   const PaintInfo&,
                                   const IntRect&);
  static bool PaintMediaTrackSelectionCheckmark(const LayoutObject&,
                                                const PaintInfo&,
                                                const IntRect&);
  static bool PaintMediaClosedCaptionsIcon(const LayoutObject&,
                                           const PaintInfo&,
                                           const IntRect&);
  static bool PaintMediaSubtitlesIcon(const LayoutObject&,
                                      const PaintInfo&,
                                      const IntRect&);
  static bool PaintMediaOverflowMenu(const LayoutObject&,
                                     const PaintInfo&,
                                     const IntRect&);
  static bool PaintMediaDownloadIcon(const LayoutObject&,
                                     const PaintInfo&,
                                     const IntRect&);
  static void AdjustMediaSliderThumbSize(ComputedStyle&);

 private:
  static void AdjustMediaSliderThumbPaintSize(const IntRect&,
                                              const ComputedStyle&,
                                              IntRect& rect_out);
  static void PaintMediaSliderInternal(const LayoutObject&,
                                       const PaintInfo&,
                                       const IntRect&);
};

}  // namespace blink

#endif  // MediaControlsPainter_h
