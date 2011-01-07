// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_gles_chromium_texture_mapping_impl.h"

#include "gpu/command_buffer/client/gles2_implementation.h"
#include "ppapi/c/dev/ppb_gles_chromium_texture_mapping_dev.h"
#include "webkit/plugins/ppapi/ppb_context_3d_impl.h"

namespace webkit {
namespace ppapi {

namespace {

void* MapTexSubImage2DCHROMIUM(
    PP_Resource context_id,
    GLenum target,
    GLint level,
    GLint xoffset,
    GLint yoffset,
    GLsizei width,
    GLsizei height,
    GLenum format,
    GLenum type,
    GLenum access) {
  scoped_refptr<PPB_Context3D_Impl> context =
      Resource::GetAs<PPB_Context3D_Impl>(context_id);
  return context->gles2_impl()->MapTexSubImage2DCHROMIUM(
      target, level, xoffset, yoffset, width, height, format, type, access);
}

void UnmapTexSubImage2DCHROMIUM(PP_Resource context_id, const void* mem) {
  scoped_refptr<PPB_Context3D_Impl> context =
      Resource::GetAs<PPB_Context3D_Impl>(context_id);
  context->gles2_impl()->UnmapTexSubImage2DCHROMIUM(mem);
}

const struct PPB_GLESChromiumTextureMapping_Dev ppb_gles_chromium_extension = {
  &MapTexSubImage2DCHROMIUM,
  &UnmapTexSubImage2DCHROMIUM
};

}  // namespace

const PPB_GLESChromiumTextureMapping_Dev*
PPB_GLESChromiumTextureMapping_Impl::GetInterface() {
  return &ppb_gles_chromium_extension;
}

}  // namespace ppapi
}  // namespace webkit

