// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_FONT_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_FONT_IMPL_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ppapi/c/dev/ppb_font_dev.h"
#include "ppapi/shared_impl/function_group_base.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/webkit_forwarding.h"
#include "ppapi/thunk/ppb_font_api.h"

namespace webkit {
namespace ppapi {

class PluginInstance;

class PPB_Font_Impl : public ::ppapi::Resource,
                      public ::ppapi::thunk::PPB_Font_API {
 public:
  virtual ~PPB_Font_Impl();

  static PP_Resource Create(PP_Instance instance,
                            const PP_FontDescription_Dev& description);

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
  PPB_Font_Impl(PP_Instance instance, const PP_FontDescription_Dev& desc);

  scoped_ptr< ::ppapi::WebKitForwarding::Font> font_forwarding_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Font_Impl);
};

class PPB_Font_FunctionImpl : public ::ppapi::FunctionGroupBase,
                              public ::ppapi::thunk::PPB_Font_FunctionAPI {
 public:
  explicit PPB_Font_FunctionImpl(PluginInstance* instance);
  virtual ~PPB_Font_FunctionImpl();

  // FunctionGroupBase overrides.
  virtual ::ppapi::thunk::PPB_Font_FunctionAPI* AsFont_FunctionAPI();

  // PPB_Font_FunctionAPI implementation.
  virtual PP_Var GetFontFamilies(PP_Instance instance) OVERRIDE;

 private:
  PluginInstance* instance_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Font_FunctionImpl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_FONT_IMPL_H_
