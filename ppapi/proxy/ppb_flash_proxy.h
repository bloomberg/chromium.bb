// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_FLASH_PROXY_H_
#define PPAPI_PROXY_PPB_FLASH_PROXY_H_

#include <string>

#include "base/compiler_specific.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/thunk/ppb_flash_api.h"

struct PPB_Flash_Print_1_0;

namespace ppapi {

struct URLRequestInfoData;

namespace proxy {

struct PPBFlash_DrawGlyphs_Params;
struct SerializedDirEntry;
class SerializedVarReturnValue;

/////////////////////////// WARNING:DEPRECTATED ////////////////////////////////
// Please do not add any new functions to this proxy. They should be
// implemented in the new-style resource proxy (see flash_resource.h).
// TODO(raymes): All of these functions should be moved to the new-style proxy.
////////////////////////////////////////////////////////////////////////////////
class PPB_Flash_Proxy : public InterfaceProxy, public thunk::PPB_Flash_API {
 public:
  explicit PPB_Flash_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_Flash_Proxy();

  // This flash proxy also proxies the PPB_Flash_Print interface. This one
  // doesn't use the regular thunk system because the _impl side is actually in
  // Chrome rather than with the rest of the interface implementations.
  static const PPB_Flash_Print_1_0* GetFlashPrintInterface();

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg);

  // PPB_Flash_API implementation.
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
                           const URLRequestInfoData& data,
                           const char* target,
                           PP_Bool from_user_action) OVERRIDE;
  virtual PP_Bool IsRectTopmost(PP_Instance instance,
                                const PP_Rect* rect) OVERRIDE;

  static const ApiID kApiID = API_ID_PPB_FLASH;

 private:
  // Message handlers.
  void OnHostMsgSetInstanceAlwaysOnTop(PP_Instance instance,
                                       PP_Bool on_top);
  void OnHostMsgDrawGlyphs(PP_Instance instance,
                           const PPBFlash_DrawGlyphs_Params& params,
                           PP_Bool* result);
  void OnHostMsgNavigate(PP_Instance instance,
                         const URLRequestInfoData& data,
                         const std::string& target,
                         PP_Bool from_user_action,
                         int32_t* result);
  void OnHostMsgIsRectTopmost(PP_Instance instance,
                              PP_Rect rect,
                              PP_Bool* result);
  void OnHostMsgInvokePrinting(PP_Instance instance);

  DISALLOW_COPY_AND_ASSIGN(PPB_Flash_Proxy);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_FLASH_PROXY_H_
