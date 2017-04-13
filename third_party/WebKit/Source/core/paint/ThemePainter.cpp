/**
 * This file is part of the theme implementation for form controls in WebCore.
 *
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "core/paint/ThemePainter.h"

#include "core/InputTypeNames.h"
#include "core/frame/FrameView.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLDataListElement.h"
#include "core/html/HTMLDataListOptionsCollection.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/layout/LayoutTheme.h"
#include "core/layout/LayoutView.h"
#include "core/paint/MediaControlsPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/style/ComputedStyle.h"
#include "platform/Theme.h"
#include "platform/graphics/GraphicsContextStateSaver.h"
#include "platform/graphics/paint/PaintCanvas.h"
#include "public/platform/Platform.h"
#include "public/platform/WebFallbackThemeEngine.h"
#include "public/platform/WebRect.h"

// The methods in this file are shared by all themes on every platform.

namespace blink {

static WebFallbackThemeEngine::State GetWebFallbackThemeState(
    const LayoutObject& o) {
  if (!LayoutTheme::IsEnabled(o))
    return WebFallbackThemeEngine::kStateDisabled;
  if (LayoutTheme::IsPressed(o))
    return WebFallbackThemeEngine::kStatePressed;
  if (LayoutTheme::IsHovered(o))
    return WebFallbackThemeEngine::kStateHover;

  return WebFallbackThemeEngine::kStateNormal;
}

ThemePainter::ThemePainter() {}

bool ThemePainter::Paint(const LayoutObject& o,
                         const PaintInfo& paint_info,
                         const IntRect& r) {
  ControlPart part = o.StyleRef().Appearance();

  if (LayoutTheme::GetTheme().ShouldUseFallbackTheme(o.StyleRef()))
    return PaintUsingFallbackTheme(o, paint_info, r);

  if (part == kButtonPart && o.GetNode()) {
    UseCounter::Count(o.GetDocument(),
                      UseCounter::kCSSValueAppearanceButtonRendered);
    if (isHTMLAnchorElement(o.GetNode())) {
      UseCounter::Count(o.GetDocument(),
                        UseCounter::kCSSValueAppearanceButtonForAnchor);
    } else if (isHTMLButtonElement(o.GetNode())) {
      UseCounter::Count(o.GetDocument(),
                        UseCounter::kCSSValueAppearanceButtonForButton);
    } else if (isHTMLInputElement(o.GetNode()) &&
               toHTMLInputElement(o.GetNode())->IsTextButton()) {
      // Text buttons (type=button, reset, submit) has
      // -webkit-appearance:push-button by default.
      UseCounter::Count(o.GetNode()->GetDocument(),
                        UseCounter::kCSSValueAppearanceButtonForOtherButtons);
    }
  }

  // Call the appropriate paint method based off the appearance value.
  switch (part) {
    case kCheckboxPart:
      return PaintCheckbox(o, paint_info, r);
    case kRadioPart:
      return PaintRadio(o, paint_info, r);
    case kPushButtonPart:
    case kSquareButtonPart:
    case kButtonPart:
      return PaintButton(o, paint_info, r);
    case kInnerSpinButtonPart:
      return PaintInnerSpinButton(o, paint_info, r);
    case kMenulistPart:
      return PaintMenuList(o, paint_info, r);
    case kMeterPart:
      return true;
    case kProgressBarPart:
      return PaintProgressBar(o, paint_info, r);
    case kSliderHorizontalPart:
    case kSliderVerticalPart:
      return PaintSliderTrack(o, paint_info, r);
    case kSliderThumbHorizontalPart:
    case kSliderThumbVerticalPart:
      return PaintSliderThumb(o, paint_info, r);
    case kMediaEnterFullscreenButtonPart:
    case kMediaExitFullscreenButtonPart:
      return MediaControlsPainter::PaintMediaFullscreenButton(o, paint_info, r);
    case kMediaPlayButtonPart:
      return MediaControlsPainter::PaintMediaPlayButton(o, paint_info, r);
    case kMediaOverlayPlayButtonPart:
      return MediaControlsPainter::PaintMediaOverlayPlayButton(o, paint_info,
                                                               r);
    case kMediaMuteButtonPart:
      return MediaControlsPainter::PaintMediaMuteButton(o, paint_info, r);
    case kMediaToggleClosedCaptionsButtonPart:
      return MediaControlsPainter::PaintMediaToggleClosedCaptionsButton(
          o, paint_info, r);
    case kMediaSliderPart:
      return MediaControlsPainter::PaintMediaSlider(o, paint_info, r);
    case kMediaSliderThumbPart:
      return MediaControlsPainter::PaintMediaSliderThumb(o, paint_info, r);
    case kMediaVolumeSliderContainerPart:
      return true;
    case kMediaVolumeSliderPart:
      return MediaControlsPainter::PaintMediaVolumeSlider(o, paint_info, r);
    case kMediaVolumeSliderThumbPart:
      return MediaControlsPainter::PaintMediaVolumeSliderThumb(o, paint_info,
                                                               r);
    case kMediaTimeRemainingPart:
    case kMediaCurrentTimePart:
    case kMediaControlsBackgroundPart:
      return true;
    case kMediaCastOffButtonPart:
    case kMediaOverlayCastOffButtonPart:
      return MediaControlsPainter::PaintMediaCastButton(o, paint_info, r);
    case kMediaTrackSelectionCheckmarkPart:
      return MediaControlsPainter::PaintMediaTrackSelectionCheckmark(
          o, paint_info, r);
    case kMediaClosedCaptionsIconPart:
      return MediaControlsPainter::PaintMediaClosedCaptionsIcon(o, paint_info,
                                                                r);
    case kMediaSubtitlesIconPart:
      return MediaControlsPainter::PaintMediaSubtitlesIcon(o, paint_info, r);
    case kMediaOverflowMenuButtonPart:
      return MediaControlsPainter::PaintMediaOverflowMenu(o, paint_info, r);
    case kMediaDownloadIconPart:
      return MediaControlsPainter::PaintMediaDownloadIcon(o, paint_info, r);
    case kMenulistButtonPart:
    case kTextFieldPart:
    case kTextAreaPart:
      return true;
    case kSearchFieldPart:
      return PaintSearchField(o, paint_info, r);
    case kSearchFieldCancelButtonPart:
      return PaintSearchFieldCancelButton(o, paint_info, r);
    case kMediaRemotingCastIconPart:
      return MediaControlsPainter::PaintMediaRemotingCastIcon(o, paint_info, r);
    default:
      break;
  }

  return true;  // We don't support the appearance, so let the normal
                // background/border paint.
}

bool ThemePainter::PaintBorderOnly(const LayoutObject& o,
                                   const PaintInfo& paint_info,
                                   const IntRect& r) {
  // Call the appropriate paint method based off the appearance value.
  switch (o.StyleRef().Appearance()) {
    case kTextFieldPart:
      UseCounter::Count(o.GetDocument(),
                        UseCounter::kCSSValueAppearanceTextFieldRendered);
      if (isHTMLInputElement(o.GetNode())) {
        HTMLInputElement* input = toHTMLInputElement(o.GetNode());
        if (input->type() == InputTypeNames::search)
          UseCounter::Count(o.GetDocument(),
                            UseCounter::kCSSValueAppearanceTextFieldForSearch);
        else if (input->IsTextField())
          UseCounter::Count(
              o.GetDocument(),
              UseCounter::kCSSValueAppearanceTextFieldForTextField);
      }
      return PaintTextField(o, paint_info, r);
    case kTextAreaPart:
      return PaintTextArea(o, paint_info, r);
    case kMenulistButtonPart:
    case kSearchFieldPart:
    case kListboxPart:
      return true;
    case kCheckboxPart:
    case kRadioPart:
    case kPushButtonPart:
    case kSquareButtonPart:
    case kButtonPart:
    case kMenulistPart:
    case kMeterPart:
    case kProgressBarPart:
    case kSliderHorizontalPart:
    case kSliderVerticalPart:
    case kSliderThumbHorizontalPart:
    case kSliderThumbVerticalPart:
    case kSearchFieldCancelButtonPart:
    default:
      break;
  }

  return false;
}

bool ThemePainter::PaintDecorations(const LayoutObject& o,
                                    const PaintInfo& paint_info,
                                    const IntRect& r) {
  // Call the appropriate paint method based off the appearance value.
  switch (o.StyleRef().Appearance()) {
    case kMenulistButtonPart:
      return PaintMenuListButton(o, paint_info, r);
    case kTextFieldPart:
    case kTextAreaPart:
    case kCheckboxPart:
    case kRadioPart:
    case kPushButtonPart:
    case kSquareButtonPart:
    case kButtonPart:
    case kMenulistPart:
    case kMeterPart:
    case kProgressBarPart:
    case kSliderHorizontalPart:
    case kSliderVerticalPart:
    case kSliderThumbHorizontalPart:
    case kSliderThumbVerticalPart:
    case kSearchFieldPart:
    case kSearchFieldCancelButtonPart:
    default:
      break;
  }

  return false;
}

void ThemePainter::PaintSliderTicks(const LayoutObject& o,
                                    const PaintInfo& paint_info,
                                    const IntRect& rect) {
  Node* node = o.GetNode();
  if (!isHTMLInputElement(node))
    return;

  HTMLInputElement* input = toHTMLInputElement(node);
  if (input->type() != InputTypeNames::range ||
      !input->UserAgentShadowRoot()->HasChildren())
    return;

  HTMLDataListElement* data_list = input->DataList();
  if (!data_list)
    return;

  double min = input->Minimum();
  double max = input->Maximum();
  ControlPart part = o.StyleRef().Appearance();
  // We don't support ticks on alternate sliders like MediaVolumeSliders.
  if (part != kSliderHorizontalPart && part != kSliderVerticalPart)
    return;
  bool is_horizontal = part == kSliderHorizontalPart;

  IntSize thumb_size;
  LayoutObject* thumb_layout_object =
      input->UserAgentShadowRoot()
          ->GetElementById(ShadowElementNames::SliderThumb())
          ->GetLayoutObject();
  if (thumb_layout_object) {
    const ComputedStyle& thumb_style = thumb_layout_object->StyleRef();
    int thumb_width = thumb_style.Width().IntValue();
    int thumb_height = thumb_style.Height().IntValue();
    thumb_size.SetWidth(is_horizontal ? thumb_width : thumb_height);
    thumb_size.SetHeight(is_horizontal ? thumb_height : thumb_width);
  }

  IntSize tick_size = LayoutTheme::GetTheme().SliderTickSize();
  float zoom_factor = o.StyleRef().EffectiveZoom();
  FloatRect tick_rect;
  int tick_region_side_margin = 0;
  int tick_region_width = 0;
  IntRect track_bounds;
  LayoutObject* track_layout_object =
      input->UserAgentShadowRoot()
          ->GetElementById(ShadowElementNames::SliderTrack())
          ->GetLayoutObject();
  // We can ignoring transforms because transform is handled by the graphics
  // context.
  if (track_layout_object)
    track_bounds =
        track_layout_object->AbsoluteBoundingBoxRectIgnoringTransforms();
  IntRect slider_bounds = o.AbsoluteBoundingBoxRectIgnoringTransforms();

  // Make position relative to the transformed ancestor element.
  track_bounds.SetX(track_bounds.X() - slider_bounds.X() + rect.X());
  track_bounds.SetY(track_bounds.Y() - slider_bounds.Y() + rect.Y());

  if (is_horizontal) {
    tick_rect.SetWidth(floor(tick_size.Width() * zoom_factor));
    tick_rect.SetHeight(floor(tick_size.Height() * zoom_factor));
    tick_rect.SetY(
        floor(rect.Y() + rect.Height() / 2.0 +
              LayoutTheme::GetTheme().SliderTickOffsetFromTrackCenter() *
                  zoom_factor));
    tick_region_side_margin =
        track_bounds.X() +
        (thumb_size.Width() - tick_size.Width() * zoom_factor) / 2.0;
    tick_region_width = track_bounds.Width() - thumb_size.Width();
  } else {
    tick_rect.SetWidth(floor(tick_size.Height() * zoom_factor));
    tick_rect.SetHeight(floor(tick_size.Width() * zoom_factor));
    tick_rect.SetX(
        floor(rect.X() + rect.Width() / 2.0 +
              LayoutTheme::GetTheme().SliderTickOffsetFromTrackCenter() *
                  zoom_factor));
    tick_region_side_margin =
        track_bounds.Y() +
        (thumb_size.Width() - tick_size.Width() * zoom_factor) / 2.0;
    tick_region_width = track_bounds.Height() - thumb_size.Width();
  }
  HTMLDataListOptionsCollection* options = data_list->options();
  for (unsigned i = 0; HTMLOptionElement* option_element = options->Item(i);
       i++) {
    String value = option_element->value();
    if (option_element->IsDisabledFormControl() || value.IsEmpty())
      continue;
    if (!input->IsValidValue(value))
      continue;
    double parsed_value =
        ParseToDoubleForNumberType(input->SanitizeValue(value));
    double tick_fraction = (parsed_value - min) / (max - min);
    double tick_ratio = is_horizontal && o.StyleRef().IsLeftToRightDirection()
                            ? tick_fraction
                            : 1.0 - tick_fraction;
    double tick_position =
        round(tick_region_side_margin + tick_region_width * tick_ratio);
    if (is_horizontal)
      tick_rect.SetX(tick_position);
    else
      tick_rect.SetY(tick_position);
    paint_info.context.FillRect(tick_rect, o.ResolveColor(CSSPropertyColor));
  }
}

bool ThemePainter::PaintUsingFallbackTheme(const LayoutObject& o,
                                           const PaintInfo& i,
                                           const IntRect& r) {
  ControlPart part = o.StyleRef().Appearance();
  switch (part) {
    case kCheckboxPart:
      return PaintCheckboxUsingFallbackTheme(o, i, r);
    case kRadioPart:
      return PaintRadioUsingFallbackTheme(o, i, r);
    default:
      break;
  }
  return true;
}

bool ThemePainter::PaintCheckboxUsingFallbackTheme(const LayoutObject& o,
                                                   const PaintInfo& i,
                                                   const IntRect& r) {
  WebFallbackThemeEngine::ExtraParams extra_params;
  PaintCanvas* canvas = i.context.Canvas();
  extra_params.button.checked = LayoutTheme::IsChecked(o);
  extra_params.button.indeterminate = LayoutTheme::IsIndeterminate(o);

  float zoom_level = o.StyleRef().EffectiveZoom();
  GraphicsContextStateSaver state_saver(i.context);
  IntRect unzoomed_rect = r;
  if (zoom_level != 1) {
    unzoomed_rect.SetWidth(unzoomed_rect.Width() / zoom_level);
    unzoomed_rect.SetHeight(unzoomed_rect.Height() / zoom_level);
    i.context.Translate(unzoomed_rect.X(), unzoomed_rect.Y());
    i.context.Scale(zoom_level, zoom_level);
    i.context.Translate(-unzoomed_rect.X(), -unzoomed_rect.Y());
  }

  Platform::Current()->FallbackThemeEngine()->Paint(
      canvas, WebFallbackThemeEngine::kPartCheckbox,
      GetWebFallbackThemeState(o), WebRect(unzoomed_rect), &extra_params);
  return false;
}

bool ThemePainter::PaintRadioUsingFallbackTheme(const LayoutObject& o,
                                                const PaintInfo& i,
                                                const IntRect& r) {
  WebFallbackThemeEngine::ExtraParams extra_params;
  WebCanvas* canvas = i.context.Canvas();
  extra_params.button.checked = LayoutTheme::IsChecked(o);
  extra_params.button.indeterminate = LayoutTheme::IsIndeterminate(o);

  float zoom_level = o.StyleRef().EffectiveZoom();
  GraphicsContextStateSaver state_saver(i.context);
  IntRect unzoomed_rect = r;
  if (zoom_level != 1) {
    unzoomed_rect.SetWidth(unzoomed_rect.Width() / zoom_level);
    unzoomed_rect.SetHeight(unzoomed_rect.Height() / zoom_level);
    i.context.Translate(unzoomed_rect.X(), unzoomed_rect.Y());
    i.context.Scale(zoom_level, zoom_level);
    i.context.Translate(-unzoomed_rect.X(), -unzoomed_rect.Y());
  }

  Platform::Current()->FallbackThemeEngine()->Paint(
      canvas, WebFallbackThemeEngine::kPartRadio, GetWebFallbackThemeState(o),
      WebRect(unzoomed_rect), &extra_params);
  return false;
}

}  // namespace blink
