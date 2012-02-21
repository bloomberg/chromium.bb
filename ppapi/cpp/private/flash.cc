// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/cpp/private/flash.h"

#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/cpp/dev/font_dev.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/module_impl.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/url_request_info.h"
#include "ppapi/cpp/var.h"
#include "ppapi/c/private/ppb_flash.h"

namespace pp {

namespace {

template <> const char* interface_name<PPB_Flash>() {
  return PPB_FLASH_INTERFACE;
}

template <> const char* interface_name<PPB_Flash_12_0>() {
  return PPB_FLASH_INTERFACE_12_0;
}

template <> const char* interface_name<PPB_Flash_11>() {
  return PPB_FLASH_INTERFACE_11_0;
}

}  // namespace

namespace flash {

// static
bool Flash::IsAvailable() {
  return has_interface<PPB_Flash>() ||
         has_interface<PPB_Flash_12_0>() ||
         has_interface<PPB_Flash_11>();
}

// static
void Flash::SetInstanceAlwaysOnTop(Instance* instance, bool on_top) {
  if (has_interface<PPB_Flash>()) {
    get_interface<PPB_Flash>()->SetInstanceAlwaysOnTop(instance->pp_instance(),
                                                       PP_FromBool(on_top));
  } else if (has_interface<PPB_Flash_12_0>()) {
    get_interface<PPB_Flash_12_0>()->SetInstanceAlwaysOnTop(
        instance->pp_instance(), PP_FromBool(on_top));
  } else if (has_interface<PPB_Flash_11>()) {
    get_interface<PPB_Flash_11>()->SetInstanceAlwaysOnTop(
        instance->pp_instance(), PP_FromBool(on_top));
  }
}

// static
bool Flash::DrawGlyphs(Instance* instance,
                       ImageData* image,
                       const FontDescription_Dev& font_desc,
                       uint32_t color,
                       const Point& position,
                       const Rect& clip,
                       const float transformation[3][3],
                       bool allow_subpixel_aa,
                       uint32_t glyph_count,
                       const uint16_t glyph_indices[],
                       const PP_Point glyph_advances[]) {
  bool rv = false;
  if (has_interface<PPB_Flash>()) {
    rv = PP_ToBool(get_interface<PPB_Flash>()->DrawGlyphs(
        instance->pp_instance(),
        image->pp_resource(),
        &font_desc.pp_font_description(),
        color,
        &position.pp_point(),
        &clip.pp_rect(),
        transformation,
        PP_FromBool(allow_subpixel_aa),
        glyph_count,
        glyph_indices,
        glyph_advances));
  } else if (has_interface<PPB_Flash_12_0>()) {
    rv = PP_ToBool(get_interface<PPB_Flash_12_0>()->DrawGlyphs(
        instance->pp_instance(),
        image->pp_resource(),
        &font_desc.pp_font_description(),
        color,
        &position.pp_point(),
        &clip.pp_rect(),
        transformation,
        PP_FromBool(allow_subpixel_aa),
        glyph_count,
        glyph_indices,
        glyph_advances));
  } else if (has_interface<PPB_Flash_11>()) {
    rv = PP_ToBool(get_interface<PPB_Flash_11>()->DrawGlyphs(
        instance->pp_instance(),
        image->pp_resource(),
        &font_desc.pp_font_description(),
        color,
        position.pp_point(),
        clip.pp_rect(),
        transformation,
        glyph_count,
        glyph_indices,
        glyph_advances));
  }
  return rv;
}

// static
Var Flash::GetProxyForURL(Instance* instance, const std::string& url) {
  Var rv;
  if (has_interface<PPB_Flash>()) {
    rv = Var(Var::PassRef(),
             get_interface<PPB_Flash>()->GetProxyForURL(instance->pp_instance(),
                                                        url.c_str()));
  } else if (has_interface<PPB_Flash_12_0>()) {
    rv = Var(Var::PassRef(),
             get_interface<PPB_Flash_12_0>()->GetProxyForURL(
                 instance->pp_instance(), url.c_str()));
  } else if (has_interface<PPB_Flash_11>()) {
    rv = Var(Var::PassRef(),
             get_interface<PPB_Flash_11>()->GetProxyForURL(
                 instance->pp_instance(), url.c_str()));
  }
  return rv;
}

// static
int32_t Flash::Navigate(const URLRequestInfo& request_info,
                        const std::string& target,
                        bool from_user_action) {
  int32_t rv = PP_ERROR_FAILED;
  if (has_interface<PPB_Flash>()) {
    rv = get_interface<PPB_Flash>()->Navigate(request_info.pp_resource(),
                                              target.c_str(),
                                              PP_FromBool(from_user_action));
  } else if (has_interface<PPB_Flash_12_0>()) {
    rv = get_interface<PPB_Flash_12_0>()->Navigate(
        request_info.pp_resource(),
        target.c_str(),
        PP_FromBool(from_user_action));
  } else if (has_interface<PPB_Flash_11>()) {
    rv = get_interface<PPB_Flash_11>()->Navigate(request_info.pp_resource(),
                                                 target.c_str(),
                                                 from_user_action);
  }
  return rv;
}

// static
void Flash::RunMessageLoop(Instance* instance) {
  if (has_interface<PPB_Flash>())
    get_interface<PPB_Flash>()->RunMessageLoop(instance->pp_instance());
  else if (has_interface<PPB_Flash_12_0>())
    get_interface<PPB_Flash_12_0>()->RunMessageLoop(instance->pp_instance());
  else if (has_interface<PPB_Flash_11>())
    get_interface<PPB_Flash_11>()->RunMessageLoop(instance->pp_instance());
}

// static
void Flash::QuitMessageLoop(Instance* instance) {
  if (has_interface<PPB_Flash>())
    get_interface<PPB_Flash>()->QuitMessageLoop(instance->pp_instance());
  else if (has_interface<PPB_Flash_12_0>())
    get_interface<PPB_Flash_12_0>()->QuitMessageLoop(instance->pp_instance());
  else if (has_interface<PPB_Flash_11>())
    get_interface<PPB_Flash_11>()->QuitMessageLoop(instance->pp_instance());
}

// static
double Flash::GetLocalTimeZoneOffset(Instance* instance, PP_Time t) {
  double rv = 0;
  if (has_interface<PPB_Flash>()) {
    rv = get_interface<PPB_Flash>()->GetLocalTimeZoneOffset(
        instance->pp_instance(), t);
  } else if (has_interface<PPB_Flash_12_0>()) {
    rv = get_interface<PPB_Flash_12_0>()->GetLocalTimeZoneOffset(
        instance->pp_instance(), t);
  } else if (has_interface<PPB_Flash_11>()) {
    rv = get_interface<PPB_Flash_11>()->GetLocalTimeZoneOffset(
        instance->pp_instance(), t);
  }
  return rv;
}

// static
Var Flash::GetCommandLineArgs(Module* module) {
  Var rv;
  if (has_interface<PPB_Flash>()) {
    rv = Var(Var::PassRef(),
             get_interface<PPB_Flash>()->GetCommandLineArgs(
                 module->pp_module()));
  } else if (has_interface<PPB_Flash_12_0>()) {
    rv = Var(Var::PassRef(),
             get_interface<PPB_Flash_12_0>()->GetCommandLineArgs(
                 module->pp_module()));
  } else if (has_interface<PPB_Flash_11>()) {
    rv = Var(Var::PassRef(),
             get_interface<PPB_Flash_11>()->GetCommandLineArgs(
                 module->pp_module()));
  }
  return rv;
}

// static
void Flash::PreloadFontWin(const void* logfontw) {
  if (has_interface<PPB_Flash>())
    get_interface<PPB_Flash>()->PreloadFontWin(logfontw);
  else if (has_interface<PPB_Flash_12_0>())
    get_interface<PPB_Flash_12_0>()->PreloadFontWin(logfontw);
}

// static
bool Flash::IsRectTopmost(Instance* instance, const Rect& rect) {
  bool rv = false;
  if (has_interface<PPB_Flash>()) {
    rv = PP_ToBool(get_interface<PPB_Flash>()->IsRectTopmost(
        instance->pp_instance(), &rect.pp_rect()));
  }
  return rv;
}

// static
int32_t Flash::InvokePrinting(Instance* instance) {
  int32_t rv = PP_ERROR_NOTSUPPORTED;
  if (has_interface<PPB_Flash>())
    rv = get_interface<PPB_Flash>()->InvokePrinting(instance->pp_instance());
  return rv;
}

// static
void Flash::UpdateActivity(Instance* instance) {
  if (has_interface<PPB_Flash>())
    get_interface<PPB_Flash>()->UpdateActivity(instance->pp_instance());
}

}  // namespace flash
}  // namespace pp
