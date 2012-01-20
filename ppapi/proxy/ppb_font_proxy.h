// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_PROXY_PPB_FONT_PROXY_H_
#define PPAPI_PROXY_PPB_FONT_PROXY_H_

#include "base/callback_forward.h"
#include "base/synchronization/waitable_event.h"
#include "ppapi/proxy/interface_proxy.h"
#include "ppapi/shared_impl/function_group_base.h"
#include "ppapi/shared_impl/host_resource.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/shared_impl/webkit_forwarding.h"
#include "ppapi/thunk/ppb_font_api.h"

namespace ppapi {
namespace proxy {

class PPB_Font_Proxy : public InterfaceProxy,
                       public ppapi::thunk::PPB_Font_FunctionAPI {
 public:
  explicit PPB_Font_Proxy(Dispatcher* dispatcher);
  virtual ~PPB_Font_Proxy();

  // FunctionGroupBase overrides.
  virtual ppapi::thunk::PPB_Font_FunctionAPI* AsPPB_Font_FunctionAPI() OVERRIDE;

  // PPB_Font_FunctionAPI implementation.
  virtual PP_Var GetFontFamilies(PP_Instance instance) OVERRIDE;

  // InterfaceProxy implementation.
  virtual bool OnMessageReceived(const IPC::Message& msg) OVERRIDE;

  static const ApiID kApiID = API_ID_PPB_FONT;

 private:
  DISALLOW_COPY_AND_ASSIGN(PPB_Font_Proxy);
};

class Font : public ppapi::Resource,
             public ppapi::thunk::PPB_Font_API {
 public:
  // Note that there isn't a "real" resource in the renderer backing a font,
  // it lives entirely in the plugin process. So the resource ID in the host
  // resource should be 0. However, various code assumes the instance in the
  // host resource is valid (this is how resources are associated with
  // instances), so that should be set.
  Font(const ppapi::HostResource& resource, const PP_FontDescription_Dev& desc);
  virtual ~Font();

  // Resource.
  virtual ppapi::thunk::PPB_Font_API* AsPPB_Font_API() OVERRIDE;

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
  // Posts the given closure to the WebKit thread.
  // If |blocking| is true, the method waits on |webkit_event_| for the task to
  // continue.
  void RunOnWebKitThread(bool blocking, const base::Closure& task);

  static void DeleteFontForwarding(
      ppapi::WebKitForwarding::Font* font_forwarding);

  base::WaitableEvent webkit_event_;

  // This class owns |font_forwarding_|.
  // |font_forwarding_| should always be used on the WebKit thread (including
  // destruction).
  ppapi::WebKitForwarding::Font* font_forwarding_;

  DISALLOW_COPY_AND_ASSIGN(Font);
};

}  // namespace proxy
}  // namespace ppapi

#endif  // PPAPI_PROXY_PPB_FONT_PROXY_H_
