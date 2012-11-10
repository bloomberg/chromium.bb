// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_PPAPI_PPB_GRAPHICS_3D_IMPL_H_
#define WEBKIT_PLUGINS_PPAPI_PPB_GRAPHICS_3D_IMPL_H_

#include "base/memory/weak_ptr.h"
#include "ppapi/shared_impl/ppb_graphics_3d_shared.h"
#include "ppapi/shared_impl/resource.h"
#include "webkit/plugins/ppapi/plugin_delegate.h"

namespace webkit {
namespace ppapi {

class PPB_Graphics3D_Impl : public ::ppapi::PPB_Graphics3D_Shared {
 public:
  virtual ~PPB_Graphics3D_Impl();

  static PP_Resource Create(PP_Instance instance,
                            PP_Resource share_context,
                            const int32_t* attrib_list);
  static PP_Resource CreateRaw(PP_Instance instance,
                               PP_Resource share_context,
                               const int32_t* attrib_list);

  // PPB_Graphics3D_API trusted implementation.
  virtual PP_Bool InitCommandBuffer() OVERRIDE;
  virtual PP_Bool SetGetBuffer(int32_t transfer_buffer_id) OVERRIDE;
  virtual PP_Graphics3DTrustedState GetState() OVERRIDE;
  virtual int32_t CreateTransferBuffer(uint32_t size) OVERRIDE;
  virtual PP_Bool DestroyTransferBuffer(int32_t id) OVERRIDE;
  virtual PP_Bool GetTransferBuffer(int32_t id,
                                    int* shm_handle,
                                    uint32_t* shm_size) OVERRIDE;
  virtual PP_Bool Flush(int32_t put_offset) OVERRIDE;
  virtual PP_Graphics3DTrustedState FlushSync(int32_t put_offset) OVERRIDE;
  virtual PP_Graphics3DTrustedState FlushSyncFast(
      int32_t put_offset,
      int32_t last_known_get) OVERRIDE;

  // Binds/unbinds the graphics of this context with the associated instance.
  // Returns true if binding/unbinding is successful.
  bool BindToInstance(bool bind);

  // Returns the id of texture that can be used by the compositor.
  unsigned int GetBackingTextureId();

  // Returns true if the backing texture is always opaque.
  bool IsOpaque();

  // Notifications about the view's progress painting.  See PluginInstance.
  // These messages are used to send Flush callbacks to the plugin.
  void ViewWillInitiatePaint();
  void ViewInitiatedPaint();
  void ViewFlushedPaint();

  PluginDelegate::PlatformContext3D* platform_context() {
    return platform_context_.get();
  }

 protected:
  // ppapi::PPB_Graphics3D_Shared overrides.
  virtual gpu::CommandBuffer* GetCommandBuffer() OVERRIDE;
  virtual int32 DoSwapBuffers() OVERRIDE;

 private:
  explicit PPB_Graphics3D_Impl(PP_Instance instance);

  static PP_Bool IsGpuBlacklisted();

  bool Init(PPB_Graphics3D_API* share_context,
            const int32_t* attrib_list);
  bool InitRaw(PPB_Graphics3D_API* share_context,
               const int32_t* attrib_list);

  // Notifications received from the GPU process.
  void OnSwapBuffers();
  void OnContextLost();
  void OnConsoleMessage(const std::string& msg, int id);
  // Notifications sent to plugin.
  void SendContextLost();

  // True if context is bound to instance.
  bool bound_to_instance_;
  // True when waiting for compositor to commit our backing texture.
  bool commit_pending_;
  // PluginDelegate's 3D Context. Responsible for providing the command buffer.
  scoped_ptr<PluginDelegate::PlatformContext3D> platform_context_;
  base::WeakPtrFactory<PPB_Graphics3D_Impl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PPB_Graphics3D_Impl);
};

}  // namespace ppapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_PPAPI_PPB_GRAPHICS_3D_IMPL_H_
