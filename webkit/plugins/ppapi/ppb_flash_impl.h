// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_FLASH_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_FLASH_IMPL_H_

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "build/build_config.h"
#include "ppapi/c/pp_point.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/private/ppb_flash.h"
#include "webkit/plugins/ppapi/callbacks.h"
#include "webkit/plugins/ppapi/resource.h"

namespace webkit {
namespace ppapi {

class PPB_Flash_Impl {
 public:
  // Returns a pointer to the interface implementing PPB_Flash that is
  // exposed to the plugin.
  static const PPB_Flash* GetInterface();

  static PP_Bool DrawGlyphs(PP_Instance pp_instance,
                            PP_Resource pp_image_data,
                            const PP_FontDescription_Dev* font_desc,
                            uint32_t color,
                            PP_Point position,
                            PP_Rect clip,
                            const float transformation[3][3],
                            uint32_t glyph_count,
                            const uint16_t glyph_indices[],
                            const PP_Point glyph_advances[])
#if defined(OS_LINUX)
      ;
#else
      { return PP_FALSE; }
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(PPB_Flash_Impl);
};

class PPB_Flash_NetConnector_Impl : public Resource {
 public:
  explicit PPB_Flash_NetConnector_Impl(PluginInstance* instance);
  virtual ~PPB_Flash_NetConnector_Impl();

  static const PPB_Flash_NetConnector* GetInterface();

  // Resource override.
  virtual PPB_Flash_NetConnector_Impl* AsPPB_Flash_NetConnector_Impl();

  // PPB_Flash_NetConnector implementation.
  int32_t ConnectTcp(const char* host,
                     uint16_t port,
                     PP_FileHandle* socket_out,
                     PP_Flash_NetAddress* local_addr_out,
                     PP_Flash_NetAddress* remote_addr_out,
                     PP_CompletionCallback callback);
  int32_t ConnectTcpAddress(const PP_Flash_NetAddress* addr,
                            PP_FileHandle* socket_out,
                            PP_Flash_NetAddress* local_addr_out,
                            PP_Flash_NetAddress* remote_addr_out,
                            PP_CompletionCallback callback);

  // Called to complete |ConnectTcp()| and |ConnectTcpAddress()|.
  void CompleteConnectTcp(PP_FileHandle socket,
                          const PP_Flash_NetAddress& local_addr,
                          const PP_Flash_NetAddress& remote_addr);

 private:
  // Any pending callback (for |ConnectTcp()| or |ConnectTcpAddress()|).
  scoped_refptr<TrackedCompletionCallback> callback_;

  // Output buffers to be filled in when the callback is completed successfully
  // (|{local,remote}_addr_out| are optional and may be null).
  PP_FileHandle* socket_out_;
  PP_Flash_NetAddress* local_addr_out_;
  PP_Flash_NetAddress* remote_addr_out_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Flash_NetConnector_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_FLASH_IMPL_H_
