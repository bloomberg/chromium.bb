// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webthemeengine_impl_linux.h"

#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "ui/gfx/native_theme.h"

using WebKit::WebCanvas;
using WebKit::WebColor;
using WebKit::WebRect;

namespace webkit_glue {

static gfx::Rect WebRectToRect(const WebRect& rect) {
  return gfx::Rect(rect.x, rect.y, rect.width, rect.height);
}

static gfx::NativeTheme::Part NativeThemePart(
    WebKit::WebThemeEngine::Part part) {
  switch (part) {
    case WebKit::WebThemeEngine::PartScrollbarDownArrow:
      return gfx::NativeTheme::kScrollbarDownArrow;
    case WebKit::WebThemeEngine::PartScrollbarLeftArrow:
      return gfx::NativeTheme::kScrollbarLeftArrow;
    case WebKit::WebThemeEngine::PartScrollbarRightArrow:
      return gfx::NativeTheme::kScrollbarRightArrow;
    case WebKit::WebThemeEngine::PartScrollbarUpArrow:
      return gfx::NativeTheme::kScrollbarUpArrow;
    case WebKit::WebThemeEngine::PartScrollbarHorizontalThumb:
      return gfx::NativeTheme::kScrollbarHorizontalThumb;
    case WebKit::WebThemeEngine::PartScrollbarVerticalThumb:
      return gfx::NativeTheme::kScrollbarVerticalThumb;
    case WebKit::WebThemeEngine::PartScrollbarHorizontalTrack:
      return gfx::NativeTheme::kScrollbarHorizontalTrack;
    case WebKit::WebThemeEngine::PartScrollbarVerticalTrack:
      return gfx::NativeTheme::kScrollbarVerticalTrack;
    case WebKit::WebThemeEngine::PartCheckbox:
      return gfx::NativeTheme::kCheckbox;
    case WebKit::WebThemeEngine::PartRadio:
      return gfx::NativeTheme::kRadio;
    case WebKit::WebThemeEngine::PartButton:
      return gfx::NativeTheme::kPushButton;
    case WebKit::WebThemeEngine::PartTextField:
      return gfx::NativeTheme::kTextField;
    case WebKit::WebThemeEngine::PartMenuList:
      return gfx::NativeTheme::kMenuList;
    case WebKit::WebThemeEngine::PartSliderTrack:
      return gfx::NativeTheme::kSliderTrack;
    case WebKit::WebThemeEngine::PartSliderThumb:
      return gfx::NativeTheme::kSliderThumb;
    case WebKit::WebThemeEngine::PartInnerSpinButton:
      return gfx::NativeTheme::kInnerSpinButton;
    case WebKit::WebThemeEngine::PartProgressBar:
      return gfx::NativeTheme::kProgressBar;
    default:
      return gfx::NativeTheme::kScrollbarDownArrow;
  }
}

static gfx::NativeTheme::State NativeThemeState(
    WebKit::WebThemeEngine::State state) {
  switch (state) {
    case WebKit::WebThemeEngine::StateDisabled:
      return gfx::NativeTheme::kDisabled;
    case WebKit::WebThemeEngine::StateHover:
      return gfx::NativeTheme::kHovered;
    case WebKit::WebThemeEngine::StateNormal:
      return gfx::NativeTheme::kNormal;
    case WebKit::WebThemeEngine::StatePressed:
      return gfx::NativeTheme::kPressed;
    default:
      return gfx::NativeTheme::kDisabled;
  }
}

static void GetNativeThemeExtraParams(
    WebKit::WebThemeEngine::Part part,
    WebKit::WebThemeEngine::State state,
    const WebKit::WebThemeEngine::ExtraParams* extra_params,
    gfx::NativeTheme::ExtraParams* native_theme_extra_params) {
  switch (part) {
    case WebKit::WebThemeEngine::PartScrollbarHorizontalTrack:
    case WebKit::WebThemeEngine::PartScrollbarVerticalTrack:
      native_theme_extra_params->scrollbar_track.track_x =
          extra_params->scrollbarTrack.trackX;
      native_theme_extra_params->scrollbar_track.track_y =
          extra_params->scrollbarTrack.trackY;
      native_theme_extra_params->scrollbar_track.track_width =
          extra_params->scrollbarTrack.trackWidth;
      native_theme_extra_params->scrollbar_track.track_height =
          extra_params->scrollbarTrack.trackHeight;
      break;
    case WebKit::WebThemeEngine::PartCheckbox:
      native_theme_extra_params->button.checked = extra_params->button.checked;
      native_theme_extra_params->button.indeterminate =
          extra_params->button.indeterminate;
      break;
    case WebKit::WebThemeEngine::PartRadio:
      native_theme_extra_params->button.checked = extra_params->button.checked;
      break;
    case WebKit::WebThemeEngine::PartButton:
      native_theme_extra_params->button.is_default =
          extra_params->button.isDefault;
      native_theme_extra_params->button.has_border =
          extra_params->button.hasBorder;
      native_theme_extra_params->button.background_color =
          extra_params->button.backgroundColor;
      break;
    case WebKit::WebThemeEngine::PartTextField:
      native_theme_extra_params->text_field.is_text_area =
          extra_params->textField.isTextArea;
      native_theme_extra_params->text_field.is_listbox =
          extra_params->textField.isListbox;
      native_theme_extra_params->text_field.background_color =
          extra_params->textField.backgroundColor;
      break;
    case WebKit::WebThemeEngine::PartMenuList:
      native_theme_extra_params->menu_list.has_border =
          extra_params->menuList.hasBorder;
      native_theme_extra_params->menu_list.has_border_radius =
          extra_params->menuList.hasBorderRadius;
      native_theme_extra_params->menu_list.arrow_x =
          extra_params->menuList.arrowX;
      native_theme_extra_params->menu_list.arrow_y =
          extra_params->menuList.arrowY;
      native_theme_extra_params->menu_list.background_color =
          extra_params->menuList.backgroundColor;
      break;
    case WebKit::WebThemeEngine::PartSliderTrack:
    case WebKit::WebThemeEngine::PartSliderThumb:
      native_theme_extra_params->slider.vertical =
          extra_params->slider.vertical;
      native_theme_extra_params->slider.in_drag = extra_params->slider.inDrag;
      break;
    case WebKit::WebThemeEngine::PartInnerSpinButton:
      native_theme_extra_params->inner_spin.spin_up =
          extra_params->innerSpin.spinUp;
      native_theme_extra_params->inner_spin.read_only =
          extra_params->innerSpin.readOnly;
      break;
    case WebKit::WebThemeEngine::PartProgressBar:
      native_theme_extra_params->progress_bar.determinate =
          extra_params->progressBar.determinate;
      native_theme_extra_params->progress_bar.value_rect_x =
          extra_params->progressBar.valueRectX;
      native_theme_extra_params->progress_bar.value_rect_y =
          extra_params->progressBar.valueRectY;
      native_theme_extra_params->progress_bar.value_rect_width =
          extra_params->progressBar.valueRectWidth;
      native_theme_extra_params->progress_bar.value_rect_height =
          extra_params->progressBar.valueRectHeight;
      break;
    default:
      break;  // Parts that have no extra params get here.
  }
}

WebKit::WebSize WebThemeEngineImpl::getSize(WebKit::WebThemeEngine::Part part) {
  gfx::NativeTheme::ExtraParams extra;
  return gfx::NativeTheme::instance()->GetPartSize(NativeThemePart(part),
                                                   gfx::NativeTheme::kNormal,
                                                   extra);
}

void WebThemeEngineImpl::paint(
    WebKit::WebCanvas* canvas,
    WebKit::WebThemeEngine::Part part,
    WebKit::WebThemeEngine::State state,
    const WebKit::WebRect& rect,
    const WebKit::WebThemeEngine::ExtraParams* extra_params) {
  gfx::NativeTheme::ExtraParams native_theme_extra_params;
  GetNativeThemeExtraParams(
      part, state, extra_params, &native_theme_extra_params);
  gfx::NativeTheme::instance()->Paint(
      canvas,
      NativeThemePart(part),
      NativeThemeState(state),
      WebRectToRect(rect),
      native_theme_extra_params);
}

}  // namespace webkit_glue
