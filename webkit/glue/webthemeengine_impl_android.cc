// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webthemeengine_impl_android.h"

#include "base/logging.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "ui/gfx/native_theme.h"

using WebKit::WebCanvas;
using WebKit::WebColor;
using WebKit::WebRect;
using WebKit::WebThemeEngine;

namespace webkit_glue {

static gfx::NativeTheme::Part NativeThemePart(
    WebThemeEngine::Part part) {
  switch (part) {
    case WebThemeEngine::PartScrollbarDownArrow:
      return gfx::NativeTheme::kScrollbarDownArrow;
    case WebThemeEngine::PartScrollbarLeftArrow:
      return gfx::NativeTheme::kScrollbarLeftArrow;
    case WebThemeEngine::PartScrollbarRightArrow:
      return gfx::NativeTheme::kScrollbarRightArrow;
    case WebThemeEngine::PartScrollbarUpArrow:
      return gfx::NativeTheme::kScrollbarUpArrow;
    case WebThemeEngine::PartScrollbarHorizontalThumb:
      // Android doesn't draw scrollbars.
      NOTREACHED();
      return static_cast<gfx::NativeTheme::Part>(0);
    case WebThemeEngine::PartScrollbarVerticalThumb:
      // Android doesn't draw scrollbars.
      NOTREACHED();
      return static_cast<gfx::NativeTheme::Part>(0);
    case WebThemeEngine::PartScrollbarHorizontalTrack:
      // Android doesn't draw scrollbars.
      NOTREACHED();
      return static_cast<gfx::NativeTheme::Part>(0);
    case WebThemeEngine::PartScrollbarVerticalTrack:
      // Android doesn't draw scrollbars.
      NOTREACHED();
      return static_cast<gfx::NativeTheme::Part>(0);
    case WebThemeEngine::PartCheckbox:
      return gfx::NativeTheme::kCheckbox;
    case WebThemeEngine::PartRadio:
      return gfx::NativeTheme::kRadio;
    case WebThemeEngine::PartButton:
      return gfx::NativeTheme::kPushButton;
    case WebThemeEngine::PartTextField:
      return gfx::NativeTheme::kTextField;
    case WebThemeEngine::PartMenuList:
      return gfx::NativeTheme::kMenuList;
    case WebThemeEngine::PartSliderTrack:
      return gfx::NativeTheme::kSliderTrack;
    case WebThemeEngine::PartSliderThumb:
      return gfx::NativeTheme::kSliderThumb;
    case WebThemeEngine::PartInnerSpinButton:
      return gfx::NativeTheme::kInnerSpinButton;
    case WebThemeEngine::PartProgressBar:
      return gfx::NativeTheme::kProgressBar;
    default:
      return gfx::NativeTheme::kScrollbarDownArrow;
  }
}

static gfx::NativeTheme::State NativeThemeState(
    WebThemeEngine::State state) {
  switch (state) {
    case WebThemeEngine::StateDisabled:
      return gfx::NativeTheme::kDisabled;
    case WebThemeEngine::StateHover:
      return gfx::NativeTheme::kHovered;
    case WebThemeEngine::StateNormal:
      return gfx::NativeTheme::kNormal;
    case WebThemeEngine::StatePressed:
      return gfx::NativeTheme::kPressed;
    default:
      return gfx::NativeTheme::kDisabled;
  }
}

static void GetNativeThemeExtraParams(
    WebThemeEngine::Part part,
    WebThemeEngine::State state,
    const WebThemeEngine::ExtraParams* extra_params,
    gfx::NativeTheme::ExtraParams* native_theme_extra_params) {
  switch (part) {
    case WebThemeEngine::PartScrollbarHorizontalTrack:
    case WebThemeEngine::PartScrollbarVerticalTrack:
      // Android doesn't draw scrollbars.
      NOTREACHED();
      break;
    case WebThemeEngine::PartCheckbox:
      native_theme_extra_params->button.checked = extra_params->button.checked;
      native_theme_extra_params->button.indeterminate =
          extra_params->button.indeterminate;
      break;
    case WebThemeEngine::PartRadio:
      native_theme_extra_params->button.checked = extra_params->button.checked;
      break;
    case WebThemeEngine::PartButton:
      native_theme_extra_params->button.is_default =
          extra_params->button.isDefault;
      native_theme_extra_params->button.has_border =
          extra_params->button.hasBorder;
      native_theme_extra_params->button.background_color =
          extra_params->button.backgroundColor;
      break;
    case WebThemeEngine::PartTextField:
      native_theme_extra_params->text_field.is_text_area =
          extra_params->textField.isTextArea;
      native_theme_extra_params->text_field.is_listbox =
          extra_params->textField.isListbox;
      native_theme_extra_params->text_field.background_color =
          extra_params->textField.backgroundColor;
      break;
    case WebThemeEngine::PartMenuList:
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
    case WebThemeEngine::PartSliderTrack:
    case WebThemeEngine::PartSliderThumb:
      native_theme_extra_params->slider.vertical =
          extra_params->slider.vertical;
      native_theme_extra_params->slider.in_drag = extra_params->slider.inDrag;
      break;
    case WebThemeEngine::PartInnerSpinButton:
      native_theme_extra_params->inner_spin.spin_up =
          extra_params->innerSpin.spinUp;
      native_theme_extra_params->inner_spin.read_only =
          extra_params->innerSpin.readOnly;
      break;
    case WebThemeEngine::PartProgressBar:
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

WebKit::WebSize WebThemeEngineImpl::getSize(WebThemeEngine::Part part) {
  gfx::NativeTheme::ExtraParams extra;
  return gfx::NativeTheme::instance()->GetPartSize(
      NativeThemePart(part), gfx::NativeTheme::kNormal, extra);
}

void WebThemeEngineImpl::paint(
    WebKit::WebCanvas* canvas,
    WebThemeEngine::Part part,
    WebThemeEngine::State state,
    const WebKit::WebRect& rect,
    const WebThemeEngine::ExtraParams* extra_params) {
  gfx::NativeTheme::ExtraParams native_theme_extra_params;
  GetNativeThemeExtraParams(
      part, state, extra_params, &native_theme_extra_params);
  gfx::NativeTheme::instance()->Paint(
      canvas,
      NativeThemePart(part),
      NativeThemeState(state),
      gfx::Rect(rect),
      native_theme_extra_params);
}
}  // namespace webkit_glue
