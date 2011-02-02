// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_gles_chromium_texture_mapping_proxy.h"

#include "gpu/command_buffer/client/gles2_implementation.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/dev/ppb_gles_chromium_texture_mapping_dev.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppb_context_3d_proxy.h"

namespace pp {
namespace proxy {

namespace {

void* MapTexSubImage2DCHROMIUM(PP_Resource context_id,
                               GLenum target,
                               GLint level,
                               GLint xoffset,
                               GLint yoffset,
                               GLsizei width,
                               GLsizei height,
                               GLenum format,
                               GLenum type,
                               GLenum access) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  return context->gles2_impl()->MapTexSubImage2DCHROMIUM(
      target, level, xoffset, yoffset, width, height, format, type, access);
}

void UnmapTexSubImage2DCHROMIUM(PP_Resource context_id, const void* mem) {
  Context3D* context = PluginResource::GetAs<Context3D>(context_id);
  context->gles2_impl()->UnmapTexSubImage2DCHROMIUM(mem);
}

const struct PPB_GLESChromiumTextureMapping_Dev ppb_gles2_chromium_tm = {
  MapTexSubImage2DCHROMIUM,
  UnmapTexSubImage2DCHROMIUM
};

}  // namespace

PPB_GLESChromiumTextureMapping_Proxy::PPB_GLESChromiumTextureMapping_Proxy(
    Dispatcher* dispatcher,
    const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_GLESChromiumTextureMapping_Proxy::~PPB_GLESChromiumTextureMapping_Proxy() {
}

const void* PPB_GLESChromiumTextureMapping_Proxy::GetSourceInterface() const {
  return &ppb_gles2_chromium_tm;
}

InterfaceID PPB_GLESChromiumTextureMapping_Proxy::GetInterfaceId() const {
  return INTERFACE_ID_NONE;
}

bool PPB_GLESChromiumTextureMapping_Proxy::OnMessageReceived(
    const IPC::Message& msg) {
  return false;
}

}  // namespace proxy
}  // namespace pp
