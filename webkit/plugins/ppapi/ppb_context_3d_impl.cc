// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/ppapi/ppb_context_3d_impl.h"

#include "base/logging.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "webkit/plugins/ppapi/common.h"
#include "webkit/plugins/ppapi/ppapi_plugin_instance.h"
#include "webkit/plugins/ppapi/ppb_surface_3d_impl.h"

namespace webkit {
namespace ppapi {

namespace {

// Size of the transfer buffer.
enum { kTransferBufferSize = 512 * 1024 };

PP_Resource Create(PP_Instance instance_id,
                   PP_Config3D_Dev config,
                   PP_Resource share_context,
                   const int32_t* attrib_list) {
  // TODO(alokp): Support shared context.
  DCHECK_EQ(0, share_context);
  if (share_context != 0)
    return 0;

  PluginInstance* instance = ResourceTracker::Get()->GetInstance(instance_id);
  if (!instance)
    return 0;

  scoped_refptr<PPB_Context3D_Impl> context(
      new PPB_Context3D_Impl(instance));
  if (!context->Init(config, share_context, attrib_list))
    return 0;

  return context->GetReference();
}

PP_Bool IsContext3D(PP_Resource resource) {
  return BoolToPPBool(!!Resource::GetAs<PPB_Context3D_Impl>(resource));
}

int32_t GetAttrib(PP_Resource context,
                  int32_t attribute,
                  int32_t* value) {
  // TODO(alokp): Implement me.
  return 0;
}

int32_t BindSurfaces(PP_Resource context_id,
                     PP_Resource draw,
                     PP_Resource read) {
  scoped_refptr<PPB_Context3D_Impl> context(
      Resource::GetAs<PPB_Context3D_Impl>(context_id));
  if (!context.get())
    return PP_ERROR_BADRESOURCE;

  scoped_refptr<PPB_Surface3D_Impl> draw_surface(
      Resource::GetAs<PPB_Surface3D_Impl>(draw));
  if (!draw_surface.get())
    return PP_ERROR_BADRESOURCE;

  scoped_refptr<PPB_Surface3D_Impl> read_surface(
      Resource::GetAs<PPB_Surface3D_Impl>(read));
  if (!read_surface.get())
    return PP_ERROR_BADRESOURCE;

  return context->BindSurfaces(draw_surface.get(), read_surface.get());
}

int32_t GetBoundSurfaces(PP_Resource context,
                         PP_Resource* draw,
                         PP_Resource* read) {
  // TODO(alokp): Implement me.
  return 0;
}

const PPB_Context3D_Dev ppb_context3d = {
  &Create,
  &IsContext3D,
  &GetAttrib,
  &BindSurfaces,
  &GetBoundSurfaces,
};

}  // namespace

PPB_Context3D_Impl::PPB_Context3D_Impl(PluginInstance* instance)
    : Resource(instance),
      instance_(instance),
      gles2_impl_(NULL),
      draw_surface_(NULL),
      read_surface_(NULL) {
}

PPB_Context3D_Impl::~PPB_Context3D_Impl() {
  Destroy();
}

const PPB_Context3D_Dev* PPB_Context3D_Impl::GetInterface() {
  return &ppb_context3d;
}

PPB_Context3D_Impl* PPB_Context3D_Impl::AsPPB_Context3D_Impl() {
  return this;
}

bool PPB_Context3D_Impl::Init(PP_Config3D_Dev config,
                              PP_Resource share_context,
                              const int32_t* attrib_list) {
  // Create and initialize the objects required to issue GLES2 calls.
  platform_context_.reset(instance()->CreateContext3D());
  if (!platform_context_.get()) {
    Destroy();
    return false;
  }
  if (!platform_context_->Init()) {
    Destroy();
    return false;
  }

  gles2_impl_ = platform_context_->GetGLES2Implementation();
  DCHECK(gles2_impl_);

  return true;
}

int32_t PPB_Context3D_Impl::BindSurfaces(PPB_Surface3D_Impl* draw,
                                         PPB_Surface3D_Impl* read) {
  // TODO(alokp): Support separate draw-read surfaces.
  DCHECK_EQ(draw, read);
  if (draw != read)
    return PP_GRAPHICS3DERROR_BAD_MATCH;

  if (draw == draw_surface_)
    return PP_OK;

  if (draw && draw->context())
    return PP_GRAPHICS3DERROR_BAD_ACCESS;

  if (draw_surface_)
    draw_surface_->BindToContext(NULL);
  if (draw && !draw->BindToContext(platform_context_.get()))
    return PP_ERROR_NOMEMORY;

  draw_surface_ = draw;
  read_surface_ = read;
  return PP_OK;
}

void PPB_Context3D_Impl::Destroy() {
  if (draw_surface_)
    draw_surface_->BindToContext(NULL);

  gles2_impl_ = NULL;
  platform_context_.reset();
}

}  // namespace ppapi
}  // namespace webkit

