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

#include "third_party/blink/renderer/core/paint/theme_painter.h"

#include "build/build_config.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/renderer/core/frame/local_frame_view.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_options_collection.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/html_select_element.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_theme.h"
#include "third_party/blink/renderer/core/layout/layout_view.h"
#include "third_party/blink/renderer/core/paint/fallback_theme.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/platform/graphics/graphics_context_state_saver.h"
#include "third_party/blink/renderer/platform/graphics/paint/paint_canvas.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "ui/base/ui_base_features.h"
#include "ui/native_theme/native_theme.h"

// The methods in this file are shared by all themes on every platform.

namespace blink {

namespace {

ui::NativeTheme::State GetFallbackThemeState(const Node* node) {
  if (!LayoutTheme::IsEnabled(node))
    return ui::NativeTheme::kDisabled;
  if (LayoutTheme::IsPressed(node))
    return ui::NativeTheme::kPressed;
  if (LayoutTheme::IsHovered(node))
    return ui::NativeTheme::kHovered;

  return ui::NativeTheme::kNormal;
}

static ui::NativeTheme::ColorScheme ToNativeColorScheme(
    WebColorScheme color_scheme) {
  switch (color_scheme) {
    case WebColorScheme::kLight:
      return ui::NativeTheme::ColorScheme::kLight;
    case WebColorScheme::kDark:
      return ui::NativeTheme::ColorScheme::kDark;
  }
}

bool IsMultipleFieldsTemporalInput(const AtomicString& type) {
#if !defined(OS_ANDROID)
  return type == input_type_names::kDate ||
         type == input_type_names::kDatetimeLocal ||
         type == input_type_names::kMonth || type == input_type_names::kTime ||
         type == input_type_names::kWeek;
#else
  return false;
#endif
}

}  // anonymous namespace

ThemePainter::ThemePainter() = default;

#define COUNT_APPEARANCE(doc, feature) \
  doc.CountUse(WebFeature::kCSSValueAppearance##feature##Rendered)

void CountAppearanceTextFieldPart(const Node* node) {
  DCHECK(node);
  if (auto* input = DynamicTo<HTMLInputElement>(node)) {
    const AtomicString& type = input->type();
    if (type == input_type_names::kSearch) {
      UseCounter::Count(node->GetDocument(),
                        WebFeature::kCSSValueAppearanceTextFieldForSearch);
    } else if (input->IsTextField()) {
      UseCounter::Count(node->GetDocument(),
                        WebFeature::kCSSValueAppearanceTextFieldForTextField);
    } else if (IsMultipleFieldsTemporalInput(type)) {
      UseCounter::Count(
          node->GetDocument(),
          WebFeature::kCSSValueAppearanceTextFieldForTemporalRendered);
    }
  }
}

// Returns true; Needs CSS painting and/or PaintBorderOnly().
bool ThemePainter::Paint(const LayoutObject& o,
                         const PaintInfo& paint_info,
                         const IntRect& r) {
  const Node* node = o.GetNode();
  Document& doc = o.GetDocument();
  const ComputedStyle& style = o.StyleRef();
  ControlPart part = o.StyleRef().EffectiveAppearance();
  // LayoutTheme::AdjustAppearanceWithElementType() ensures |node| is a
  // non-null Element.
  DCHECK(node);
  DCHECK_NE(part, kNoControlPart);

  if (LayoutTheme::GetTheme().ShouldUseFallbackTheme(style))
    return PaintUsingFallbackTheme(node, style, paint_info, r);

  if (part == kButtonPart) {
    if (IsA<HTMLButtonElement>(node)) {
      UseCounter::Count(doc, WebFeature::kCSSValueAppearanceButtonForButton);
    } else if (IsA<HTMLInputElement>(node) &&
               To<HTMLInputElement>(node)->IsTextButton()) {
      // Text buttons (type=button, reset, submit) has
      // -webkit-appearance:push-button by default.
      UseCounter::Count(doc,
                        WebFeature::kCSSValueAppearanceButtonForOtherButtons);
    } else if (IsA<HTMLInputElement>(node) &&
               To<HTMLInputElement>(node)->type() == input_type_names::kColor) {
      //  'button' for input[type=color], of which default appearance is
      // 'square-button', is not deprecated.
    }
  }

  // Call the appropriate paint method based off the appearance value.
  switch (part) {
    case kCheckboxPart: {
      COUNT_APPEARANCE(doc, Checkbox);
      return PaintCheckbox(node, o.GetDocument(), style, paint_info, r);
    }
    case kRadioPart: {
      COUNT_APPEARANCE(doc, Radio);
      return PaintRadio(node, o.GetDocument(), style, paint_info, r);
    }
    case kPushButtonPart: {
      COUNT_APPEARANCE(doc, PushButton);
      return PaintButton(node, o.GetDocument(), style, paint_info, r);
    }
    case kSquareButtonPart: {
      COUNT_APPEARANCE(doc, SquareButton);
      return PaintButton(node, o.GetDocument(), style, paint_info, r);
    }
    case kButtonPart:
      // UseCounter for this is handled at the beginning of the function.
      return PaintButton(node, o.GetDocument(), style, paint_info, r);
    case kInnerSpinButtonPart: {
      COUNT_APPEARANCE(doc, InnerSpinButton);
      return PaintInnerSpinButton(node, style, paint_info, r);
    }
    case kMenulistPart:
      COUNT_APPEARANCE(doc, MenuList);
      return PaintMenuList(node, o.GetDocument(), style, paint_info, r);
    case kMeterPart:
      return true;
    case kProgressBarPart:
      COUNT_APPEARANCE(doc, ProgressBar);
      // Note that |-webkit-appearance: progress-bar| works only for <progress>.
      return PaintProgressBar(o, paint_info, r);
    case kSliderHorizontalPart: {
      COUNT_APPEARANCE(doc, SliderHorizontal);
      return PaintSliderTrack(o, paint_info, r);
    }
    case kSliderVerticalPart: {
      COUNT_APPEARANCE(doc, SliderVertical);
      return PaintSliderTrack(o, paint_info, r);
    }
    case kSliderThumbHorizontalPart: {
      COUNT_APPEARANCE(doc, SliderThumbHorizontal);
      return PaintSliderThumb(node, style, paint_info, r);
    }
    case kSliderThumbVerticalPart: {
      COUNT_APPEARANCE(doc, SliderThumbVertical);
      return PaintSliderThumb(node, style, paint_info, r);
    }
    case kMediaSliderPart:
    case kMediaSliderThumbPart:
    case kMediaVolumeSliderPart:
    case kMediaVolumeSliderThumbPart:
      return true;
    case kMenulistButtonPart:
      return true;
    case kTextFieldPart:
      if (!features::IsFormControlsRefreshEnabled()) {
        return true;
      }
      CountAppearanceTextFieldPart(node);
      return PaintTextField(node, style, paint_info, r);
    case kTextAreaPart:
      if (!features::IsFormControlsRefreshEnabled()) {
        return true;
      }
      COUNT_APPEARANCE(doc, TextArea);
      return PaintTextArea(node, style, paint_info, r);
    case kSearchFieldPart: {
      COUNT_APPEARANCE(doc, SearchField);
      return PaintSearchField(node, style, paint_info, r);
    }
    case kSearchFieldCancelButtonPart: {
      COUNT_APPEARANCE(doc, SearchCancel);
      return PaintSearchFieldCancelButton(o, paint_info, r);
    }
    case kListboxPart:
      return true;
    default:
      break;
  }

  // We don't support the appearance, so let the normal background/border paint.
  return true;
}

// Returns true; Needs CSS border painting.
bool ThemePainter::PaintBorderOnly(const Node* node,
                                   const ComputedStyle& style,
                                   const PaintInfo& paint_info,
                                   const IntRect& r) {
  // Call the appropriate paint method based off the appearance value.
  switch (style.EffectiveAppearance()) {
    case kTextFieldPart:
      if (features::IsFormControlsRefreshEnabled()) {
        return false;
      }
      CountAppearanceTextFieldPart(node);
      return PaintTextField(node, style, paint_info, r);
    case kTextAreaPart:
      if (features::IsFormControlsRefreshEnabled()) {
        return false;
      }
      COUNT_APPEARANCE(node->GetDocument(), TextArea);
      return PaintTextArea(node, style, paint_info, r);
    case kMenulistButtonPart:
    case kSearchFieldPart:
    case kListboxPart:
      return true;
    case kButtonPart:
    case kCheckboxPart:
    case kInnerSpinButtonPart:
    case kMenulistPart:
    case kProgressBarPart:
    case kPushButtonPart:
    case kRadioPart:
    case kSearchFieldCancelButtonPart:
    case kSliderHorizontalPart:
    case kSliderThumbHorizontalPart:
    case kSliderThumbVerticalPart:
    case kSliderVerticalPart:
    case kSquareButtonPart:
      // Supported appearance values don't need CSS border painting.
      return false;
    default:
      UseCounter::Count(
          node->GetDocument(),
          WebFeature::kCSSValueAppearanceNoImplementationSkipBorder);
      // TODO(tkent): Should do CSS border painting for non-supported
      // appearance values.
      return false;
  }

  return false;
}

bool ThemePainter::PaintDecorations(const Node* node,
                                    const Document& document,
                                    const ComputedStyle& style,
                                    const PaintInfo& paint_info,
                                    const IntRect& r) {
  // Call the appropriate paint method based off the appearance value.
  switch (style.EffectiveAppearance()) {
    case kMenulistButtonPart:
      COUNT_APPEARANCE(document, MenuListButton);
      return PaintMenuListButton(node, document, style, paint_info, r);
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

#undef COUNT_APPEARANCE

void ThemePainter::PaintSliderTicks(const LayoutObject& o,
                                    const PaintInfo& paint_info,
                                    const IntRect& rect) {
  auto* input = DynamicTo<HTMLInputElement>(o.GetNode());
  if (!input)
    return;

  if (input->type() != input_type_names::kRange ||
      !input->UserAgentShadowRoot()->HasChildren())
    return;

  HTMLDataListElement* data_list = input->DataList();
  if (!data_list)
    return;

  double min = input->Minimum();
  double max = input->Maximum();
  ControlPart part = o.StyleRef().EffectiveAppearance();
  // We don't support ticks on alternate sliders like MediaVolumeSliders.
  if (part != kSliderHorizontalPart && part != kSliderVerticalPart)
    return;
  bool is_horizontal = part == kSliderHorizontalPart;

  IntSize thumb_size;
  LayoutObject* thumb_layout_object =
      input->UserAgentShadowRoot()
          ->getElementById(shadow_element_names::SliderThumb())
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
          ->getElementById(shadow_element_names::SliderTrack())
          ->GetLayoutObject();
  if (track_layout_object)
    track_bounds = track_layout_object->FirstFragment().VisualRect();

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
    paint_info.context.FillRect(tick_rect,
                                o.ResolveColor(GetCSSPropertyColor()));
  }
}

bool ThemePainter::PaintUsingFallbackTheme(const Node* node,
                                           const ComputedStyle& style,
                                           const PaintInfo& paint_info,
                                           const IntRect& paint_rect) {
  ControlPart part = style.EffectiveAppearance();
  switch (part) {
    case kCheckboxPart:
      return PaintCheckboxUsingFallbackTheme(node, style, paint_info,
                                             paint_rect);
    case kRadioPart:
      return PaintRadioUsingFallbackTheme(node, style, paint_info, paint_rect);
    default:
      break;
  }
  return true;
}

bool ThemePainter::PaintCheckboxUsingFallbackTheme(const Node* node,
                                                   const ComputedStyle& style,
                                                   const PaintInfo& paint_info,
                                                   const IntRect& paint_rect) {
  ui::NativeTheme::ExtraParams extra_params;
  extra_params.button.checked = LayoutTheme::IsChecked(node);
  extra_params.button.indeterminate = LayoutTheme::IsIndeterminate(node);

  float zoom_level = style.EffectiveZoom();
  GraphicsContextStateSaver state_saver(paint_info.context);
  IntRect unzoomed_rect = paint_rect;
  if (zoom_level != 1) {
    unzoomed_rect.SetWidth(unzoomed_rect.Width() / zoom_level);
    unzoomed_rect.SetHeight(unzoomed_rect.Height() / zoom_level);
    paint_info.context.Translate(unzoomed_rect.X(), unzoomed_rect.Y());
    paint_info.context.Scale(zoom_level, zoom_level);
    paint_info.context.Translate(-unzoomed_rect.X(), -unzoomed_rect.Y());
  }

  GetFallbackTheme().Paint(
      paint_info.context.Canvas(), ui::NativeTheme::kCheckbox,
      GetFallbackThemeState(node), unzoomed_rect, extra_params,
      ToNativeColorScheme(style.UsedColorScheme()));
  return false;
}

bool ThemePainter::PaintRadioUsingFallbackTheme(const Node* node,
                                                const ComputedStyle& style,
                                                const PaintInfo& paint_info,
                                                const IntRect& paint_rect) {
  ui::NativeTheme::ExtraParams extra_params;
  extra_params.button.checked = LayoutTheme::IsChecked(node);

  float zoom_level = style.EffectiveZoom();
  GraphicsContextStateSaver state_saver(paint_info.context);
  IntRect unzoomed_rect = paint_rect;
  if (zoom_level != 1) {
    unzoomed_rect.SetWidth(unzoomed_rect.Width() / zoom_level);
    unzoomed_rect.SetHeight(unzoomed_rect.Height() / zoom_level);
    paint_info.context.Translate(unzoomed_rect.X(), unzoomed_rect.Y());
    paint_info.context.Scale(zoom_level, zoom_level);
    paint_info.context.Translate(-unzoomed_rect.X(), -unzoomed_rect.Y());
  }

  GetFallbackTheme().Paint(paint_info.context.Canvas(), ui::NativeTheme::kRadio,
                           GetFallbackThemeState(node), unzoomed_rect,
                           extra_params,
                           ToNativeColorScheme(style.UsedColorScheme()));
  return false;
}

}  // namespace blink
