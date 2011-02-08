// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_FONT_PROXY_H_
#define PPAPI_PROXY_PPB_FONT_PROXY_H_

#include "base/basictypes.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/proxy/host_resource.h"
#include "ppapi/proxy/interface_proxy.h"

struct PPB_Font_Dev;

namespace pp {
namespace proxy {

struct PPBFont_DrawTextAt_Params;
struct SerializedFontDescription;
class SerializedVarReceiveInput;

class PPB_Font_Proxy : public InterfaceProxy {
 public:
  PPB_Font_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_Font_Proxy();

  static const Info* GetInfo();

  const PPB_Font_Dev* ppb_font_target() const {
    return static_cast<const PPB_Font_Dev*>(target_interface());
  }

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

 private:
  // Message handlers.
  void OnMsgCreate(PP_Instance instance,
                   const SerializedFontDescription& in_description,
                   HostResource* result,
                   SerializedFontDescription* out_description,
                   std::string* out_metrics);
  void OnMsgDrawTextAt(SerializedVarReceiveInput text,
                       const PPBFont_DrawTextAt_Params& params,
                       PP_Bool* result);
  void OnMsgMeasureText(HostResource font,
                        SerializedVarReceiveInput text,
                        PP_Bool text_is_rtl,
                        PP_Bool override_direction,
                        int32_t* result);
  void OnMsgCharacterOffsetForPixel(HostResource font,
                                    SerializedVarReceiveInput text,
                                    PP_Bool text_is_rtl,
                                    PP_Bool override_direction,
                                    int32_t pixel_pos,
                                    uint32_t* result);
  void OnMsgPixelOffsetForCharacter(HostResource font,
                                    SerializedVarReceiveInput text,
                                    PP_Bool text_is_rtl,
                                    PP_Bool override_direction,
                                    uint32_t char_offset,
                                    int32_t* result);

  DISALLOW_COPY_AND_ASSIGN(PPB_Font_Proxy);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_PPB_FONT_PROXY_H_
