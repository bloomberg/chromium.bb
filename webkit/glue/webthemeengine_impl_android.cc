// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/webthemeengine_impl_android.h"

#include "base/logging.h"
#include "skia/ext/platform_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebRect.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebSize.h"
#include "ui/gfx/native_theme_android.h"

using WebKit::WebCanvas;
using WebKit::WebColor;
using WebKit::WebRect;
using WebKit::WebThemeEngine;

namespace webkit_glue {

static gfx::NativeThemeAndroid::Part NativeThemePart(
    WebThemeEngine::Part part) {
  switch (part) {
    case WebThemeEngine::PartScrollbarDownArrow:
      return gfx::NativeThemeAndroid::SCROLLBAR_DOWN_ARROW;
    case WebThemeEngine::PartScrollbarLeftArrow:
      return gfx::NativeThemeAndroid::SCROLLBAR_LEFT_ARROW;
    case WebThemeEngine::PartScrollbarRightArrow:
      return gfx::NativeThemeAndroid::SCROLLBAR_RIGHT_ARROW;
    case WebThemeEngine::PartScrollbarUpArrow:
      return gfx::NativeThemeAndroid::SCROLLBAR_UP_ARROW;
    case WebThemeEngine::PartScrollbarHorizontalThumb:
      // Android doesn't draw scrollbars.
      NOTREACHED();
      return static_cast<gfx::NativeThemeAndroid::Part>(0);
    case WebThemeEngine::PartScrollbarVerticalThumb:
      // Android doesn't draw scrollbars.
      NOTREACHED();
      return static_cast<gfx::NativeThemeAndroid::Part>(0);
    case WebThemeEngine::PartScrollbarHorizontalTrack:
      // Android doesn't draw scrollbars.
      NOTREACHED();
      return static_cast<gfx::NativeThemeAndroid::Part>(0);
    case WebThemeEngine::PartScrollbarVerticalTrack:
      // Android doesn't draw scrollbars.
      NOTREACHED();
      return static_cast<gfx::NativeThemeAndroid::Part>(0);
    case WebThemeEngine::PartCheckbox:
      return gfx::NativeThemeAndroid::CHECKBOX;
    case WebThemeEngine::PartRadio:
      return gfx::NativeThemeAndroid::RADIO;
    case WebThemeEngine::PartButton:
      return gfx::NativeThemeAndroid::PUSH_BUTTON;
    case WebThemeEngine::PartTextField:
      return gfx::NativeThemeAndroid::TEXTFIELD;
    case WebThemeEngine::PartMenuList:
      return gfx::NativeThemeAndroid::MENU_LIST;
    case WebThemeEngine::PartSliderTrack:
      return gfx::NativeThemeAndroid::SLIDER_TRACK;
    case WebThemeEngine::PartSliderThumb:
      return gfx::NativeThemeAndroid::SLIDER_THUMB;
    case WebThemeEngine::PartInnerSpinButton:
      return gfx::NativeThemeAndroid::INNER_SPIN_BUTTON;
    case WebThemeEngine::PartProgressBar:
      return gfx::NativeThemeAndroid::PROGRESS_BAR;
    default:
      return gfx::NativeThemeAndroid::SCROLLBAR_DOWN_ARROW;
  }
}

static gfx::NativeThemeAndroid::State NativeThemeState(
    WebThemeEngine::State state) {
  switch (state) {
    case WebThemeEngine::StateDisabled:
      return gfx::NativeThemeAndroid::DISABLED;
    case WebThemeEngine::StateHover:
      return gfx::NativeThemeAndroid::HOVERED;
    case WebThemeEngine::StateNormal:
      return gfx::NativeThemeAndroid::NORMAL;
    case WebThemeEngine::StatePressed:
      return gfx::NativeThemeAndroid::PRESSED;
    default:
      return gfx::NativeThemeAndroid::DISABLED;
  }
}

static void GetNativeThemeExtraParams(
    WebThemeEngine::Part part,
    WebThemeEngine::State state,
    const WebThemeEngine::ExtraParams* extra_params,
    gfx::NativeThemeAndroid::ExtraParams* native_theme_extra_params) {
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
  return gfx::NativeThemeAndroid::instance()->GetPartSize(
      NativeThemePart(part));
}

void WebThemeEngineImpl::paint(
    WebKit::WebCanvas* canvas,
    WebThemeEngine::Part part,
    WebThemeEngine::State state,
    const WebKit::WebRect& rect,
    const WebThemeEngine::ExtraParams* extra_params) {
  gfx::NativeThemeAndroid::ExtraParams native_theme_extra_params;
  GetNativeThemeExtraParams(
      part, state, extra_params, &native_theme_extra_params);
  gfx::NativeThemeAndroid::instance()->Paint(
      canvas,
      NativeThemePart(part),
      NativeThemeState(state),
      gfx::Rect(rect),
      native_theme_extra_params);
}
}  // namespace webkit_glue
