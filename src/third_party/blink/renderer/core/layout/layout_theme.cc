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

#include "third_party/blink/renderer/core/layout/layout_theme.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/web_rect.h"
#include "third_party/blink/public/web/blink.h"
#include "third_party/blink/renderer/core/css_value_keywords.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/editing/frame_selection.h"
#include "third_party/blink/renderer/core/fileapi/file_list.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/frame/settings.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_element.h"
#include "third_party/blink/renderer/core/html/forms/html_data_list_options_collection.h"
#include "third_party/blink/renderer/core/html/forms/html_form_control_element.h"
#include "third_party/blink/renderer/core/html/forms/html_input_element.h"
#include "third_party/blink/renderer/core/html/forms/html_option_element.h"
#include "third_party/blink/renderer/core/html/forms/spin_button_element.h"
#include "third_party/blink/renderer/core/html/forms/text_control_inner_elements.h"
#include "third_party/blink/renderer/core/html/html_collection.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/core/html/shadow/shadow_element_names.h"
#include "third_party/blink/renderer/core/html_names.h"
#include "third_party/blink/renderer/core/input_type_names.h"
#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/layout/layout_theme_mobile.h"
#include "third_party/blink/renderer/core/page/focus_controller.h"
#include "third_party/blink/renderer/core/page/page.h"
#include "third_party/blink/renderer/core/paint/fallback_theme.h"
#include "third_party/blink/renderer/core/style/computed_style.h"
#include "third_party/blink/renderer/core/style/computed_style_initial_values.h"
#include "third_party/blink/renderer/platform/file_metadata.h"
#include "third_party/blink/renderer/platform/fonts/font_selector.h"
#include "third_party/blink/renderer/platform/fonts/string_truncator.h"
#include "third_party/blink/renderer/platform/graphics/touch_action.h"
#include "third_party/blink/renderer/platform/runtime_enabled_features.h"
#include "third_party/blink/renderer/platform/text/platform_locale.h"
#include "third_party/blink/renderer/platform/web_test_support.h"
#include "third_party/blink/renderer/platform/wtf/text/string_builder.h"
#include "ui/native_theme/native_theme.h"

// The methods in this file are shared by all themes on every platform.

namespace blink {

// Wrapper function defined in WebKit.h
void SetMockThemeEnabledForTest(bool value) {
  WebTestSupport::SetMockThemeEnabledForTest(value);
  LayoutTheme::GetTheme().DidChangeThemeEngine();
}

LayoutTheme& LayoutTheme::GetTheme() {
  if (RuntimeEnabledFeatures::MobileLayoutThemeEnabled()) {
    DEFINE_STATIC_REF(LayoutTheme, layout_theme_mobile,
                      (LayoutThemeMobile::Create()));
    return *layout_theme_mobile;
  }
  return NativeTheme();
}

LayoutTheme::LayoutTheme() : has_custom_focus_ring_color_(false) {}

void LayoutTheme::AdjustStyle(ComputedStyle& style, Element* e) {
  DCHECK(style.HasAppearance());

  // Force inline and table display styles to be inline-block (except for table-
  // which is block)
  ControlPart part = style.Appearance();
  if (style.Display() == EDisplay::kInline ||
      style.Display() == EDisplay::kInlineTable ||
      style.Display() == EDisplay::kTableRowGroup ||
      style.Display() == EDisplay::kTableHeaderGroup ||
      style.Display() == EDisplay::kTableFooterGroup ||
      style.Display() == EDisplay::kTableRow ||
      style.Display() == EDisplay::kTableColumnGroup ||
      style.Display() == EDisplay::kTableColumn ||
      style.Display() == EDisplay::kTableCell ||
      style.Display() == EDisplay::kTableCaption)
    style.SetDisplay(EDisplay::kInlineBlock);
  else if (style.Display() == EDisplay::kListItem ||
           style.Display() == EDisplay::kTable)
    style.SetDisplay(EDisplay::kBlock);

  if (IsControlStyled(style)) {
    if (part == kMenulistPart) {
      style.SetAppearance(kMenulistButtonPart);
      part = kMenulistButtonPart;
    } else {
      style.SetAppearance(kNoControlPart);
      return;
    }
  }

  if (ShouldUseFallbackTheme(style)) {
    AdjustStyleUsingFallbackTheme(style);
    return;
  }

  AdjustControlPartStyle(style);

  // Call the appropriate style adjustment method based off the appearance
  // value.
  switch (style.Appearance()) {
    case kMenulistPart:
      return AdjustMenuListStyle(style, e);
    case kMenulistButtonPart:
      return AdjustMenuListButtonStyle(style, e);
    case kSliderHorizontalPart:
    case kSliderVerticalPart:
    case kMediaSliderPart:
    case kMediaVolumeSliderPart:
      return AdjustSliderContainerStyle(style, e);
    case kSliderThumbHorizontalPart:
    case kSliderThumbVerticalPart:
      return AdjustSliderThumbStyle(style);
    case kSearchFieldPart:
      return AdjustSearchFieldStyle(style);
    case kSearchFieldCancelButtonPart:
      return AdjustSearchFieldCancelButtonStyle(style);
    default:
      break;
  }
}

String LayoutTheme::ExtraDefaultStyleSheet() {
  return g_empty_string;
}

String LayoutTheme::ExtraQuirksStyleSheet() {
  return String();
}

String LayoutTheme::ExtraFullscreenStyleSheet() {
  return String();
}

Color LayoutTheme::ActiveSelectionBackgroundColor() const {
  return PlatformActiveSelectionBackgroundColor().BlendWithWhite();
}

Color LayoutTheme::InactiveSelectionBackgroundColor() const {
  return PlatformInactiveSelectionBackgroundColor().BlendWithWhite();
}

Color LayoutTheme::ActiveSelectionForegroundColor() const {
  return PlatformActiveSelectionForegroundColor();
}

Color LayoutTheme::InactiveSelectionForegroundColor() const {
  return PlatformInactiveSelectionForegroundColor();
}

Color LayoutTheme::ActiveListBoxSelectionBackgroundColor() const {
  return PlatformActiveListBoxSelectionBackgroundColor();
}

Color LayoutTheme::InactiveListBoxSelectionBackgroundColor() const {
  return PlatformInactiveListBoxSelectionBackgroundColor();
}

Color LayoutTheme::ActiveListBoxSelectionForegroundColor() const {
  return PlatformActiveListBoxSelectionForegroundColor();
}

Color LayoutTheme::InactiveListBoxSelectionForegroundColor() const {
  return PlatformInactiveListBoxSelectionForegroundColor();
}

Color LayoutTheme::PlatformSpellingMarkerUnderlineColor() const {
  return Color(255, 0, 0);
}

Color LayoutTheme::PlatformGrammarMarkerUnderlineColor() const {
  return Color(192, 192, 192);
}

Color LayoutTheme::PlatformActiveSpellingMarkerHighlightColor() const {
  return Color(255, 0, 0, 102);
}

Color LayoutTheme::PlatformActiveSelectionBackgroundColor() const {
  // Use a blue color by default if the platform theme doesn't define anything.
  return Color(0, 0, 255);
}

Color LayoutTheme::PlatformActiveSelectionForegroundColor() const {
  // Use a white color by default if the platform theme doesn't define anything.
  return Color::kWhite;
}

Color LayoutTheme::PlatformInactiveSelectionBackgroundColor() const {
  // Use a grey color by default if the platform theme doesn't define anything.
  // This color matches Firefox's inactive color.
  return Color(176, 176, 176);
}

Color LayoutTheme::PlatformInactiveSelectionForegroundColor() const {
  // Use a black color by default.
  return Color::kBlack;
}

Color LayoutTheme::PlatformActiveListBoxSelectionBackgroundColor() const {
  return PlatformActiveSelectionBackgroundColor();
}

Color LayoutTheme::PlatformActiveListBoxSelectionForegroundColor() const {
  return PlatformActiveSelectionForegroundColor();
}

Color LayoutTheme::PlatformInactiveListBoxSelectionBackgroundColor() const {
  return PlatformInactiveSelectionBackgroundColor();
}

Color LayoutTheme::PlatformInactiveListBoxSelectionForegroundColor() const {
  return PlatformInactiveSelectionForegroundColor();
}

LayoutUnit LayoutTheme::BaselinePositionAdjustment(
    const ComputedStyle& style) const {
  return LayoutUnit();
}

bool LayoutTheme::IsControlContainer(ControlPart appearance) const {
  // There are more leaves than this, but we'll patch this function as we add
  // support for more controls.
  return appearance != kCheckboxPart && appearance != kRadioPart;
}

bool LayoutTheme::IsControlStyled(const ComputedStyle& style) const {
  switch (style.Appearance()) {
    case kPushButtonPart:
    case kSquareButtonPart:
    case kButtonPart:
    case kProgressBarPart:
      return style.HasAuthorBackground() || style.HasAuthorBorder();

    case kMenulistPart:
    case kSearchFieldPart:
    case kTextAreaPart:
    case kTextFieldPart:
      return style.HasAuthorBackground() || style.HasAuthorBorder() ||
             style.BoxShadow();

    default:
      return false;
  }
}

bool LayoutTheme::ShouldDrawDefaultFocusRing(const Node* node,
                                             const ComputedStyle& style) const {
  if (ThemeDrawsFocusRing(style))
    return false;
  if (!node)
    return true;
  if (!style.HasAppearance() && !node->IsLink())
    return true;
  // We can't use LayoutTheme::isFocused because outline:auto might be
  // specified to non-:focus rulesets.
  if (node->IsFocused() && !node->ShouldHaveFocusAppearance())
    return false;
  return true;
}

bool LayoutTheme::ControlStateChanged(const Node* node,
                                      const ComputedStyle& style,
                                      ControlState state) const {
  if (!style.HasAppearance())
    return false;

  // Default implementation assumes the controls don't respond to changes in
  // :hover state
  if (state == kHoverControlState && !SupportsHover(style))
    return false;

  // Assume pressed state is only responded to if the control is enabled.
  if (state == kPressedControlState && !IsEnabled(node))
    return false;

  return true;
}

ControlStates LayoutTheme::ControlStatesForNode(const Node* node,
                                                const ComputedStyle& style) {
  ControlStates result = 0;
  if (IsHovered(node)) {
    result |= kHoverControlState;
    if (IsSpinUpButtonPartHovered(node))
      result |= kSpinUpControlState;
  }
  if (IsPressed(node)) {
    result |= kPressedControlState;
    if (IsSpinUpButtonPartPressed(node))
      result |= kSpinUpControlState;
  }
  if (IsFocused(node) && style.OutlineStyleIsAuto())
    result |= kFocusControlState;
  if (IsEnabled(node))
    result |= kEnabledControlState;
  if (IsChecked(node))
    result |= kCheckedControlState;
  if (IsReadOnlyControl(node))
    result |= kReadOnlyControlState;
  if (!IsActive(node))
    result |= kWindowInactiveControlState;
  if (IsIndeterminate(node))
    result |= kIndeterminateControlState;
  return result;
}

bool LayoutTheme::IsActive(const Node* node) {
  if (!node)
    return false;

  Page* page = node->GetDocument().GetPage();
  if (!page)
    return false;

  return page->GetFocusController().IsActive();
}

bool LayoutTheme::IsChecked(const Node* node) {
  if (auto* input = ToHTMLInputElementOrNull(node))
    return input->ShouldAppearChecked();
  return false;
}

bool LayoutTheme::IsIndeterminate(const Node* node) {
  if (auto* input = ToHTMLInputElementOrNull(node))
    return input->ShouldAppearIndeterminate();
  return false;
}

bool LayoutTheme::IsEnabled(const Node* node) {
  if (!node || !node->IsElementNode())
    return true;
  return !ToElement(node)->IsDisabledFormControl();
}

bool LayoutTheme::IsFocused(const Node* node) {
  if (!node)
    return false;

  node = node->FocusDelegate();
  Document& document = node->GetDocument();
  LocalFrame* frame = document.GetFrame();
  return node == document.FocusedElement() && node->IsFocused() &&
         node->ShouldHaveFocusAppearance() && frame &&
         frame->Selection().FrameIsFocusedAndActive();
}

bool LayoutTheme::IsPressed(const Node* node) {
  if (!node)
    return false;
  return node->IsActive();
}

bool LayoutTheme::IsSpinUpButtonPartPressed(const Node* node) {
  if (!node || !node->IsActive() || !node->IsElementNode() ||
      !ToElement(node)->IsSpinButtonElement())
    return false;
  const SpinButtonElement* element = ToSpinButtonElement(node);
  return element->GetUpDownState() == SpinButtonElement::kUp;
}

bool LayoutTheme::IsReadOnlyControl(const Node* node) {
  if (!node || !node->IsElementNode() ||
      !ToElement(node)->IsFormControlElement())
    return false;
  const HTMLFormControlElement* element = ToHTMLFormControlElement(node);
  return element->IsReadOnly();
}

bool LayoutTheme::IsHovered(const Node* node) {
  if (!node)
    return false;
  if (!node->IsElementNode() || !ToElement(node)->IsSpinButtonElement())
    return node->IsHovered();
  const SpinButtonElement* element = ToSpinButtonElement(node);
  return element->IsHovered() &&
         element->GetUpDownState() != SpinButtonElement::kIndeterminate;
}

bool LayoutTheme::IsSpinUpButtonPartHovered(const Node* node) {
  if (!node || !node->IsElementNode() ||
      !ToElement(node)->IsSpinButtonElement())
    return false;
  const SpinButtonElement* element = ToSpinButtonElement(node);
  return element->GetUpDownState() == SpinButtonElement::kUp;
}

void LayoutTheme::AdjustCheckboxStyle(ComputedStyle& style) const {
  // A summary of the rules for checkbox designed to match WinIE:
  // width/height - honored (WinIE actually scales its control for small widths,
  // but lets it overflow for small heights.)
  // font-size - not honored (control has no text), but we use it to decide
  // which control size to use.
  SetCheckboxSize(style);

  // padding - not honored by WinIE, needs to be removed.
  style.ResetPadding();

  // border - honored by WinIE, but looks terrible (just paints in the control
  // box and turns off the Windows XP theme) for now, we will not honor it.
  style.ResetBorder();
}

void LayoutTheme::AdjustRadioStyle(ComputedStyle& style) const {
  // A summary of the rules for checkbox designed to match WinIE:
  // width/height - honored (WinIE actually scales its control for small widths,
  // but lets it overflow for small heights.)
  // font-size - not honored (control has no text), but we use it to decide
  // which control size to use.
  SetRadioSize(style);

  // padding - not honored by WinIE, needs to be removed.
  style.ResetPadding();

  // border - honored by WinIE, but looks terrible (just paints in the control
  // box and turns off the Windows XP theme) for now, we will not honor it.
  style.ResetBorder();
}

void LayoutTheme::AdjustButtonStyle(ComputedStyle& style) const {}

void LayoutTheme::AdjustInnerSpinButtonStyle(ComputedStyle&) const {}

void LayoutTheme::AdjustMenuListStyle(ComputedStyle&, Element*) const {}

TimeDelta LayoutTheme::AnimationRepeatIntervalForProgressBar() const {
  return TimeDelta();
}

TimeDelta LayoutTheme::AnimationDurationForProgressBar() const {
  return TimeDelta();
}

bool LayoutTheme::ShouldHaveSpinButton(HTMLInputElement* input_element) const {
  return input_element->IsSteppable() &&
         input_element->type() != input_type_names::kRange;
}

void LayoutTheme::AdjustMenuListButtonStyle(ComputedStyle&, Element*) const {}

void LayoutTheme::AdjustSliderContainerStyle(ComputedStyle& style,
                                             Element* e) const {
  if (e && (e->ShadowPseudoId() == "-webkit-media-slider-container" ||
            e->ShadowPseudoId() == "-webkit-slider-container")) {
    if (style.Appearance() == kSliderVerticalPart) {
      style.SetTouchAction(TouchAction::kTouchActionPanX);
      style.SetAppearance(kNoControlPart);
    } else {
      style.SetTouchAction(TouchAction::kTouchActionPanY);
      style.SetAppearance(kNoControlPart);
    }
  }
}

void LayoutTheme::AdjustSliderThumbStyle(ComputedStyle& style) const {
  AdjustSliderThumbSize(style);
}

void LayoutTheme::AdjustSliderThumbSize(ComputedStyle&) const {}

void LayoutTheme::AdjustSearchFieldStyle(ComputedStyle&) const {}

void LayoutTheme::AdjustSearchFieldCancelButtonStyle(ComputedStyle&) const {}

void LayoutTheme::PlatformColorsDidChange() {
  Page::PlatformColorsChanged();
}

void LayoutTheme::SetCaretBlinkInterval(TimeDelta interval) {
  caret_blink_interval_ = interval;
}

TimeDelta LayoutTheme::CaretBlinkInterval() const {
  // Disable the blinking caret in web test mode, as it introduces
  // a race condition for the pixel tests. http://b/1198440
  return WebTestSupport::IsRunningWebTest() ? TimeDelta()
                                            : caret_blink_interval_;
}

static FontDescription& GetCachedFontDescription(CSSValueID system_font_id) {
  DEFINE_STATIC_LOCAL(FontDescription, caption, ());
  DEFINE_STATIC_LOCAL(FontDescription, icon, ());
  DEFINE_STATIC_LOCAL(FontDescription, menu, ());
  DEFINE_STATIC_LOCAL(FontDescription, message_box, ());
  DEFINE_STATIC_LOCAL(FontDescription, small_caption, ());
  DEFINE_STATIC_LOCAL(FontDescription, status_bar, ());
  DEFINE_STATIC_LOCAL(FontDescription, webkit_mini_control, ());
  DEFINE_STATIC_LOCAL(FontDescription, webkit_small_control, ());
  DEFINE_STATIC_LOCAL(FontDescription, webkit_control, ());
  DEFINE_STATIC_LOCAL(FontDescription, default_description, ());
  switch (system_font_id) {
    case CSSValueID::kCaption:
      return caption;
    case CSSValueID::kIcon:
      return icon;
    case CSSValueID::kMenu:
      return menu;
    case CSSValueID::kMessageBox:
      return message_box;
    case CSSValueID::kSmallCaption:
      return small_caption;
    case CSSValueID::kStatusBar:
      return status_bar;
    case CSSValueID::kWebkitMiniControl:
      return webkit_mini_control;
    case CSSValueID::kWebkitSmallControl:
      return webkit_small_control;
    case CSSValueID::kWebkitControl:
      return webkit_control;
    case CSSValueID::kNone:
      return default_description;
    default:
      NOTREACHED();
      return default_description;
  }
}

void LayoutTheme::SystemFont(CSSValueID system_font_id,
                             FontDescription& font_description) {
  font_description = GetCachedFontDescription(system_font_id);
  if (font_description.IsAbsoluteSize())
    return;

  FontSelectionValue font_slope = NormalSlopeValue();
  FontSelectionValue font_weight = NormalWeightValue();
  float font_size = 0;
  AtomicString font_family;
  SystemFont(system_font_id, font_slope, font_weight, font_size, font_family);
  font_description.SetStyle(font_slope);
  font_description.SetWeight(font_weight);
  font_description.SetSpecifiedSize(font_size);
  font_description.SetIsAbsoluteSize(true);
  font_description.FirstFamily().SetFamily(font_family);
  font_description.SetGenericFamily(FontDescription::kNoFamily);
}

Color LayoutTheme::SystemColor(CSSValueID css_value_id) const {
  switch (css_value_id) {
    case CSSValueID::kActiveborder:
      return 0xFFFFFFFF;
    case CSSValueID::kActivecaption:
      return 0xFFCCCCCC;
    case CSSValueID::kAppworkspace:
      return 0xFFFFFFFF;
    case CSSValueID::kBackground:
      return 0xFF6363CE;
    case CSSValueID::kButtonface:
      return 0xFFC0C0C0;
    case CSSValueID::kButtonhighlight:
      return 0xFFDDDDDD;
    case CSSValueID::kButtonshadow:
      return 0xFF888888;
    case CSSValueID::kButtontext:
      return 0xFF000000;
    case CSSValueID::kCaptiontext:
      return 0xFF000000;
    case CSSValueID::kGraytext:
      return 0xFF808080;
    case CSSValueID::kHighlight:
      return 0xFFB5D5FF;
    case CSSValueID::kHighlighttext:
      return 0xFF000000;
    case CSSValueID::kInactiveborder:
      return 0xFFFFFFFF;
    case CSSValueID::kInactivecaption:
      return 0xFFFFFFFF;
    case CSSValueID::kInactivecaptiontext:
      return 0xFF7F7F7F;
    case CSSValueID::kInfobackground:
      return 0xFFFBFCC5;
    case CSSValueID::kInfotext:
      return 0xFF000000;
    case CSSValueID::kMenu:
      return 0xFFC0C0C0;
    case CSSValueID::kMenutext:
      return 0xFF000000;
    case CSSValueID::kScrollbar:
      return 0xFFFFFFFF;
    case CSSValueID::kText:
      return 0xFF000000;
    case CSSValueID::kThreeddarkshadow:
      return 0xFF666666;
    case CSSValueID::kThreedface:
      return 0xFFC0C0C0;
    case CSSValueID::kThreedhighlight:
      return 0xFFDDDDDD;
    case CSSValueID::kThreedlightshadow:
      return 0xFFC0C0C0;
    case CSSValueID::kThreedshadow:
      return 0xFF888888;
    case CSSValueID::kWindow:
      return 0xFFFFFFFF;
    case CSSValueID::kWindowframe:
      return 0xFFCCCCCC;
    case CSSValueID::kWindowtext:
      return 0xFF000000;
    case CSSValueID::kInternalActiveListBoxSelection:
      return ActiveListBoxSelectionBackgroundColor();
    case CSSValueID::kInternalActiveListBoxSelectionText:
      return ActiveListBoxSelectionForegroundColor();
    case CSSValueID::kInternalInactiveListBoxSelection:
      return InactiveListBoxSelectionBackgroundColor();
    case CSSValueID::kInternalInactiveListBoxSelectionText:
      return InactiveListBoxSelectionForegroundColor();
    default:
      break;
  }
  NOTREACHED();
  return Color();
}

Color LayoutTheme::PlatformTextSearchHighlightColor(bool active_match) const {
  if (active_match)
    return Color(255, 150, 50);  // Orange.
  return Color(255, 255, 0);     // Yellow.
}

Color LayoutTheme::PlatformTextSearchColor(bool active_match) const {
  return Color::kBlack;
}

Color LayoutTheme::TapHighlightColor() {
  return GetTheme().PlatformTapHighlightColor();
}

void LayoutTheme::SetCustomFocusRingColor(const Color& c) {
  custom_focus_ring_color_ = c;
  has_custom_focus_ring_color_ = true;
}

bool LayoutTheme::IsFocusRingOutset() const {
  return false;
}

Color LayoutTheme::FocusRingColor() const {
  return has_custom_focus_ring_color_ ? custom_focus_ring_color_
                                      : GetTheme().PlatformFocusRingColor();
}

String LayoutTheme::FileListNameForWidth(Locale& locale,
                                         const FileList* file_list,
                                         const Font& font,
                                         int width) const {
  if (width <= 0)
    return String();

  String string;
  if (file_list->IsEmpty()) {
    string =
        locale.QueryString(WebLocalizedString::kFileButtonNoFileSelectedLabel);
  } else if (file_list->length() == 1) {
    string = file_list->item(0)->name();
  } else {
    return StringTruncator::RightTruncate(
        locale.QueryString(WebLocalizedString::kMultipleFileUploadText,
                           locale.ConvertToLocalizedNumber(
                               String::Number(file_list->length()))),
        width, font);
  }

  return StringTruncator::CenterTruncate(string, width, font);
}

bool LayoutTheme::ShouldOpenPickerWithF4Key() const {
  return false;
}

bool LayoutTheme::SupportsCalendarPicker(const AtomicString& type) const {
  DCHECK(RuntimeEnabledFeatures::InputMultipleFieldsUIEnabled());
  return type == input_type_names::kDate ||
         type == input_type_names::kDatetime ||
         type == input_type_names::kDatetimeLocal ||
         type == input_type_names::kMonth || type == input_type_names::kWeek;
}

bool LayoutTheme::ShouldUseFallbackTheme(const ComputedStyle&) const {
  return false;
}

void LayoutTheme::AdjustStyleUsingFallbackTheme(ComputedStyle& style) {
  ControlPart part = style.Appearance();
  switch (part) {
    case kCheckboxPart:
      return AdjustCheckboxStyleUsingFallbackTheme(style);
    case kRadioPart:
      return AdjustRadioStyleUsingFallbackTheme(style);
    default:
      break;
  }
}

// static
void LayoutTheme::SetSizeIfAuto(ComputedStyle& style, const IntSize& size) {
  if (style.Width().IsIntrinsicOrAuto())
    style.SetWidth(Length::Fixed(size.Width()));
  if (style.Height().IsIntrinsicOrAuto())
    style.SetHeight(Length::Fixed(size.Height()));
}

// static
void LayoutTheme::SetMinimumSize(ComputedStyle& style,
                                 const LengthSize* part_size,
                                 const LengthSize* min_part_size) {
  DCHECK(part_size || min_part_size);
  // We only want to set a minimum size if no explicit size is specified, to
  // avoid overriding author intentions.
  if (part_size && style.MinWidth().IsIntrinsicOrAuto() &&
      style.Width().IsIntrinsicOrAuto())
    style.SetMinWidth(part_size->Width());
  else if (min_part_size && min_part_size->Width() != style.MinWidth())
    style.SetMinWidth(min_part_size->Width());
  if (part_size && style.MinHeight().IsIntrinsicOrAuto() &&
      style.Height().IsIntrinsicOrAuto())
    style.SetMinHeight(part_size->Height());
  else if (min_part_size && min_part_size->Height() != style.MinHeight())
    style.SetMinHeight(min_part_size->Height());
}

// static
void LayoutTheme::SetMinimumSizeIfAuto(ComputedStyle& style,
                                       const IntSize& size) {
  LengthSize length_size(Length::Fixed(size.Width()),
                         Length::Fixed(size.Height()));
  SetMinimumSize(style, &length_size);
}

void LayoutTheme::AdjustCheckboxStyleUsingFallbackTheme(
    ComputedStyle& style) const {
  // If the width and height are both specified, then we have nothing to do.
  if (!style.Width().IsIntrinsicOrAuto() && !style.Height().IsAuto())
    return;

  IntSize size(GetFallbackTheme().GetPartSize(ui::NativeTheme::kCheckbox,
                                              ui::NativeTheme::kNormal,
                                              ui::NativeTheme::ExtraParams()));
  float zoom_level = style.EffectiveZoom();
  size.SetWidth(size.Width() * zoom_level);
  size.SetHeight(size.Height() * zoom_level);
  SetMinimumSizeIfAuto(style, size);
  SetSizeIfAuto(style, size);

  // padding - not honored by WinIE, needs to be removed.
  style.ResetPadding();

  // border - honored by WinIE, but looks terrible (just paints in the control
  // box and turns off the Windows XP theme)
  // for now, we will not honor it.
  style.ResetBorder();
}

void LayoutTheme::AdjustRadioStyleUsingFallbackTheme(
    ComputedStyle& style) const {
  // If the width and height are both specified, then we have nothing to do.
  if (!style.Width().IsIntrinsicOrAuto() && !style.Height().IsAuto())
    return;

  IntSize size(GetFallbackTheme().GetPartSize(ui::NativeTheme::kRadio,
                                              ui::NativeTheme::kNormal,
                                              ui::NativeTheme::ExtraParams()));
  float zoom_level = style.EffectiveZoom();
  size.SetWidth(size.Width() * zoom_level);
  size.SetHeight(size.Height() * zoom_level);
  SetMinimumSizeIfAuto(style, size);
  SetSizeIfAuto(style, size);

  // padding - not honored by WinIE, needs to be removed.
  style.ResetPadding();

  // border - honored by WinIE, but looks terrible (just paints in the control
  // box and turns off the Windows XP theme)
  // for now, we will not honor it.
  style.ResetBorder();
}

Color LayoutTheme::RootElementColor(ColorScheme color_scheme) const {
  if (color_scheme == ColorScheme::kDark)
    return Color::kWhite;
  return ComputedStyleInitialValues::InitialColor();
}

LengthBox LayoutTheme::ControlPadding(ControlPart part,
                                      const FontDescription&,
                                      const Length& zoomed_box_top,
                                      const Length& zoomed_box_right,
                                      const Length& zoomed_box_bottom,
                                      const Length& zoomed_box_left,
                                      float) const {
  switch (part) {
    case kMenulistPart:
    case kMenulistButtonPart:
    case kCheckboxPart:
    case kRadioPart:
      return LengthBox(0);
    default:
      return LengthBox(zoomed_box_top, zoomed_box_right, zoomed_box_bottom,
                       zoomed_box_left);
  }
}

LengthBox LayoutTheme::ControlBorder(ControlPart part,
                                     const FontDescription&,
                                     const LengthBox& zoomed_box,
                                     float) const {
  switch (part) {
    case kPushButtonPart:
    case kMenulistPart:
    case kSearchFieldPart:
    case kCheckboxPart:
    case kRadioPart:
      return LengthBox(0);
    default:
      return zoomed_box;
  }
}

void LayoutTheme::AdjustControlPartStyle(ComputedStyle& style) {
  // Call the appropriate style adjustment method based off the appearance
  // value.
  switch (style.Appearance()) {
    case kCheckboxPart:
      return AdjustCheckboxStyle(style);
    case kRadioPart:
      return AdjustRadioStyle(style);
    case kPushButtonPart:
    case kSquareButtonPart:
    case kButtonPart:
      return AdjustButtonStyle(style);
    case kInnerSpinButtonPart:
      return AdjustInnerSpinButtonStyle(style);
    default:
      break;
  }
}

}  // namespace blink
