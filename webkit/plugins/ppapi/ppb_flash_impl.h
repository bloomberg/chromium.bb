// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_FLASH_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_FLASH_IMPL_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "build/build_config.h"
#include "ppapi/thunk/ppb_flash_api.h"

namespace webkit {
namespace ppapi {

class PluginInstance;

class PPB_Flash_Impl : public ::ppapi::thunk::PPB_Flash_API {
 public:
  explicit PPB_Flash_Impl(PluginInstance* instance);
  virtual ~PPB_Flash_Impl();

  // PPB_Flash_API.
  virtual void SetInstanceAlwaysOnTop(PP_Instance instance,
                                      PP_Bool on_top) OVERRIDE;
  virtual PP_Bool DrawGlyphs(
      PP_Instance instance,
      PP_Resource pp_image_data,
      const PP_BrowserFont_Trusted_Description* font_desc,
      uint32_t color,
      const PP_Point* position,
      const PP_Rect* clip,
      const float transformation[3][3],
      PP_Bool allow_subpixel_aa,
      uint32_t glyph_count,
      const uint16_t glyph_indices[],
      const PP_Point glyph_advances[]) OVERRIDE;
  virtual int32_t Navigate(PP_Instance instance,
                           PP_Resource request_info,
                           const char* target,
                           PP_Bool from_user_action) OVERRIDE;
  virtual int32_t Navigate(PP_Instance instance,
                           const ::ppapi::URLRequestInfoData& data,
                           const char* target,
                           PP_Bool from_user_action) OVERRIDE;
  virtual PP_Bool IsRectTopmost(PP_Instance instance,
                                const PP_Rect* rect) OVERRIDE;

 private:
  PluginInstance* instance_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Flash_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_FLASH_IMPL_H_
