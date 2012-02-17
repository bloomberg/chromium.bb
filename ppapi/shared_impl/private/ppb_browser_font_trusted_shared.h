// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PRIVATE_PPB_BROWSER_FONT_TRUSTED_SHARED_H_
#define PPAPI_SHARED_IMPL_PRIVATE_PPB_BROWSER_FONT_TRUSTED_SHARED_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"
#include "ppapi/shared_impl/ppapi_preferences.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_browser_font_trusted_api.h"

namespace skia {
class PlatformCanvas;
}

namespace WebKit {
class WebFont;
}

namespace ppapi {

class PPAPI_SHARED_EXPORT PPB_BrowserFont_Trusted_Shared
    : public Resource,
      public thunk::PPB_BrowserFont_Trusted_API {
 public:
  // Validates the parameters in thee description. Can be called on any thread.
  static bool IsPPFontDescriptionValid(
      const PP_BrowserFont_Trusted_Description& desc);

  virtual ~PPB_BrowserFont_Trusted_Shared();

  static PP_Resource Create(
      ResourceObjectType type,
      PP_Instance instance,
      const PP_BrowserFont_Trusted_Description& description,
      const ::ppapi::Preferences& prefs);

  // Resource.
  virtual ::ppapi::thunk::PPB_BrowserFont_Trusted_API*
      AsPPB_BrowserFont_Trusted_API() OVERRIDE;

  // PPB_Font implementation.
  virtual PP_Bool Describe(PP_BrowserFont_Trusted_Description* description,
                           PP_BrowserFont_Trusted_Metrics* metrics) OVERRIDE;
  virtual PP_Bool DrawTextAt(PP_Resource image_data,
                             const PP_BrowserFont_Trusted_TextRun* text,
                             const PP_Point* position,
                             uint32_t color,
                             const PP_Rect* clip,
                             PP_Bool image_data_is_opaque) OVERRIDE;
  virtual int32_t MeasureText(
      const PP_BrowserFont_Trusted_TextRun* text) OVERRIDE;
  virtual uint32_t CharacterOffsetForPixel(
      const PP_BrowserFont_Trusted_TextRun* text,
      int32_t pixel_position) OVERRIDE;
  virtual int32_t PixelOffsetForCharacter(
      const PP_BrowserFont_Trusted_TextRun* text,
      uint32_t char_offset) OVERRIDE;

 private:
  PPB_BrowserFont_Trusted_Shared(ResourceObjectType type,
                                 PP_Instance instance,
                                 const PP_BrowserFont_Trusted_Description& desc,
                                 const Preferences& prefs);

  // Internal version of DrawTextAt that takes a mapped PlatformCanvas.
  void DrawTextToCanvas(skia::PlatformCanvas* destination,
                        const PP_BrowserFont_Trusted_TextRun& text,
                        const PP_Point* position,
                        uint32_t color,
                        const PP_Rect* clip,
                        PP_Bool image_data_is_opaque);

  scoped_ptr<WebKit::WebFont> font_;

  DISALLOW_COPY_AND_ASSIGN(PPB_BrowserFont_Trusted_Shared);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PRIVATE_PPB_BROWSER_FONT_TRUSTED_SHARED_H_
