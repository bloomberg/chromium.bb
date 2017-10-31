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

#include "core/layout/LayoutVideo.h"

#include "core/dom/Document.h"
#include "core/html/media/HTMLVideoElement.h"
#include "core/html_names.h"
#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutFullScreen.h"
#include "core/paint/VideoPainter.h"
#include "public/platform/WebLayer.h"

namespace blink {

using namespace HTMLNames;

LayoutVideo::LayoutVideo(HTMLVideoElement* video) : LayoutMedia(video) {
  SetIntrinsicSize(CalculateIntrinsicSize());
}

LayoutVideo::~LayoutVideo() {}

LayoutSize LayoutVideo::DefaultSize() {
  return LayoutSize(kDefaultWidth, kDefaultHeight);
}

void LayoutVideo::IntrinsicSizeChanged() {
  if (VideoElement()->ShouldDisplayPosterImage())
    LayoutMedia::IntrinsicSizeChanged();
  UpdateIntrinsicSize();
}

void LayoutVideo::UpdateIntrinsicSize() {
  LayoutSize size = CalculateIntrinsicSize();
  size.Scale(Style()->EffectiveZoom());

  // Never set the element size to zero when in a media document.
  if (size.IsEmpty() && GetNode()->ownerDocument() &&
      GetNode()->ownerDocument()->IsMediaDocument())
    return;

  if (size == IntrinsicSize())
    return;

  SetIntrinsicSize(size);
  SetPreferredLogicalWidthsDirty();
  SetNeedsLayoutAndFullPaintInvalidation(
      LayoutInvalidationReason::kSizeChanged);
}

LayoutSize LayoutVideo::CalculateIntrinsicSize() {
  HTMLVideoElement* video = VideoElement();

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
    IntSize size = web_media_player->NaturalSize();
    if (!size.IsEmpty())
      return LayoutSize(size);
  }

  if (video->ShouldDisplayPosterImage() && !cached_image_size_.IsEmpty() &&
      !ImageResource()->ErrorOccurred())
    return cached_image_size_;

  // <video> in standalone media documents should not use the default 300x150
  // size since they also have audio-only files. By setting the intrinsic
  // size to 300x1 the video will resize itself in these cases, and audio will
  // have the correct height (it needs to be > 0 for controls to layout
  // properly).
  if (video->ownerDocument() && video->ownerDocument()->IsMediaDocument())
    return LayoutSize(DefaultSize().Width(), LayoutUnit(1));

  return DefaultSize();
}

void LayoutVideo::ImageChanged(WrappedImagePtr new_image,
                               CanDeferInvalidation defer,
                               const IntRect* rect) {
  LayoutMedia::ImageChanged(new_image, defer, rect);

  // Cache the image intrinsic size so we can continue to use it to draw the
  // image correctly even if we know the video intrinsic size but aren't able to
  // draw video frames yet (we don't want to scale the poster to the video size
  // without keeping aspect ratio).
  if (VideoElement()->ShouldDisplayPosterImage())
    cached_image_size_ = IntrinsicSize();

  // The intrinsic size is now that of the image, but in case we already had the
  // intrinsic size of the video we call this here to restore the video size.
  UpdateIntrinsicSize();
}

bool LayoutVideo::ShouldDisplayVideo() const {
  return !VideoElement()->ShouldDisplayPosterImage();
}

void LayoutVideo::PaintReplaced(const PaintInfo& paint_info,
                                const LayoutPoint& paint_offset) const {
  VideoPainter(*this).PaintReplaced(paint_info, paint_offset);
}

void LayoutVideo::UpdateLayout() {
  UpdatePlayer();
  LayoutMedia::UpdateLayout();
}

HTMLVideoElement* LayoutVideo::VideoElement() const {
  return ToHTMLVideoElement(GetNode());
}

void LayoutVideo::UpdateFromElement() {
  LayoutMedia::UpdateFromElement();
  UpdatePlayer();

  // If the DisplayMode of the video changed, then we need to paint.
  SetShouldDoFullPaintInvalidation();
}

void LayoutVideo::UpdatePlayer() {
  UpdateIntrinsicSize();

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

LayoutRect LayoutVideo::ReplacedContentRect() const {
  if (ShouldDisplayVideo()) {
    // Video codecs may need to restart from an I-frame when the output is
    // resized. Round size in advance to avoid 1px snap difference.
    // TODO(trchen): The way of rounding is different from LayoutEmbeddedContent
    // just to match existing behavior. This is probably a bug and We should
    // unify it with LayoutEmbeddedContent.
    return LayoutRect(PixelSnappedIntRect(ComputeObjectFit()));
  }
  // If we are displaying the poster image no pre-rounding is needed, but the
  // size of the image should be used for fitting instead.
  return ComputeObjectFit(&cached_image_size_);
}

bool LayoutVideo::SupportsAcceleratedRendering() const {
  return !!MediaElement()->PlatformLayer();
}

static const LayoutBlock* LayoutObjectPlaceholder(
    const LayoutObject* layout_object) {
  LayoutObject* parent = layout_object->Parent();
  if (!parent)
    return nullptr;

  LayoutFullScreen* full_screen =
      parent->IsLayoutFullScreen() ? ToLayoutFullScreen(parent) : nullptr;
  if (!full_screen)
    return nullptr;

  return full_screen->Placeholder();
}

LayoutUnit LayoutVideo::OffsetLeft(const Element* parent) const {
  if (const LayoutBlock* block = LayoutObjectPlaceholder(this))
    return block->OffsetLeft(parent);
  return LayoutMedia::OffsetLeft(parent);
}

LayoutUnit LayoutVideo::OffsetTop(const Element* parent) const {
  if (const LayoutBlock* block = LayoutObjectPlaceholder(this))
    return block->OffsetTop(parent);
  return LayoutMedia::OffsetTop(parent);
}

LayoutUnit LayoutVideo::OffsetWidth() const {
  if (const LayoutBlock* block = LayoutObjectPlaceholder(this))
    return block->OffsetWidth();
  return LayoutMedia::OffsetWidth();
}

LayoutUnit LayoutVideo::OffsetHeight() const {
  if (const LayoutBlock* block = LayoutObjectPlaceholder(this))
    return block->OffsetHeight();
  return LayoutMedia::OffsetHeight();
}

CompositingReasons LayoutVideo::AdditionalCompositingReasons() const {
  HTMLMediaElement* element = ToHTMLMediaElement(GetNode());
  if (element->IsFullscreen() && element->UsesOverlayFullscreenVideo())
    return kCompositingReasonVideo;

  if (ShouldDisplayVideo() && SupportsAcceleratedRendering())
    return kCompositingReasonVideo;

  return kCompositingReasonNone;
}

}  // namespace blink
