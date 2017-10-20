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

#ifndef ScrollbarTheme_h
#define ScrollbarTheme_h

#include "platform/PlatformExport.h"
#include "platform/geometry/IntRect.h"
#include "platform/graphics/paint/DisplayItem.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/scroll/Scrollbar.h"
#include "public/platform/WebScrollbarButtonsPlacement.h"

namespace blink {

class CullRect;
class GraphicsContext;
class WebMouseEvent;

class PLATFORM_EXPORT ScrollbarTheme {
  WTF_MAKE_NONCOPYABLE(ScrollbarTheme);
  USING_FAST_MALLOC(ScrollbarTheme);

 public:
  ScrollbarTheme() {}
  virtual ~ScrollbarTheme() {}

  // If true, then scrollbars with this theme will be painted every time
  // Scrollbar::setNeedsPaintInvalidation is called. If false, then only parts
  // which are explicitly invalidated will be repainted.
  virtual bool ShouldRepaintAllPartsOnInvalidation() const { return true; }

  virtual void UpdateEnabledState(const ScrollbarThemeClient&) {}

  virtual bool Paint(const Scrollbar&, GraphicsContext&, const CullRect&);

  virtual ScrollbarPart HitTest(const ScrollbarThemeClient&, const IntPoint&);

  // This returns a fixed value regardless of device-scale-factor.
  // This returns thickness when scrollbar is painted.  i.e. It's not 0 even in
  // overlay scrollbar mode.
  // See also Scrollbar::scrollbarThickness().
  virtual int ScrollbarThickness(ScrollbarControlSize = kRegularScrollbar) {
    return 0;
  }
  virtual int ScrollbarMargin() const { return 0; }

  virtual WebScrollbarButtonsPlacement ButtonsPlacement() const {
    return kWebScrollbarButtonsPlacementSingle;
  }

  virtual bool SupportsControlTints() const { return false; }
  virtual bool UsesOverlayScrollbars() const { return false; }
  virtual void UpdateScrollbarOverlayColorTheme(const ScrollbarThemeClient&) {}

  // If true, scrollbars that become invisible (i.e. overlay scrollbars that
  // fade out) should be marked as disabled. This option exists since Mac and
  // Aura overlays implement the fade out differently, with Mac painting code
  // fading out the scrollbars. Aura scrollbars require disabling the scrollbar
  // to prevent painting it.
  virtual bool ShouldDisableInvisibleScrollbars() const { return true; }

  virtual bool InvalidateOnMouseEnterExit() { return false; }
  virtual bool InvalidateOnWindowActiveChange() const { return false; }

  // Returns parts of the scrollbar which must be repainted following a change
  // in the thumb position, given scroll positions before and after.
  virtual ScrollbarPart InvalidateOnThumbPositionChange(
      const ScrollbarThemeClient&,
      float old_position,
      float new_position) const {
    return kAllParts;
  }

  virtual void PaintScrollCorner(GraphicsContext&,
                                 const DisplayItemClient&,
                                 const IntRect& corner_rect);
  virtual void PaintTickmarks(GraphicsContext&,
                              const Scrollbar&,
                              const IntRect&);

  virtual bool ShouldCenterOnThumb(const ScrollbarThemeClient&,
                                   const WebMouseEvent&);
  virtual bool ShouldSnapBackToDragOrigin(const ScrollbarThemeClient&,
                                          const WebMouseEvent&);
  virtual bool ShouldDragDocumentInsteadOfThumb(const ScrollbarThemeClient&,
                                                const WebMouseEvent&) {
    return false;
  }

  // The position of the thumb relative to the track.
  int ThumbPosition(const ScrollbarThemeClient& scrollbar) {
    return ThumbPosition(scrollbar, scrollbar.CurrentPos());
  }
  virtual double OverlayScrollbarFadeOutDelaySeconds() const;
  virtual double OverlayScrollbarFadeOutDurationSeconds() const;
  // The position the thumb would have, relative to the track, at the specified
  // scroll position.
  virtual int ThumbPosition(const ScrollbarThemeClient&, float scroll_position);
  // The length of the thumb along the axis of the scrollbar.
  virtual int ThumbLength(const ScrollbarThemeClient&);
  // The position of the track relative to the scrollbar.
  virtual int TrackPosition(const ScrollbarThemeClient&);
  // The length of the track along the axis of the scrollbar.
  virtual int TrackLength(const ScrollbarThemeClient&);
  // The opacity to be applied to the thumb.
  virtual float ThumbOpacity(const ScrollbarThemeClient&) const { return 1.0f; }

  virtual bool HasButtons(const ScrollbarThemeClient&) = 0;
  virtual bool HasThumb(const ScrollbarThemeClient&) = 0;

  virtual IntRect BackButtonRect(const ScrollbarThemeClient&,
                                 ScrollbarPart,
                                 bool painting = false) = 0;
  virtual IntRect ForwardButtonRect(const ScrollbarThemeClient&,
                                    ScrollbarPart,
                                    bool painting = false) = 0;
  virtual IntRect TrackRect(const ScrollbarThemeClient&,
                            bool painting = false) = 0;
  virtual IntRect ThumbRect(const ScrollbarThemeClient&);
  virtual int ThumbThickness(const ScrollbarThemeClient&);

  virtual int MinimumThumbLength(const ScrollbarThemeClient&) = 0;

  virtual void SplitTrack(const ScrollbarThemeClient&,
                          const IntRect& track,
                          IntRect& start_track,
                          IntRect& thumb,
                          IntRect& end_track);

  virtual void PaintScrollbarBackground(GraphicsContext&, const Scrollbar&) {}
  virtual void PaintTrackBackground(GraphicsContext&,
                                    const Scrollbar&,
                                    const IntRect&) {}
  virtual void PaintTrackPiece(GraphicsContext&,
                               const Scrollbar&,
                               const IntRect&,
                               ScrollbarPart) {}
  virtual void PaintButton(GraphicsContext&,
                           const Scrollbar&,
                           const IntRect&,
                           ScrollbarPart) {}
  virtual void PaintThumb(GraphicsContext&, const Scrollbar&, const IntRect&) {}

  virtual int MaxOverlapBetweenPages() {
    return std::numeric_limits<int>::max();
  }

  virtual double InitialAutoscrollTimerDelay() { return 0.25; }
  virtual double AutoscrollTimerDelay() { return 0.05; }

  virtual IntRect ConstrainTrackRectToTrackPieces(const ScrollbarThemeClient&,
                                                  const IntRect& rect) {
    return rect;
  }

  virtual void RegisterScrollbar(ScrollbarThemeClient&) {}
  virtual void UnregisterScrollbar(ScrollbarThemeClient&) {}

  virtual bool IsMockTheme() const { return false; }

  virtual bool UsesNinePatchThumbResource() const { return false; }

  // For a nine-patch scrollbar, this defines the painting canvas size which the
  // painting code will use to paint the scrollbar into. The actual scrollbar
  // dimensions will be ignored for purposes of painting since the resource can
  // be then resized without a repaint.
  virtual IntSize NinePatchThumbCanvasSize(const ScrollbarThemeClient&) const {
    NOTREACHED();
    return IntSize();
  }

  // For a nine-patch resource, the aperture defines the center patch that will
  // be stretched out.
  virtual IntRect NinePatchThumbAperture(const ScrollbarThemeClient&) const {
    NOTREACHED();
    return IntRect();
  }

  // Warning: Please call Page::GetScrollbarTheme instead of call this method
  // directly since we support different native scrollbar theme base on page
  // settings. See crrev.com/c/646727, this function will eventually be removed.
  static ScrollbarTheme& DeprecatedStaticGetTheme();

  static void SetMockScrollbarsEnabled(bool flag);
  static bool MockScrollbarsEnabled();

 protected:
  virtual int TickmarkBorderWidth() { return 0; }
  static DisplayItem::Type ButtonPartToDisplayItemType(ScrollbarPart);
  static DisplayItem::Type TrackPiecePartToDisplayItemType(ScrollbarPart);

 private:
  static ScrollbarTheme&
  NativeTheme();  // Must be implemented to return the correct theme subclass.
  static bool g_mock_scrollbars_enabled_;
};

}  // namespace blink
#endif
