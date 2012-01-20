// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PRIVATE_PPB_FONT_SHARED_H_
#define PPAPI_SHARED_IMPL_PRIVATE_PPB_FONT_SHARED_H_
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
#include "ppapi/shared_impl/webkit_forwarding.h"
#include "ppapi/thunk/ppb_font_api.h"

struct PP_FontDescription_Dev;

namespace ppapi {

class PPAPI_SHARED_EXPORT PPB_Font_Shared
    : public ::ppapi::Resource,
      public ::ppapi::thunk::PPB_Font_API {
 public:
  // Validates the parameters in thee description. Can be called on any thread.
  static bool IsPPFontDescriptionValid(const PP_FontDescription_Dev& desc);

  virtual ~PPB_Font_Shared();

  static PP_Resource CreateAsImpl(PP_Instance instance,
                                  const PP_FontDescription_Dev& description,
                                  const ::ppapi::Preferences& prefs);
  static PP_Resource CreateAsProxy(PP_Instance instance,
                                   const PP_FontDescription_Dev& description,
                                   const ::ppapi::Preferences& prefs);

  // Resource.
  virtual ::ppapi::thunk::PPB_Font_API* AsPPB_Font_API() OVERRIDE;

  // PPB_Font implementation.
  virtual PP_Bool Describe(PP_FontDescription_Dev* description,
                           PP_FontMetrics_Dev* metrics) OVERRIDE;
  virtual PP_Bool DrawTextAt(PP_Resource image_data,
                             const PP_TextRun_Dev* text,
                             const PP_Point* position,
                             uint32_t color,
                             const PP_Rect* clip,
                             PP_Bool image_data_is_opaque) OVERRIDE;
  virtual int32_t MeasureText(const PP_TextRun_Dev* text) OVERRIDE;
  virtual uint32_t CharacterOffsetForPixel(const PP_TextRun_Dev* text,
                                           int32_t pixel_position) OVERRIDE;
  virtual int32_t PixelOffsetForCharacter(const PP_TextRun_Dev* text,
                                          uint32_t char_offset) OVERRIDE;

 private:
  PPB_Font_Shared(PP_Instance instance, const PP_FontDescription_Dev& desc,
                  const ::ppapi::Preferences& prefs);

  scoped_ptr< ::ppapi::WebKitForwarding::Font> font_impl_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Font_Shared);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PRIVATE_PPB_FONT_SHARED_H_
