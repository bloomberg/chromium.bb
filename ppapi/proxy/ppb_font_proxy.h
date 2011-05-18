// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_FONT_PROXY_H_
#define PPAPI_PROXY_PPB_FONT_PROXY_H_

#include "base/basictypes.h"
#include "base/synchronization/waitable_event.h"
#include "ppapi/proxy/host_resource.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/shared_impl/webkit_forwarding.h"
#include "ppapi/thunk/ppb_font_api.h"

struct PPB_Font_Dev;

namespace pp {
namespace proxy {

class SerializedVarReturnValue;

class PPB_Font_Proxy : public ::ppapi::FunctionGroupBase,
                       public ::ppapi::thunk::PPB_Font_FunctionAPI,
                       public InterfaceProxy {
 public:
  PPB_Font_Proxy(Dispatcher* dispatcher, const void* target_interface);
  virtual ~PPB_Font_Proxy();

  static const Info* GetInfo();

  // FunctionGroupBase overrides.
  virtual ::ppapi::thunk::PPB_Font_FunctionAPI* AsFont_FunctionAPI() OVERRIDE;

  // PPB_Font_FunctionAPI implementation.
  virtual PP_Var GetFontFamilies(PP_Instance instance) OVERRIDE;

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(PPB_Font_Proxy);
};

class Font : public PluginResource,
             public ppapi::thunk::PPB_Font_API {
 public:
  // Note that there isn't a "real" resource in the renderer backing a font,
  // it lives entirely in the plugin process. So the resource ID in the host
  // resource should be 0. However, various code assumes the instance in the
  // host resource is valid (this is how resources are associated with
  // instances), so that should be set.
  Font(const HostResource& resource, const PP_FontDescription_Dev& desc);
  virtual ~Font();

  // ResourceObjectBase.
  virtual ppapi::thunk::PPB_Font_API* AsFont_API() OVERRIDE;

  // PluginResource overrides.
  virtual Font* AsFont() OVERRIDE;

  // PPB_Font_API implementation.
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
  // Posts the given closure to the WebKit thread and waits on the
  // webkit_event_ for the task to continue.
  void RunOnWebKitThread(const base::Closure& task);

  base::WaitableEvent webkit_event_;

  scoped_ptr<ppapi::WebKitForwarding::Font> font_forwarding_;

  DISALLOW_COPY_AND_ASSIGN(Font);
};

}  // namespace proxy
}  // namespace pp

#endif  // PPAPI_PROXY_PPB_FONT_PROXY_H_
