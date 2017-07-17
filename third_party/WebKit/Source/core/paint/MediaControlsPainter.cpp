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
 * OF LIABILITY, WHETHER IN CONTRACTg, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/paint/MediaControlsPainter.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/html/HTMLDivElement.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/TimeRanges.h"
#include "core/html/media/MediaControls.h"
#include "core/layout/LayoutBox.h"
#include "core/paint/PaintInfo.h"
#include "core/style/ComputedStyle.h"
#include "platform/graphics/Gradient.h"
#include "platform/graphics/GraphicsContext.h"

namespace blink {

namespace {

const double kCurrentTimeBufferedDelta = 1.0;

typedef WTF::HashMap<const char*, Image*> MediaControlImageMap;
MediaControlImageMap* g_media_control_image_map = 0;

// Slider thumb sizes, shard between time and volume.
const int kMediaSliderThumbTouchWidth = 36;  // Touch zone size.
const int kMediaSliderThumbTouchHeight = 48;
const int kMediaSliderThumbPaintWidth = 12;  // Painted area.
const int kMediaSliderThumbPaintHeight = 12;

// Overlay play button size. If this changes, it must also be changed in
// core/html/shadow/MediaControls.cpp.
const int kMediaOverlayPlayButtonWidth = 48;
const int kMediaOverlayPlayButtonHeight = 48;

// Alpha for disabled elements.
const float kDisabledAlpha = 0.4;

const HTMLMediaElement* ToParentMediaElement(const Node* node) {
  if (!node)
    return nullptr;
  const Node* media_node = node->OwnerShadowHost();
  if (!media_node)
    return nullptr;
  if (!IsHTMLMediaElement(media_node))
    return nullptr;

  return ToHTMLMediaElement(media_node);
}

const HTMLMediaElement* ToParentMediaElement(
    const LayoutObject& layout_object) {
  return ToParentMediaElement(layout_object.GetNode());
}

}  // anonymous namespace

static Image* PlatformResource(const char* name) {
  if (!g_media_control_image_map)
    g_media_control_image_map = new MediaControlImageMap();
  if (Image* image = g_media_control_image_map->at(name))
    return image;
  if (Image* image = Image::LoadPlatformResource(name).LeakRef()) {
    g_media_control_image_map->Set(name, image);
    return image;
  }
  NOTREACHED();
  return 0;
}

static bool HasSource(const HTMLMediaElement* media_element) {
  return media_element->getNetworkState() != HTMLMediaElement::kNetworkEmpty &&
         media_element->getNetworkState() != HTMLMediaElement::kNetworkNoSource;
}

static FloatRect AdjustRectForPadding(IntRect rect,
                                      const LayoutObject* object) {
  FloatRect adjusted_rect(rect);

  if (!object)
    return adjusted_rect;

  // TODO(liberato): make this more elegant, crbug.com/598861 .
  if (const ComputedStyle* style = object->Style()) {
    const float padding_left = style->PaddingLeft().GetFloatValue();
    const float padding_top = style->PaddingTop().GetFloatValue();
    const float padding_right = style->PaddingRight().GetFloatValue();
    const float padding_bottom = style->PaddingBottom().GetFloatValue();
    adjusted_rect = FloatRect(rect.X() + padding_left, rect.Y() + padding_top,
                              rect.Width() - padding_left - padding_right,
                              rect.Height() - padding_top - padding_bottom);
  }

  return adjusted_rect;
}

static bool PaintMediaButton(GraphicsContext& context,
                             const IntRect& rect,
                             Image* image,
                             const LayoutObject* object,
                             bool is_enabled) {
  FloatRect draw_rect = AdjustRectForPadding(rect, object);

  if (!is_enabled)
    context.BeginLayer(kDisabledAlpha);

  context.DrawImage(image, draw_rect);

  if (!is_enabled)
    context.EndLayer();

  return true;
}

static bool PaintMediaButton(GraphicsContext& context,
                             const IntRect& rect,
                             Image* image,
                             bool is_enabled = true) {
  return PaintMediaButton(context, rect, image, 0, is_enabled);
}

bool MediaControlsPainter::PaintMediaMuteButton(const LayoutObject& object,
                                                const PaintInfo& paint_info,
                                                const IntRect& rect) {
  const HTMLMediaElement* media_element = ToParentMediaElement(object);
  if (!media_element)
    return false;

  static Image* sound_not_muted = PlatformResource("mediaplayerSoundNotMuted");
  static Image* sound_muted = PlatformResource("mediaplayerSoundMuted");

  if (!HasSource(media_element) || !media_element->HasAudio()) {
    return PaintMediaButton(paint_info.context, rect, sound_muted, &object,
                            false);
  }

  if (media_element->muted() || media_element->volume() <= 0)
    return PaintMediaButton(paint_info.context, rect, sound_muted, &object,
                            true);

  return PaintMediaButton(paint_info.context, rect, sound_not_muted, &object,
                          true);
}

bool MediaControlsPainter::PaintMediaPlayButton(const LayoutObject& object,
                                                const PaintInfo& paint_info,
                                                const IntRect& rect) {
  const HTMLMediaElement* media_element = ToParentMediaElement(object);
  if (!media_element)
    return false;

  static Image* media_play = PlatformResource("mediaplayerPlay");
  static Image* media_pause = PlatformResource("mediaplayerPause");

  // Draw the regular play button grayed out.
  if (!HasSource(media_element))
    return PaintMediaButton(paint_info.context, rect, media_play, &object,
                            false);

  Image* image = media_element->paused() ? media_play : media_pause;
  return PaintMediaButton(paint_info.context, rect, image, &object, true);
}

bool MediaControlsPainter::PaintMediaOverlayPlayButton(
    const LayoutObject& object,
    const PaintInfo& paint_info,
    const IntRect& rect) {
  const HTMLMediaElement* media_element = ToParentMediaElement(object);
  if (!media_element)
    return false;

  if (!HasSource(media_element) || !media_element->paused())
    return false;

  static Image* media_overlay_play = PlatformResource("mediaplayerOverlayPlay");

  IntRect button_rect(rect);

  // Overlay play button covers the entire player, so center and draw a
  // smaller button.  Center in the entire element.
  // TODO(liberato): object.enclosingBox()?
  const LayoutBox* box = media_element->GetLayoutObject()->EnclosingBox();
  if (!box)
    return false;
  int media_height = box->PixelSnappedHeight();

  int media_panel_height = 0;
  if (media_element->GetMediaControls()) {
    if (LayoutObject* object =
            media_element->GetMediaControls()->PanelLayoutObject()) {
      if (object->IsBox()) {
        media_panel_height =
            AdjustLayoutUnitForAbsoluteZoom(ToLayoutBox(object)->ClientHeight(),
                                            *ToLayoutBox(object))
                .Round();
      }
    }
  }

  button_rect.SetX(rect.Center().X() - kMediaOverlayPlayButtonWidth / 2);
  button_rect.SetY(rect.Center().Y() - kMediaOverlayPlayButtonHeight / 2 +
                   (media_height - rect.Height() - media_panel_height) / 2);
  button_rect.SetWidth(kMediaOverlayPlayButtonWidth);
  button_rect.SetHeight(kMediaOverlayPlayButtonHeight);

  return PaintMediaButton(paint_info.context, button_rect, media_overlay_play);
}

static void PaintRoundedSliderBackground(const IntRect& rect,
                                         const ComputedStyle& style,
                                         GraphicsContext& context,
                                         Color slider_background_color) {
  float border_radius = rect.Height() / 2;
  FloatSize radii(border_radius, border_radius);

  context.FillRoundedRect(FloatRoundedRect(rect, radii, radii, radii, radii),
                          slider_background_color);
}

bool MediaControlsPainter::PaintMediaRemotingCastIcon(
    const LayoutObject& object,
    const PaintInfo& paintInfo,
    const IntRect& rect) {
  const HTMLMediaElement* media_element = ToParentMediaElement(object);
  if (!media_element)
    return false;
  static Image* cast_icon = PlatformResource("mediaRemotingCastIcon");

  return PaintMediaButton(paintInfo.context, rect, cast_icon);
}

static void PaintSliderRangeHighlight(const IntRect& rect,
                                      const ComputedStyle& style,
                                      GraphicsContext& context,
                                      int start_position,
                                      int end_position,
                                      Color start_color,
                                      Color end_color) {
  // Calculate border radius; need to avoid being smaller than half the slider
  // height because of https://bugs.webkit.org/show_bug.cgi?id=30143.
  float border_radius = rect.Height() / 2.0f;
  FloatSize radii(border_radius, border_radius);

  // Calculate highlight rectangle and edge dimensions.
  int start_offset = start_position;
  int end_offset = rect.Width() - end_position;
  int range_width = end_position - start_position;

  if (range_width <= 0)
    return;

  // Make sure the range width is bigger than border radius at the edges to
  // retain rounded corners.
  if (start_offset < border_radius && range_width < border_radius)
    range_width = border_radius;
  if (end_offset < border_radius && range_width < border_radius)
    range_width = border_radius;

  // Set rectangle to highlight range.
  IntRect highlight_rect = rect;
  highlight_rect.Move(start_offset, 0);
  highlight_rect.SetWidth(range_width);

  // Don't bother drawing an empty area.
  if (highlight_rect.IsEmpty())
    return;

  // Calculate white-grey gradient.
  FloatPoint slider_top_left = highlight_rect.Location();
  FloatPoint slider_bottom_left = slider_top_left;
  slider_bottom_left.Move(0, highlight_rect.Height());
  RefPtr<Gradient> gradient =
      Gradient::CreateLinear(slider_top_left, slider_bottom_left);
  gradient->AddColorStop(0.0, start_color);
  gradient->AddColorStop(1.0, end_color);

  // Fill highlight rectangle with gradient, potentially rounded if on left or
  // right edge.
  PaintFlags gradient_flags(context.FillFlags());
  gradient->ApplyToFlags(gradient_flags, SkMatrix::I());

  if (start_offset < border_radius && end_offset < border_radius) {
    context.DrawRRect(
        FloatRoundedRect(highlight_rect, radii, radii, radii, radii),
        gradient_flags);
  } else if (start_offset < border_radius) {
    context.DrawRRect(FloatRoundedRect(highlight_rect, radii, FloatSize(0, 0),
                                       radii, FloatSize(0, 0)),
                      gradient_flags);
  } else if (end_offset < border_radius) {
    context.DrawRRect(FloatRoundedRect(highlight_rect, FloatSize(0, 0), radii,
                                       FloatSize(0, 0), radii),
                      gradient_flags);
  } else {
    context.DrawRect(highlight_rect, gradient_flags);
  }
}

bool MediaControlsPainter::PaintMediaSlider(const LayoutObject& object,
                                            const PaintInfo& paint_info,
                                            const IntRect& rect) {
  const HTMLMediaElement* media_element = ToParentMediaElement(object);
  if (!media_element)
    return false;

  GraphicsContext& context = paint_info.context;

  // Should we paint the slider partially transparent?
  bool draw_ui_grayed = !HasSource(media_element);
  if (draw_ui_grayed)
    context.BeginLayer(kDisabledAlpha);

  PaintMediaSliderInternal(object, paint_info, rect);

  if (draw_ui_grayed)
    context.EndLayer();

  return true;
}

void MediaControlsPainter::PaintMediaSliderInternal(const LayoutObject& object,
                                                    const PaintInfo& paint_info,
                                                    const IntRect& rect) {
  const HTMLMediaElement* media_element = ToParentMediaElement(object);
  if (!media_element)
    return;

  const ComputedStyle& style = object.StyleRef();
  GraphicsContext& context = paint_info.context;

  // Paint the slider bar in the "no data buffered" state.
  PaintRoundedSliderBackground(rect, style, context, Color(0xda, 0xda, 0xda));

  // Draw the buffered range. Since the element may have multiple buffered
  // ranges and it'd be distracting/'busy' to show all of them, show only the
  // buffered range containing the current play head.
  TimeRanges* buffered_time_ranges = media_element->buffered();
  float duration = media_element->duration();
  float current_time = media_element->currentTime();
  if (std::isnan(duration) || std::isinf(duration) || !duration ||
      std::isnan(current_time))
    return;

  for (unsigned i = 0; i < buffered_time_ranges->length(); ++i) {
    float start = buffered_time_ranges->start(i, ASSERT_NO_EXCEPTION);
    float end = buffered_time_ranges->end(i, ASSERT_NO_EXCEPTION);
    // The delta is there to avoid corner cases when buffered
    // ranges is out of sync with current time because of
    // asynchronous media pipeline and current time caching in
    // HTMLMediaElement.
    // This is related to https://www.w3.org/Bugs/Public/show_bug.cgi?id=28125
    // FIXME: Remove this workaround when WebMediaPlayer
    // has an asynchronous pause interface.
    if (std::isnan(start) || std::isnan(end) ||
        start > current_time + kCurrentTimeBufferedDelta || end < current_time)
      continue;
    int start_position = int(start * rect.Width() / duration);
    int current_position = int(current_time * rect.Width() / duration);
    int end_position = int(end * rect.Width() / duration);

    // Draw highlight before current time.
    Color start_color = Color(0x42, 0x85, 0xf4);
    Color end_color = Color(0x42, 0x85, 0xf4);

    if (current_position > start_position) {
      PaintSliderRangeHighlight(rect, style, context, start_position,
                                current_position, start_color, end_color);
    }

    // Draw dark grey highlight after current time.
    start_color = end_color = Color(0x5a, 0x5a, 0x5a);

    if (end_position > current_position) {
      PaintSliderRangeHighlight(rect, style, context, current_position,
                                end_position, start_color, end_color);
    }
    return;
  }
}

void MediaControlsPainter::AdjustMediaSliderThumbPaintSize(
    const IntRect& rect,
    const ComputedStyle& style,
    IntRect& rect_out) {
  // Adjust the rectangle to be centered, the right size for the image.
  // We do this because it's quite hard to get the thumb touch target
  // to match.  So, we provide the touch target size with
  // adjustMediaSliderThumbSize(), and scale it back when we paint.
  rect_out = rect;

  const float zoom_level = style.EffectiveZoom();
  const float zoomed_paint_width = kMediaSliderThumbPaintWidth * zoom_level;
  const float zoomed_paint_height = kMediaSliderThumbPaintHeight * zoom_level;

  rect_out.SetX(rect.Center().X() - zoomed_paint_width / 2);
  rect_out.SetY(rect.Center().Y() - zoomed_paint_height / 2);
  rect_out.SetWidth(zoomed_paint_width);
  rect_out.SetHeight(zoomed_paint_height);
}

bool MediaControlsPainter::PaintMediaSliderThumb(const LayoutObject& object,
                                                 const PaintInfo& paint_info,
                                                 const IntRect& rect) {
  if (!object.GetNode())
    return false;

  const HTMLMediaElement* media_element =
      ToParentMediaElement(object.GetNode()->OwnerShadowHost());
  if (!media_element)
    return false;

  if (!HasSource(media_element))
    return true;

  static Image* media_slider_thumb = PlatformResource("mediaplayerSliderThumb");
  IntRect paint_rect;
  const ComputedStyle& style = object.StyleRef();
  AdjustMediaSliderThumbPaintSize(rect, style, paint_rect);
  return PaintMediaButton(paint_info.context, paint_rect, media_slider_thumb);
}

bool MediaControlsPainter::PaintMediaVolumeSlider(const LayoutObject& object,
                                                  const PaintInfo& paint_info,
                                                  const IntRect& rect) {
  const HTMLMediaElement* media_element = ToParentMediaElement(object);
  if (!media_element)
    return false;

  GraphicsContext& context = paint_info.context;
  const ComputedStyle& style = object.StyleRef();

  // Paint the slider bar.
  PaintRoundedSliderBackground(rect, style, context, Color(0x5a, 0x5a, 0x5a));

  // Calculate volume position for white background rectangle.
  float volume = media_element->volume();
  if (std::isnan(volume) || volume < 0)
    return true;
  if (volume > 1)
    volume = 1;
  if (!HasSource(media_element) || !media_element->HasAudio() ||
      media_element->muted()) {
    volume = 0;
  }

  // Calculate the position relative to the center of the thumb.
  const float fill_width = volume * rect.Width();
  static const Color kColor = Color(0x42, 0x85, 0xf4);  // blue
  PaintSliderRangeHighlight(rect, style, context, 0.0, fill_width, kColor,
                            kColor);

  return true;
}

bool MediaControlsPainter::PaintMediaVolumeSliderThumb(
    const LayoutObject& object,
    const PaintInfo& paint_info,
    const IntRect& rect) {
  if (!object.GetNode())
    return false;

  const HTMLMediaElement* media_element =
      ToParentMediaElement(object.GetNode()->OwnerShadowHost());
  if (!media_element)
    return false;

  if (!HasSource(media_element) || !media_element->HasAudio())
    return true;

  static Image* media_volume_slider_thumb =
      PlatformResource("mediaplayerVolumeSliderThumb");

  IntRect paint_rect;
  const ComputedStyle& style = object.StyleRef();
  AdjustMediaSliderThumbPaintSize(rect, style, paint_rect);
  return PaintMediaButton(paint_info.context, paint_rect,
                          media_volume_slider_thumb);
}

bool MediaControlsPainter::PaintMediaFullscreenButton(
    const LayoutObject& object,
    const PaintInfo& paint_info,
    const IntRect& rect) {
  const HTMLMediaElement* media_element = ToParentMediaElement(object);
  if (!media_element)
    return false;

  static Image* media_enter_fullscreen_button =
      PlatformResource("mediaplayerEnterFullscreen");
  static Image* media_exit_fullscreen_button =
      PlatformResource("mediaplayerExitFullscreen");

  Image* image = media_element->IsFullscreen() ? media_exit_fullscreen_button
                                               : media_enter_fullscreen_button;
  const bool is_enabled = HasSource(media_element);
  return PaintMediaButton(paint_info.context, rect, image, &object, is_enabled);
}

bool MediaControlsPainter::PaintMediaToggleClosedCaptionsButton(
    const LayoutObject& object,
    const PaintInfo& paint_info,
    const IntRect& rect) {
  const HTMLMediaElement* media_element = ToParentMediaElement(object);
  if (!media_element)
    return false;

  static Image* media_closed_caption_button =
      PlatformResource("mediaplayerClosedCaption");
  static Image* media_closed_caption_button_disabled =
      PlatformResource("mediaplayerClosedCaptionDisabled");

  Image* image = media_element->TextTracksVisible()
                     ? media_closed_caption_button
                     : media_closed_caption_button_disabled;
  const bool is_enabled = media_element->HasClosedCaptions();
  return PaintMediaButton(paint_info.context, rect, image, &object, is_enabled);
}

bool MediaControlsPainter::PaintMediaCastButton(const LayoutObject& object,
                                                const PaintInfo& paint_info,
                                                const IntRect& rect) {
  const HTMLMediaElement* media_element = ToParentMediaElement(object);
  if (!media_element)
    return false;

  static Image* media_cast_on = PlatformResource("mediaplayerCastOn");
  static Image* media_cast_off = PlatformResource("mediaplayerCastOff");
  // To ensure that the overlaid cast button is visible when overlaid on pale
  // videos we use a different version of it for the overlaid case with a
  // semi-opaque background.
  static Image* media_overlay_cast_off =
      PlatformResource("mediaplayerOverlayCastOff");

  bool is_enabled = media_element->HasRemoteRoutes();
  bool playing_remotely = media_element->IsPlayingRemotely();
  bool native_controls = media_element->ShouldShowControls();

  if (playing_remotely) {
    if (native_controls) {
      return PaintMediaButton(paint_info.context, rect, media_cast_on, &object,
                              is_enabled);
    }
    return PaintMediaButton(paint_info.context, rect, media_cast_on);
  }

  if (native_controls) {
    return PaintMediaButton(paint_info.context, rect, media_cast_off, &object,
                            is_enabled);
  }

  return PaintMediaButton(paint_info.context, rect, media_overlay_cast_off);
}

bool MediaControlsPainter::PaintMediaTrackSelectionCheckmark(
    const LayoutObject& object,
    const PaintInfo& paint_info,
    const IntRect& rect) {
  const HTMLMediaElement* media_element = ToParentMediaElement(object);
  if (!media_element)
    return false;

  static Image* media_track_selection_checkmark =
      PlatformResource("mediaplayerTrackSelectionCheckmark");
  return PaintMediaButton(paint_info.context, rect,
                          media_track_selection_checkmark);
}

bool MediaControlsPainter::PaintMediaClosedCaptionsIcon(
    const LayoutObject& object,
    const PaintInfo& paint_info,
    const IntRect& rect) {
  const HTMLMediaElement* media_element = ToParentMediaElement(object);
  if (!media_element)
    return false;

  static Image* media_closed_captions_icon =
      PlatformResource("mediaplayerClosedCaptionsIcon");
  return PaintMediaButton(paint_info.context, rect, media_closed_captions_icon);
}

bool MediaControlsPainter::PaintMediaSubtitlesIcon(const LayoutObject& object,
                                                   const PaintInfo& paint_info,
                                                   const IntRect& rect) {
  const HTMLMediaElement* media_element = ToParentMediaElement(object);
  if (!media_element)
    return false;

  static Image* media_subtitles_icon =
      PlatformResource("mediaplayerSubtitlesIcon");
  return PaintMediaButton(paint_info.context, rect, media_subtitles_icon);
}

bool MediaControlsPainter::PaintMediaOverflowMenu(const LayoutObject& object,
                                                  const PaintInfo& paint_info,
                                                  const IntRect& rect) {
  const HTMLMediaElement* media_element = ToParentMediaElement(object);
  if (!media_element)
    return false;

  static Image* media_overflow_button =
      PlatformResource("mediaplayerOverflowMenu");
  return PaintMediaButton(paint_info.context, rect, media_overflow_button,
                          &object, true);
}

bool MediaControlsPainter::PaintMediaDownloadIcon(const LayoutObject& object,
                                                  const PaintInfo& paint_info,
                                                  const IntRect& rect) {
  const HTMLMediaElement* media_element = ToParentMediaElement(object);
  if (!media_element)
    return false;

  bool is_enabled = HasSource(media_element);

  static Image* media_download_icon =
      PlatformResource("mediaplayerDownloadIcon");
  return PaintMediaButton(paint_info.context, rect, media_download_icon,
                          &object, is_enabled);
}

void MediaControlsPainter::AdjustMediaSliderThumbSize(ComputedStyle& style) {
  const float zoom_level = style.EffectiveZoom();

  style.SetWidth(Length(
      static_cast<int>(kMediaSliderThumbTouchWidth * zoom_level), kFixed));
  style.SetHeight(Length(
      static_cast<int>(kMediaSliderThumbTouchHeight * zoom_level), kFixed));
}

}  // namespace blink
