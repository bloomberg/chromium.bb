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
  virtual bool shouldRepaintAllPartsOnInvalidation() const { return true; }

  virtual void updateEnabledState(const ScrollbarThemeClient&) {}

  virtual bool paint(const Scrollbar&, GraphicsContext&, const CullRect&);

  virtual ScrollbarPart hitTest(const ScrollbarThemeClient&, const IntPoint&);

  // This returns a fixed value regardless of device-scale-factor.
  // This returns thickness when scrollbar is painted.  i.e. It's not 0 even in
  // overlay scrollbar mode.
  // See also Scrollbar::scrollbarThickness().
  virtual int scrollbarThickness(ScrollbarControlSize = RegularScrollbar) {
    return 0;
  }
  virtual int scrollbarMargin() const { return 0; }

  virtual WebScrollbarButtonsPlacement buttonsPlacement() const {
    return WebScrollbarButtonsPlacementSingle;
  }

  virtual bool supportsControlTints() const { return false; }
  virtual bool usesOverlayScrollbars() const { return false; }
  virtual void updateScrollbarOverlayColorTheme(const ScrollbarThemeClient&) {}

  // If true, scrollbars that become invisible (i.e. overlay scrollbars that
  // fade out) should be marked as disabled. This option exists since Mac and
  // Aura overlays implement the fade out differently, with Mac painting code
  // fading out the scrollbars. Aura scrollbars require disabling the scrollbar
  // to prevent painting it.
  virtual bool shouldDisableInvisibleScrollbars() const { return true; }

  virtual bool invalidateOnMouseEnterExit() { return false; }
  virtual bool invalidateOnWindowActiveChange() const { return false; }

  // Returns parts of the scrollbar which must be repainted following a change
  // in the thumb position, given scroll positions before and after.
  virtual ScrollbarPart invalidateOnThumbPositionChange(
      const ScrollbarThemeClient&,
      float oldPosition,
      float newPosition) const {
    return AllParts;
  }

  // Returns parts of the scrollbar which must be repainted following a change
  // in enabled state.
  virtual ScrollbarPart invalidateOnEnabledChange() const { return AllParts; }

  virtual void paintScrollCorner(GraphicsContext&,
                                 const DisplayItemClient&,
                                 const IntRect& cornerRect);
  virtual void paintTickmarks(GraphicsContext&,
                              const Scrollbar&,
                              const IntRect&);

  virtual bool shouldCenterOnThumb(const ScrollbarThemeClient&,
                                   const WebMouseEvent&);
  virtual bool shouldSnapBackToDragOrigin(const ScrollbarThemeClient&,
                                          const WebMouseEvent&);
  virtual bool shouldDragDocumentInsteadOfThumb(const ScrollbarThemeClient&,
                                                const WebMouseEvent&) {
    return false;
  }

  // The position of the thumb relative to the track.
  int thumbPosition(const ScrollbarThemeClient& scrollbar) {
    return thumbPosition(scrollbar, scrollbar.currentPos());
  }
  virtual double overlayScrollbarFadeOutDelaySeconds() const;
  virtual double overlayScrollbarFadeOutDurationSeconds() const;
  // The position the thumb would have, relative to the track, at the specified
  // scroll position.
  virtual int thumbPosition(const ScrollbarThemeClient&, float scrollPosition);
  // The length of the thumb along the axis of the scrollbar.
  virtual int thumbLength(const ScrollbarThemeClient&);
  // The position of the track relative to the scrollbar.
  virtual int trackPosition(const ScrollbarThemeClient&);
  // The length of the track along the axis of the scrollbar.
  virtual int trackLength(const ScrollbarThemeClient&);
  // The opacity to be applied to the thumb.
  virtual float thumbOpacity(const ScrollbarThemeClient&) const { return 1.0f; }

  virtual bool hasButtons(const ScrollbarThemeClient&) = 0;
  virtual bool hasThumb(const ScrollbarThemeClient&) = 0;

  virtual IntRect backButtonRect(const ScrollbarThemeClient&,
                                 ScrollbarPart,
                                 bool painting = false) = 0;
  virtual IntRect forwardButtonRect(const ScrollbarThemeClient&,
                                    ScrollbarPart,
                                    bool painting = false) = 0;
  virtual IntRect trackRect(const ScrollbarThemeClient&,
                            bool painting = false) = 0;
  virtual IntRect thumbRect(const ScrollbarThemeClient&);
  virtual int thumbThickness(const ScrollbarThemeClient&);

  virtual int minimumThumbLength(const ScrollbarThemeClient&);

  virtual void splitTrack(const ScrollbarThemeClient&,
                          const IntRect& track,
                          IntRect& startTrack,
                          IntRect& thumb,
                          IntRect& endTrack);

  virtual void paintScrollbarBackground(GraphicsContext&, const Scrollbar&) {}
  virtual void paintTrackBackground(GraphicsContext&,
                                    const Scrollbar&,
                                    const IntRect&) {}
  virtual void paintTrackPiece(GraphicsContext&,
                               const Scrollbar&,
                               const IntRect&,
                               ScrollbarPart) {}
  virtual void paintButton(GraphicsContext&,
                           const Scrollbar&,
                           const IntRect&,
                           ScrollbarPart) {}
  virtual void paintThumb(GraphicsContext&, const Scrollbar&, const IntRect&) {}

  virtual int maxOverlapBetweenPages() {
    return std::numeric_limits<int>::max();
  }

  virtual double initialAutoscrollTimerDelay() { return 0.25; }
  virtual double autoscrollTimerDelay() { return 0.05; }

  virtual IntRect constrainTrackRectToTrackPieces(const ScrollbarThemeClient&,
                                                  const IntRect& rect) {
    return rect;
  }

  virtual void registerScrollbar(ScrollbarThemeClient&) {}
  virtual void unregisterScrollbar(ScrollbarThemeClient&) {}

  virtual bool isMockTheme() const { return false; }

  virtual bool usesNinePatchThumbResource() const { return false; }

  // For a nine-patch scrollbar, this defines the painting canvas size which the
  // painting code will use to paint the scrollbar into. The actual scrollbar
  // dimensions will be ignored for purposes of painting since the resource can
  // be then resized without a repaint.
  virtual IntSize ninePatchThumbCanvasSize(const ScrollbarThemeClient&) const {
    NOTREACHED();
    return IntSize();
  }

  // For a nine-patch resource, the aperture defines the center patch that will
  // be stretched out.
  virtual IntRect ninePatchThumbAperture(const ScrollbarThemeClient&) const {
    NOTREACHED();
    return IntRect();
  }

  static ScrollbarTheme& theme();

  static void setMockScrollbarsEnabled(bool flag);
  static bool mockScrollbarsEnabled();

 protected:
  virtual int tickmarkBorderWidth() { return 0; }
  static DisplayItem::Type buttonPartToDisplayItemType(ScrollbarPart);
  static DisplayItem::Type trackPiecePartToDisplayItemType(ScrollbarPart);

 private:
  static ScrollbarTheme&
  nativeTheme();  // Must be implemented to return the correct theme subclass.
  static bool gMockScrollbarsEnabled;
};

}  // namespace blink
#endif
