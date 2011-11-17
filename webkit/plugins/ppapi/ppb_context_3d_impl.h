// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_CONTEXT_3D_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_CONTEXT_3D_IMPL_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/ppb_context_3d_api.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"

namespace gpu {
namespace gles2 {
class GLES2CmdHelper;
class GLES2Implementation;
}  // namespace gles2
}  // namespace gpu

namespace webkit {
namespace ppapi {

class PPB_Surface3D_Impl;

class PPB_Context3D_Impl : public ::ppapi::Resource,
                           public ::ppapi::thunk::PPB_Context3D_API {
 public:
  virtual ~PPB_Context3D_Impl();

  static PP_Resource Create(PP_Instance instance,
                            PP_Config3D_Dev config,
                            PP_Resource share_context,
                            const int32_t* attrib_list);
  static PP_Resource CreateRaw(PP_Instance instance,
                               PP_Config3D_Dev config,
                               PP_Resource share_context,
                               const int32_t* attrib_list);

  // Resource override.
  virtual ::ppapi::thunk::PPB_Context3D_API* AsPPB_Context3D_API();

  // PPB_Context3D_API implementation.
  virtual int32_t GetAttrib(int32_t attribute, int32_t* value) OVERRIDE;
  virtual int32_t BindSurfaces(PP_Resource draw, PP_Resource read) OVERRIDE;
  virtual int32_t GetBoundSurfaces(PP_Resource* draw,
                                   PP_Resource* read) OVERRIDE;
  virtual PP_Bool InitializeTrusted(int32_t size) OVERRIDE;
  virtual PP_Bool GetRingBuffer(int* shm_handle,
                                uint32_t* shm_size) OVERRIDE;
  virtual PP_Context3DTrustedState GetState() OVERRIDE;
  virtual PP_Bool Flush(int32_t put_offset) OVERRIDE;
  virtual PP_Context3DTrustedState FlushSync(int32_t put_offset) OVERRIDE;
  virtual int32_t CreateTransferBuffer(uint32_t size) OVERRIDE;
  virtual PP_Bool DestroyTransferBuffer(int32_t id) OVERRIDE;
  virtual PP_Bool GetTransferBuffer(int32_t id,
                                    int* shm_handle,
                                    uint32_t* shm_size) OVERRIDE;
  virtual PP_Context3DTrustedState FlushSyncFast(
      int32_t put_offset,
      int32_t last_known_get) OVERRIDE;
  virtual void* MapTexSubImage2DCHROMIUM(GLenum target,
                                         GLint level,
                                         GLint xoffset,
                                         GLint yoffset,
                                         GLsizei width,
                                         GLsizei height,
                                         GLenum format,
                                         GLenum type,
                                         GLenum access) OVERRIDE;
  virtual void UnmapTexSubImage2DCHROMIUM(const void* mem) OVERRIDE;
  virtual gpu::gles2::GLES2Implementation* GetGLES2Impl() OVERRIDE;

  int32_t BindSurfacesImpl(PPB_Surface3D_Impl* draw, PPB_Surface3D_Impl* read);

  gpu::gles2::GLES2Implementation* gles2_impl() {
    return gles2_impl_.get();
  }

  // Possibly NULL if initialization fails.
  PluginDelegate::PlatformContext3D* platform_context() {
    return platform_context_.get();
  }

 private:
  explicit PPB_Context3D_Impl(PP_Instance instance);

  bool Init(PP_Config3D_Dev config,
            PP_Resource share_context,
            const int32_t* attrib_list);
  bool InitRaw(PP_Config3D_Dev config,
               PP_Resource share_context,
               const int32_t* attrib_list);

  bool CreateImplementation();
  void Destroy();
  void OnContextLost();

  // PluginDelegate's 3D Context. Responsible for providing the command buffer.
  // Possibly NULL.
  scoped_ptr<PluginDelegate::PlatformContext3D> platform_context_;

  scoped_ptr<gpu::gles2::GLES2CmdHelper> helper_;
  int32 transfer_buffer_id_;
  scoped_ptr<gpu::gles2::GLES2Implementation> gles2_impl_;

  PPB_Surface3D_Impl* draw_surface_;
  PPB_Surface3D_Impl* read_surface_;

  base::WeakPtrFactory<PPB_Context3D_Impl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Context3D_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_CONTEXT_3D_IMPL_H_
