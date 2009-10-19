/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


// This file contains the implementation of the command buffer renderer.

#ifndef O3D_CORE_CROSS_COMMAND_BUFFER_RENDERER_CB_H_
#define O3D_CORE_CROSS_COMMAND_BUFFER_RENDERER_CB_H_

#include "core/cross/precompile.h"
#include <vector>
#include "core/cross/renderer.h"
#include "command_buffer/common/cross/constants.h"
#include "command_buffer/common/cross/resource.h"
#include "command_buffer/client/cross/id_allocator.h"
#include "gpu_plugin/command_buffer.h"
#include "gpu_plugin/np_utils/np_object_pointer.h"

namespace o3d {
namespace command_buffer {
class CommandBufferHelper;
class BufferSyncInterface;
class FencedAllocatorWrapper;
}  // namespace command_buffer

class Material;

// TODO: change the Init API to not rely on direct HWND so we don't need
// to create a command buffer locally.
class Win32CBServer;

// This is the implementation of the Renderer interface for command buffers.
class RendererCB : public Renderer {
 public:
  typedef command_buffer::IdAllocator IdAllocator;
  typedef command_buffer::FencedAllocatorWrapper FencedAllocatorWrapper;

  virtual ~RendererCB();

  // Handles the plugin resize event.
  virtual void Resize(int width, int height);

  // Releases all hardware resources. This should be called before destroying
  // the window used for rendering. It will be called automatically from the
  // destructor.
  // Destroy() should be called before Init() is called again.
  virtual void Destroy();

  // Overridden from Renderer.
  virtual bool GoFullscreen(const DisplayWindow& display,
                            int mode_id) {
    // TODO(gman): implement this.
    return false;
  }

  // Overridden from Renderer.
  virtual bool CancelFullscreen(const DisplayWindow& display,
                                int width, int height) {
    // TODO(gman): implement this.
    return false;
  }

  // Tells whether we're currently displayed fullscreen or not.
  virtual bool fullscreen() const {
    // TODO(gman): implement this.
    return false;
  }

  // Get a vector of the available fullscreen display modes.
  // Clears *modes on error.
  virtual void GetDisplayModes(std::vector<DisplayMode> *modes) {
    // TODO(gman): implement this.
  }

  // Get a single fullscreen display mode by id.
  // Returns true on success, false on error.
  virtual bool GetDisplayMode(int id, DisplayMode *mode) {
    // TODO(gman): implement this.
    return false;
  }

  // Creates a StreamBank, returning a platform specific implementation class.
  virtual StreamBank::Ref CreateStreamBank();

  // Creates a Primitive, returning a platform specific implementation class.
  virtual Primitive::Ref CreatePrimitive();

  // Creates a DrawElement, returning a platform specific implementation
  // class.
  virtual DrawElement::Ref CreateDrawElement();

  // Creates and returns a platform specific float buffer
  virtual VertexBuffer::Ref CreateVertexBuffer();

  // Creates and returns a platform specific integer buffer
  virtual IndexBuffer::Ref CreateIndexBuffer();

  // Creates and returns a platform specific effect object
  virtual Effect::Ref CreateEffect();

  // Creates and returns a platform specific Sampler object.
  virtual Sampler::Ref CreateSampler();

  // Creates and returns a platform specific RenderDepthStencilSurface object.
  virtual RenderDepthStencilSurface::Ref CreateDepthStencilSurface(
      int width,
      int height);

  // Gets the allocator for vertex buffer IDs.
  IdAllocator &vertex_buffer_ids() { return vertex_buffer_ids_; }

  // Gets the allocator for index buffer IDs.
  IdAllocator &index_buffer_ids() { return index_buffer_ids_; }

  // Gets the allocator for vertex struct IDs.
  IdAllocator &vertex_structs_ids() { return vertex_structs_ids_; }

  // Gets the allocator for effect IDs.
  IdAllocator &effect_ids() { return effect_ids_; }

  // Gets the allocator for effect param IDs.
  IdAllocator &effect_param_ids() { return effect_param_ids_; }

  // Gets the allocator for texture IDs.
  IdAllocator &texture_ids() { return texture_ids_; }

  // Gets the allocator for sampler IDs.
  IdAllocator &sampler_ids() { return sampler_ids_; }

  // Gets the allocator for render surfaces IDs.
  IdAllocator &render_surface_ids() { return render_surface_ids_; }

  // Gets the allocator for depth stencil surfaces IDs.
  IdAllocator &depth_surface_ids() { return depth_surface_ids_; }

  // Gets the command buffer helper.
  command_buffer::CommandBufferHelper *helper() const { return helper_; }

  // Gets the registered ID of the transfer shared memory.
  int32 transfer_shm_id() const { return transfer_shm_id_; }

  // Gets the base address of the transfer shared memory.
  void *transfer_shm_address() const { return transfer_shm_address_; }

  // Gets the allocator of the transfer shared memory.
  FencedAllocatorWrapper *allocator() const {
    return allocator_;
  }

  // Overridden from Renderer.
  virtual const int* GetRGBAUByteNSwizzleTable();

  command_buffer::parse_error::ParseError GetParseError();

 protected:
  // Protected so that callers are forced to call the factory method.
  RendererCB(ServiceLocator* service_locator, int32 transfer_memory_size);

  // Overridden from Renderer.
  virtual bool PlatformSpecificBeginDraw();

  // Overridden from Renderer.
  virtual void PlatformSpecificEndDraw();

  // Overridden from Renderer.
  virtual bool PlatformSpecificStartRendering();

  // Overridden from Renderer.
  virtual void PlatformSpecificFinishRendering();

  // Overridden from Renderer.
  virtual void PlatformSpecificPresent();

  // Overridden from Renderer.
  virtual void PlatformSpecificClear(const Float4 &color,
                                     bool color_flag,
                                     float depth,
                                     bool depth_flag,
                                     int stencil,
                                     bool stencil_flag);

  // Creates a platform specific ParamCache.
  virtual ParamCache* CreatePlatformSpecificParamCache();

  // Sets the viewport. This is the platform specific version.
  virtual void SetViewportInPixels(int left,
                                   int top,
                                   int width,
                                   int height,
                                   float min_z,
                                   float max_z);

  // Overridden from Renderer.
  virtual void SetBackBufferPlatformSpecific();

  // Overridden from Renderer.
  virtual void SetRenderSurfacesPlatformSpecific(
      const RenderSurface* surface,
      const RenderDepthStencilSurface* depth_surface);

  // Overridden from Renderer.
  virtual Texture2D::Ref CreatePlatformSpecificTexture2D(
      int width,
      int height,
      Texture::Format format,
      int levels,
      bool enable_render_surfaces);

  // Overridden from Renderer.
  virtual TextureCUBE::Ref CreatePlatformSpecificTextureCUBE(
      int edge_length,
      Texture::Format format,
      int levels,
      bool enable_render_surfaces);

  // Overridden from Renderer.
  virtual void ApplyDirtyStates();

 protected:
  // Initializes the renderer for use, claiming hardware resources.
  virtual InitStatus InitPlatformSpecific(const DisplayWindow& display_window,
                                          bool off_screen);

  // Create a shared memory object of the given size.
  virtual gpu_plugin::NPObjectPointer<NPObject>
      CreateSharedMemory(int32 size, NPP npp) = 0;

 private:
  int32 transfer_memory_size_;
  gpu_plugin::NPObjectPointer<NPObject> transfer_shm_;
  int32 transfer_shm_id_;
  void *transfer_shm_address_;
  NPP npp_;
  gpu_plugin::NPObjectPointer<NPObject> command_buffer_;
  command_buffer::CommandBufferHelper *helper_;
  FencedAllocatorWrapper *allocator_;

  IdAllocator vertex_buffer_ids_;
  IdAllocator index_buffer_ids_;
  IdAllocator vertex_structs_ids_;
  IdAllocator effect_ids_;
  IdAllocator effect_param_ids_;
  IdAllocator texture_ids_;
  IdAllocator sampler_ids_;
  IdAllocator render_surface_ids_;
  IdAllocator depth_surface_ids_;
  int32 frame_token_;

  class StateManager;
  scoped_ptr<StateManager> state_manager_;

  DISALLOW_COPY_AND_ASSIGN(RendererCB);
};

// This subclass initializes itself with a locally created in-process
// CommandBuffer and GPUProcessor. This class will eventually go away and the
// code in RendererCBRemote will be merged into RendererCB.
class RendererCBLocal : public RendererCB {
 public:
  static gpu_plugin::NPObjectPointer<gpu_plugin::CommandBuffer>
      CreateCommandBuffer(NPP npp, void* hwnd, int32 size);

  // Creates a default RendererCBLocal.
  static RendererCBLocal *CreateDefault(ServiceLocator* service_locator);

 protected:
  RendererCBLocal(ServiceLocator* service_locator,
                  int32 transfer_memory_size);
  virtual ~RendererCBLocal();

  // Create a shared memory object of the given size using the system services
  // library directly.
  virtual gpu_plugin::NPObjectPointer<NPObject>
      CreateSharedMemory(int32 size, NPP npp);

 private:
  DISALLOW_COPY_AND_ASSIGN(RendererCBLocal);
};

// This subclass initializes itself with a remotely created, potentially out-
// of-process CommandBuffer. It requires that the browser supports the "system
// service" to create shared memory, which is not available in the mange branch
// of Chrome. Use RendererCBLocal for now.
class RendererCBRemote : public RendererCB {
 public:
  // Creates a default RendererCBRemote.
  static RendererCBRemote *CreateDefault(ServiceLocator* service_locator);

 protected:
  RendererCBRemote(ServiceLocator* service_locator, int32 transfer_memory_size);
  virtual ~RendererCBRemote();

  // Create a shared memory object using the browser's
  // chromium.system.createSharedMemory method.
  virtual gpu_plugin::NPObjectPointer<NPObject>
      CreateSharedMemory(int32 size, NPP npp);

 private:
  DISALLOW_COPY_AND_ASSIGN(RendererCBRemote);
};

}  // namespace o3d

#endif  // O3D_CORE_CROSS_COMMAND_BUFFER_RENDERER_CB_H_
