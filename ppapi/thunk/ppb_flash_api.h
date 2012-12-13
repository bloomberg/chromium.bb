// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_THUNK_PPB_FLASH_API_H_
#define PPAPI_THUNK_PPB_FLASH_API_H_

#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/c/private/ppb_flash_file.h"
#include "ppapi/thunk/ppapi_thunk_export.h"

namespace ppapi {

struct URLRequestInfoData;

namespace thunk {

/////////////////////////// WARNING:DEPRECTATED ////////////////////////////////
// Please do not add any new functions to this API. They should be implemented
// in the new-style resource proxy (see flash_functions_api.h and
// flash_resource.h).
// TODO(raymes): All of these functions should be moved to
// flash_functions_api.h.
////////////////////////////////////////////////////////////////////////////////
// This class collects all of the Flash interface-related APIs into one place.
class PPAPI_THUNK_EXPORT PPB_Flash_API {
 public:
  virtual ~PPB_Flash_API() {}

  // Flash.
  virtual void SetInstanceAlwaysOnTop(PP_Instance instance, PP_Bool on_top) = 0;
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
      const PP_Point glyph_advances[]) = 0;

  // External function that takes a PPB_URLRequestInfo resource.
  virtual int32_t Navigate(PP_Instance instance,
                           PP_Resource request_info,
                           const char* target,
                           PP_Bool from_user_action) = 0;

  // Internal navigate function that takes a URLRequestInfoData.
  virtual int32_t Navigate(PP_Instance instance,
                           const URLRequestInfoData& data,
                           const char* target,
                           PP_Bool from_user_action) = 0;

  virtual double GetLocalTimeZoneOffset(PP_Instance instance, PP_Time t) = 0;
  virtual PP_Bool IsRectTopmost(PP_Instance instance, const PP_Rect* rect) = 0;
  virtual PP_Var GetSetting(PP_Instance instance, PP_FlashSetting setting) = 0;
};

}  // namespace thunk
}  // namespace ppapi

#endif // PPAPI_THUNK_PPB_FLASH_API_H_
