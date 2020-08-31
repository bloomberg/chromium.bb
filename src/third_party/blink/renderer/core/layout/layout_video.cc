/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/renderer/core/layout/layout_video.h"

#include "third_party/blink/public/platform/web_size.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/html/media/html_video_element.h"
#include "third_party/blink/renderer/core/paint/video_painter.h"

namespace blink {

LayoutVideo::LayoutVideo(HTMLVideoElement* video) : LayoutMedia(video) {
  SetIntrinsicSize(CalculateIntrinsicSize());
}

LayoutVideo::~LayoutVideo() = default;

LayoutSize LayoutVideo::DefaultSize() {
  return LayoutSize(kDefaultWidth, kDefaultHeight);
}

void LayoutVideo::IntrinsicSizeChanged() {
  if (VideoElement()->ShouldDisplayPosterImage())
    LayoutMedia::IntrinsicSizeChanged();
  UpdateIntrinsicSize(/* is_in_layout */ false);
}

void LayoutVideo::UpdateIntrinsicSize(bool is_in_layout) {
  LayoutSize size = CalculateIntrinsicSize();
  size.Scale(StyleRef().EffectiveZoom());

  // Never set the element size to zero when in a media document.
  if (size.IsEmpty() && GetNode()->ownerDocument() &&
      GetNode()->ownerDocument()->IsMediaDocument())
    return;

  if (size == IntrinsicSize())
    return;

  SetIntrinsicSize(size);
  SetIntrinsicLogicalWidthsDirty();
  if (!is_in_layout) {
    SetNeedsLayoutAndFullPaintInvalidation(
        layout_invalidation_reason::kSizeChanged);
  }
}

LayoutSize LayoutVideo::CalculateIntrinsicSize() {
  HTMLVideoElement* video = VideoElement();
  DCHECK(video);

  if (RuntimeEnabledFeatures::ExperimentalProductivityFeaturesEnabled()) {
    if (video->IsDefaultIntrinsicSize())
      return DefaultSize();
  }

  // Spec text from 4.8.6
  //
  // The intrinsic width of a video element's playback area is the intrinsic
  // width of the video resource, if that is available; otherwise it is the
  // intrinsic width of the poster frame, if that is available; otherwise it is
  // 300 CSS pixels.
  //
  // The intrinsic height of a video element's playback area is the intrinsic
  // height of the video resource, if that is available; otherwise it is the
  // intrinsic height of the poster frame, if that is available; otherwise it is
  // 150 CSS pixels.
  WebMediaPlayer* web_media_player = MediaElement()->GetWebMediaPlayer();
  if (web_media_player &&
      video->getReadyState() >= HTMLVideoElement::kHaveMetadata) {
    IntSize size(web_media_player->NaturalSize());
    if (!size.IsEmpty())
      return LayoutSize(size);
  }

  if (video->ShouldDisplayPosterImage() && !cached_image_size_.IsEmpty() &&
      !ImageResource()->ErrorOccurred())
    return cached_image_size_;

  return DefaultSize();
}

void LayoutVideo::ImageChanged(WrappedImagePtr new_image,
                               CanDeferInvalidation defer) {
  LayoutMedia::ImageChanged(new_image, defer);

  // Cache the image intrinsic size so we can continue to use it to draw the
  // image correctly even if we know the video intrinsic size but aren't able to
  // draw video frames yet (we don't want to scale the poster to the video size
  // without keeping aspect ratio). We do not need to check
  // |ShouldDisplayPosterImage| because the image can be ready before we find
  // out we actually need it.
  cached_image_size_ = IntrinsicSize();

  // The intrinsic size is now that of the image, but in case we already had the
  // intrinsic size of the video we call this here to restore the video size.
  UpdateIntrinsicSize(/* is_in_layout */ false);
}

bool LayoutVideo::ShouldDisplayVideo() const {
  return !VideoElement()->ShouldDisplayPosterImage();
}

void LayoutVideo::PaintReplaced(const PaintInfo& paint_info,
                                const PhysicalOffset& paint_offset) const {
  VideoPainter(*this).PaintReplaced(paint_info, paint_offset);
}

void LayoutVideo::UpdateLayout() {
  UpdatePlayer(/* is_in_layout */ true);
  LayoutMedia::UpdateLayout();
}

HTMLVideoElement* LayoutVideo::VideoElement() const {
  return To<HTMLVideoElement>(GetNode());
}

void LayoutVideo::UpdateFromElement() {
  LayoutMedia::UpdateFromElement();
  UpdatePlayer(/* is_in_layout */ false);

  // If the DisplayMode of the video changed, then we need to paint.
  SetShouldDoFullPaintInvalidation();
}

void LayoutVideo::UpdatePlayer(bool is_in_layout) {
  UpdateIntrinsicSize(is_in_layout);

  WebMediaPlayer* media_player = MediaElement()->GetWebMediaPlayer();
  if (!media_player)
    return;

  if (!VideoElement()->InActiveDocument())
    return;

  VideoElement()->SetNeedsCompositingUpdate();
}

LayoutUnit LayoutVideo::ComputeReplacedLogicalWidth(
    ShouldComputePreferred should_compute_preferred) const {
  return LayoutReplaced::ComputeReplacedLogicalWidth(should_compute_preferred);
}

LayoutUnit LayoutVideo::ComputeReplacedLogicalHeight(
    LayoutUnit estimated_used_width) const {
  return LayoutReplaced::ComputeReplacedLogicalHeight(estimated_used_width);
}

LayoutUnit LayoutVideo::MinimumReplacedHeight() const {
  return LayoutReplaced::MinimumReplacedHeight();
}

PhysicalRect LayoutVideo::ReplacedContentRect() const {
  if (ShouldDisplayVideo()) {
    // Video codecs may need to restart from an I-frame when the output is
    // resized. Round size in advance to avoid 1px snap difference.
    return PreSnappedRectForPersistentSizing(ComputeObjectFit());
  }
  // If we are displaying the poster image no pre-rounding is needed, but the
  // size of the image should be used for fitting instead.
  return ComputeObjectFit(&cached_image_size_);
}

bool LayoutVideo::SupportsAcceleratedRendering() const {
  return !!MediaElement()->CcLayer();
}

CompositingReasons LayoutVideo::AdditionalCompositingReasons() const {
  auto* element = To<HTMLMediaElement>(GetNode());
  if (element->IsFullscreen() && element->UsesOverlayFullscreenVideo())
    return CompositingReason::kVideo;

  if (ShouldDisplayVideo() && SupportsAcceleratedRendering())
    return CompositingReason::kVideo;

  return CompositingReason::kNone;
}

}  // namespace blink
