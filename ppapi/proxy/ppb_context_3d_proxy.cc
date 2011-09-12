// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_context_3d_proxy.h"

#include "base/hash_tables.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/dev/ppb_context_3d_dev.h"
#include "ppapi/c/dev/ppb_context_3d_trusted_dev.h"
#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_surface_3d_proxy.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::thunk::EnterFunctionNoLock;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Context3D_API;
using ppapi::thunk::PPB_Surface3D_API;
using ppapi::thunk::ResourceCreationAPI;

namespace ppapi {
namespace proxy {

namespace {

base::SharedMemoryHandle TransportSHMHandleFromInt(Dispatcher* dispatcher,
                                                   int shm_handle) {
  // TODO(piman): Change trusted interface to return a PP_FileHandle, those
  // casts are ugly.
  base::PlatformFile source =
#if defined(OS_WIN)
      reinterpret_cast<HANDLE>(static_cast<intptr_t>(shm_handle));
#elif defined(OS_POSIX)
      shm_handle;
#else
  #error Not implemented.
#endif
  // Don't close the handle, it doesn't belong to us.
  return dispatcher->ShareHandleWithRemote(source, false);
}

PP_Context3DTrustedState GetErrorState() {
  PP_Context3DTrustedState error_state = { 0 };
  error_state.error = kGenericError;
  return error_state;
}

gpu::CommandBuffer::State GPUStateFromPPState(
    const PP_Context3DTrustedState& s) {
  gpu::CommandBuffer::State state;
  state.num_entries = s.num_entries;
  state.get_offset = s.get_offset;
  state.put_offset = s.put_offset;
  state.token = s.token;
  state.error = static_cast<gpu::error::Error>(s.error);
  state.generation = s.generation;
  return state;
}

// Size of the transfer buffer.
const int32 kCommandBufferSize = 1024 * 1024;
const int32 kTransferBufferSize = 1024 * 1024;

}  // namespace

class PepperCommandBuffer : public gpu::CommandBuffer {
 public:
  PepperCommandBuffer(const HostResource& resource,
                      PluginDispatcher* dispatcher);
  virtual ~PepperCommandBuffer();

  // CommandBuffer implementation:
  virtual bool Initialize(int32 size);
  virtual bool Initialize(base::SharedMemory* buffer, int32 size);
  virtual gpu::Buffer GetRingBuffer();
  virtual State GetState();
  virtual State GetLastState();
  virtual void Flush(int32 put_offset);
  virtual State FlushSync(int32 put_offset, int32 last_known_get);
  virtual void SetGetOffset(int32 get_offset);
  virtual int32 CreateTransferBuffer(size_t size, int32 id_request);
  virtual int32 RegisterTransferBuffer(base::SharedMemory* shared_memory,
                                       size_t size,
                                       int32 id_request);
  virtual void DestroyTransferBuffer(int32 id);
  virtual gpu::Buffer GetTransferBuffer(int32 handle);
  virtual void SetToken(int32 token);
  virtual void SetParseError(gpu::error::Error error);
  virtual void SetContextLostReason(gpu::error::ContextLostReason reason);

 private:
  bool Send(IPC::Message* msg);
  void UpdateState(const gpu::CommandBuffer::State& state);

  int32 num_entries_;
  scoped_ptr<base::SharedMemory> ring_buffer_;

  typedef base::hash_map<int32, gpu::Buffer> TransferBufferMap;
  TransferBufferMap transfer_buffers_;

  State last_state_;

  HostResource resource_;
  PluginDispatcher* dispatcher_;

  DISALLOW_COPY_AND_ASSIGN(PepperCommandBuffer);
};

PepperCommandBuffer::PepperCommandBuffer(
    const HostResource& resource,
    PluginDispatcher* dispatcher)
    : num_entries_(0),
      resource_(resource),
      dispatcher_(dispatcher) {
}

PepperCommandBuffer::~PepperCommandBuffer() {
  // Delete all the locally cached shared memory objects, closing the handle
  // in this process.
  for (TransferBufferMap::iterator it = transfer_buffers_.begin();
       it != transfer_buffers_.end();
       ++it) {
    delete it->second.shared_memory;
    it->second.shared_memory = NULL;
  }
}

bool PepperCommandBuffer::Initialize(int32 size) {
  DCHECK(!ring_buffer_.get());

  // Initialize the service. Assuming we are sandboxed, the GPU
  // process is responsible for duplicating the handle. This might not be true
  // for NaCl.
  base::SharedMemoryHandle handle;
  if (Send(new PpapiHostMsg_PPBContext3D_Initialize(
          INTERFACE_ID_PPB_CONTEXT_3D, resource_, size, &handle)) &&
      base::SharedMemory::IsHandleValid(handle)) {
    ring_buffer_.reset(new base::SharedMemory(handle, false));
    if (ring_buffer_->Map(size)) {
      num_entries_ = size / sizeof(gpu::CommandBufferEntry);
      return true;
    }

    ring_buffer_.reset();
  }

  return false;
}

bool PepperCommandBuffer::Initialize(base::SharedMemory* buffer, int32 size) {
  // Not implemented in proxy.
  NOTREACHED();
  return false;
}

gpu::Buffer PepperCommandBuffer::GetRingBuffer() {
  // Return locally cached ring buffer.
  gpu::Buffer buffer;
  buffer.ptr = ring_buffer_->memory();
  buffer.size = num_entries_ * sizeof(gpu::CommandBufferEntry);
  buffer.shared_memory = ring_buffer_.get();
  return buffer;
}

gpu::CommandBuffer::State PepperCommandBuffer::GetState() {
  // Send will flag state with lost context if IPC fails.
  if (last_state_.error == gpu::error::kNoError) {
    gpu::CommandBuffer::State state;
    if (Send(new PpapiHostMsg_PPBContext3D_GetState(
             INTERFACE_ID_PPB_CONTEXT_3D, resource_, &state)))
      UpdateState(state);
  }

  return last_state_;
}

gpu::CommandBuffer::State PepperCommandBuffer::GetLastState() {
  return last_state_;
}

void PepperCommandBuffer::Flush(int32 put_offset) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  IPC::Message* message = new PpapiHostMsg_PPBContext3D_AsyncFlush(
      INTERFACE_ID_PPB_CONTEXT_3D, resource_, put_offset);

  // Do not let a synchronous flush hold up this message. If this handler is
  // deferred until after the synchronous flush completes, it will overwrite the
  // cached last_state_ with out-of-date data.
  message->set_unblock(true);
  Send(message);
}

gpu::CommandBuffer::State PepperCommandBuffer::FlushSync(
    int32 put_offset, int32 last_known_get) {
  if (last_known_get == last_state_.get_offset) {
    // Send will flag state with lost context if IPC fails.
    if (last_state_.error == gpu::error::kNoError) {
      gpu::CommandBuffer::State state;
      if (Send(new PpapiHostMsg_PPBContext3D_Flush(
              INTERFACE_ID_PPB_CONTEXT_3D, resource_, put_offset,
              last_known_get, &state)))
        UpdateState(state);
    }
  } else {
    Flush(put_offset);
  }

  return last_state_;
}

void PepperCommandBuffer::SetGetOffset(int32 get_offset) {
  // Not implemented in proxy.
  NOTREACHED();
}

int32 PepperCommandBuffer::CreateTransferBuffer(size_t size, int32 id_request) {
  if (last_state_.error == gpu::error::kNoError) {
    int32 id;
    if (Send(new PpapiHostMsg_PPBContext3D_CreateTransferBuffer(
            INTERFACE_ID_PPB_CONTEXT_3D, resource_, size, &id))) {
      return id;
    }
  }

  return -1;
}

int32 PepperCommandBuffer::RegisterTransferBuffer(
    base::SharedMemory* shared_memory,
    size_t size,
    int32 id_request) {
  // Not implemented in proxy.
  NOTREACHED();
  return -1;
}

void PepperCommandBuffer::DestroyTransferBuffer(int32 id) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  // Remove the transfer buffer from the client side4 cache.
  TransferBufferMap::iterator it = transfer_buffers_.find(id);
  DCHECK(it != transfer_buffers_.end());

  // Delete the shared memory object, closing the handle in this process.
  delete it->second.shared_memory;

  transfer_buffers_.erase(it);

  Send(new PpapiHostMsg_PPBContext3D_DestroyTransferBuffer(
      INTERFACE_ID_PPB_CONTEXT_3D, resource_, id));
}

gpu::Buffer PepperCommandBuffer::GetTransferBuffer(int32 id) {
  if (last_state_.error != gpu::error::kNoError)
    return gpu::Buffer();

  // Check local cache to see if there is already a client side shared memory
  // object for this id.
  TransferBufferMap::iterator it = transfer_buffers_.find(id);
  if (it != transfer_buffers_.end()) {
    return it->second;
  }

  // Assuming we are in the renderer process, the service is responsible for
  // duplicating the handle. This might not be true for NaCl.
  base::SharedMemoryHandle handle;
  uint32 size;
  if (!Send(new PpapiHostMsg_PPBContext3D_GetTransferBuffer(
          INTERFACE_ID_PPB_CONTEXT_3D, resource_, id, &handle, &size))) {
    return gpu::Buffer();
  }

  // Cache the transfer buffer shared memory object client side.
  scoped_ptr<base::SharedMemory> shared_memory(
      new base::SharedMemory(handle, false));

  // Map the shared memory on demand.
  if (!shared_memory->memory()) {
    if (!shared_memory->Map(size)) {
      return gpu::Buffer();
    }
  }

  gpu::Buffer buffer;
  buffer.ptr = shared_memory->memory();
  buffer.size = size;
  buffer.shared_memory = shared_memory.release();
  transfer_buffers_[id] = buffer;

  return buffer;
}

void PepperCommandBuffer::SetToken(int32 token) {
  NOTREACHED();
}

void PepperCommandBuffer::SetParseError(gpu::error::Error error) {
  NOTREACHED();
}

void PepperCommandBuffer::SetContextLostReason(
    gpu::error::ContextLostReason reason) {
  NOTREACHED();
}

bool PepperCommandBuffer::Send(IPC::Message* msg) {
  DCHECK(last_state_.error == gpu::error::kNoError);

  if (dispatcher_->Send(msg))
    return true;

  last_state_.error = gpu::error::kLostContext;
  return false;
}

void PepperCommandBuffer::UpdateState(const gpu::CommandBuffer::State& state) {
  // Handle wraparound. It works as long as we don't have more than 2B state
  // updates in flight across which reordering occurs.
  if (state.generation - last_state_.generation < 0x80000000U)
    last_state_ = state;
}

// Context3D -------------------------------------------------------------------

Context3D::Context3D(const HostResource& resource)
    : Resource(resource),
      draw_(NULL),
      read_(NULL),
      transfer_buffer_id_(0) {
}

Context3D::~Context3D() {
  if (draw_)
    draw_->set_context(NULL);
}

PPB_Context3D_API* Context3D::AsPPB_Context3D_API() {
  return this;
}

bool Context3D::CreateImplementation() {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForResource(this);
  if (!dispatcher)
    return false;

  command_buffer_.reset(new PepperCommandBuffer(host_resource(), dispatcher));

  if (!command_buffer_->Initialize(kCommandBufferSize))
    return false;

  helper_.reset(new gpu::gles2::GLES2CmdHelper(command_buffer_.get()));
  if (!helper_->Initialize(kCommandBufferSize))
    return false;

  transfer_buffer_id_ =
      command_buffer_->CreateTransferBuffer(kTransferBufferSize, -1);
  if (transfer_buffer_id_ < 0)
    return false;

  gpu::Buffer transfer_buffer =
      command_buffer_->GetTransferBuffer(transfer_buffer_id_);
  if (!transfer_buffer.ptr)
    return false;

  gles2_impl_.reset(new gpu::gles2::GLES2Implementation(
      helper_.get(),
      transfer_buffer.size,
      transfer_buffer.ptr,
      transfer_buffer_id_,
      false,
      true));

  return true;
}

int32_t Context3D::GetAttrib(int32_t attribute, int32_t* value) {
  // TODO(alokp): Implement me.
  return 0;
}

int32_t Context3D::BindSurfaces(PP_Resource pp_draw, PP_Resource pp_read) {
  // TODO(alokp): Support separate draw-read surfaces.
  DCHECK_EQ(pp_draw, pp_read);
  if (pp_draw != pp_read)
    return PP_ERROR_BADARGUMENT;

  EnterResourceNoLock<PPB_Surface3D_API> enter_draw(pp_draw, false);
  EnterResourceNoLock<PPB_Surface3D_API> enter_read(pp_read, false);
  Surface3D* draw_surface = enter_draw.succeeded() ?
      static_cast<Surface3D*>(enter_draw.object()) : NULL;
  Surface3D* read_surface = enter_read.succeeded() ?
      static_cast<Surface3D*>(enter_read.object()) : NULL;

  if (pp_draw && !draw_surface)
    return PP_ERROR_BADRESOURCE;
  if (pp_read && !read_surface)
    return PP_ERROR_BADRESOURCE;
  HostResource host_draw =
      draw_surface ? draw_surface->host_resource() : HostResource();
  HostResource host_read =
      read_surface ? read_surface->host_resource() : HostResource();

  int32_t result;
  PluginDispatcher::GetForResource(this)->Send(
      new PpapiHostMsg_PPBContext3D_BindSurfaces(
          INTERFACE_ID_PPB_CONTEXT_3D,
          host_resource(), host_draw, host_read, &result));
  if (result != PP_OK)
    return result;

  if (draw_surface != draw_) {
    if (draw_)
      draw_->set_context(NULL);
    if (draw_surface) {
      draw_surface->set_context(this);
      // Resize the backing texture to the size of the instance when it is
      // bound.
      // TODO(alokp): This should be the responsibility of plugins.
      InstanceData* data =
          PluginDispatcher::GetForResource(this)->GetInstanceData(
              pp_instance());
      gles2_impl()->ResizeCHROMIUM(data->position.size.width,
                                   data->position.size.height);
    }
    draw_ = draw_surface;
  }
  read_ = read_surface;
  return PP_OK;
}

int32_t Context3D::GetBoundSurfaces(PP_Resource* draw, PP_Resource* read) {
  *draw = draw_ ? draw_->pp_resource() : 0;
  *read = read_ ? read_->pp_resource() : 0;
  return PP_OK;
}

PP_Bool Context3D::InitializeTrusted(int32_t size) {
  // Trusted interface not implemented in the proxy.
  return PP_FALSE;
}

PP_Bool Context3D::GetRingBuffer(int* shm_handle,
                                 uint32_t* shm_size) {
  // Trusted interface not implemented in the proxy.
  return PP_FALSE;
}

PP_Context3DTrustedState Context3D::GetState() {
  // Trusted interface not implemented in the proxy.
  return GetErrorState();
}

PP_Bool Context3D::Flush(int32_t put_offset) {
  // Trusted interface not implemented in the proxy.
  return PP_FALSE;
}

PP_Context3DTrustedState Context3D::FlushSync(int32_t put_offset) {
  // Trusted interface not implemented in the proxy.
  return GetErrorState();
}

int32_t Context3D::CreateTransferBuffer(uint32_t size) {
  // Trusted interface not implemented in the proxy.
  return 0;
}

PP_Bool Context3D::DestroyTransferBuffer(int32_t id) {
  // Trusted interface not implemented in the proxy.
  return PP_FALSE;
}

PP_Bool Context3D::GetTransferBuffer(int32_t id,
                                     int* shm_handle,
                                     uint32_t* shm_size) {
  // Trusted interface not implemented in the proxy.
  return PP_FALSE;
}

PP_Context3DTrustedState Context3D::FlushSyncFast(int32_t put_offset,
                                                  int32_t last_known_get) {
  // Trusted interface not implemented in the proxy.
  return GetErrorState();
}

void* Context3D::MapTexSubImage2DCHROMIUM(GLenum target,
                                          GLint level,
                                          GLint xoffset,
                                          GLint yoffset,
                                          GLsizei width,
                                          GLsizei height,
                                          GLenum format,
                                          GLenum type,
                                          GLenum access) {
  return gles2_impl_->MapTexSubImage2DCHROMIUM(
      target, level, xoffset, yoffset, width, height, format, type, access);
}

void Context3D::UnmapTexSubImage2DCHROMIUM(const void* mem) {
  gles2_impl_->UnmapTexSubImage2DCHROMIUM(mem);
}

gpu::gles2::GLES2Implementation* Context3D::GetGLES2Impl() {
  return gles2_impl();
}

// PPB_Context3D_Proxy ---------------------------------------------------------

PPB_Context3D_Proxy::PPB_Context3D_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

PPB_Context3D_Proxy::~PPB_Context3D_Proxy() {
}

// static
PP_Resource PPB_Context3D_Proxy::Create(PP_Instance instance,
                                        PP_Config3D_Dev config,
                                        PP_Resource share_context,
                                        const int32_t* attrib_list) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return PP_ERROR_BADARGUMENT;

  // TODO(alokp): Support shared context.
  DCHECK_EQ(0, share_context);
  if (share_context != 0)
    return 0;

  std::vector<int32_t> attribs;
  if (attrib_list) {
    for (const int32_t* attr = attrib_list; attr; ++attr)
      attribs.push_back(*attr);
  } else {
    attribs.push_back(0);
  }

  HostResource result;
  dispatcher->Send(new PpapiHostMsg_PPBContext3D_Create(
      INTERFACE_ID_PPB_CONTEXT_3D, instance, config, attribs, &result));

  if (result.is_null())
    return 0;
  scoped_refptr<Context3D> context_3d(new Context3D(result));
  if (!context_3d->CreateImplementation())
    return 0;
  return context_3d->GetReference();
}

bool PPB_Context3D_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Context3D_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBContext3D_Create,
                        OnMsgCreate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBContext3D_BindSurfaces,
                        OnMsgBindSurfaces)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBContext3D_Initialize,
                        OnMsgInitialize)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBContext3D_GetState,
                        OnMsgGetState)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBContext3D_Flush,
                        OnMsgFlush)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBContext3D_AsyncFlush,
                        OnMsgAsyncFlush)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBContext3D_CreateTransferBuffer,
                        OnMsgCreateTransferBuffer)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBContext3D_DestroyTransferBuffer,
                        OnMsgDestroyTransferBuffer)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBContext3D_GetTransferBuffer,
                        OnMsgGetTransferBuffer)
    IPC_MESSAGE_UNHANDLED(handled = false)

  IPC_END_MESSAGE_MAP()
  // FIXME(brettw) handle bad messages!
  return handled;
}

void PPB_Context3D_Proxy::OnMsgCreate(PP_Instance instance,
                                      PP_Config3D_Dev config,
                                      const std::vector<int32_t>& attribs,
                                     HostResource* result) {
  if (attribs.empty() || attribs.back() != 0)
    return;  // Bad message.
  thunk::EnterResourceCreation enter(instance);
  if (enter.succeeded()) {
    result->SetHostResource(
        instance,
        enter.functions()->CreateContext3DRaw(instance, config, 0,
                                              &attribs.front()));
  }
}

void PPB_Context3D_Proxy::OnMsgBindSurfaces(const HostResource& context,
                                            const HostResource& draw,
                                            const HostResource& read,
                                            int32_t* result) {
  EnterHostFromHostResource<PPB_Context3D_API> enter(context);
  if (enter.succeeded()) {
    *result = enter.object()->BindSurfaces(draw.host_resource(),
                                           read.host_resource());
  } else {
    *result = PP_ERROR_BADRESOURCE;
  }
}

void PPB_Context3D_Proxy::OnMsgInitialize(
    const HostResource& context,
    int32 size,
    base::SharedMemoryHandle* ring_buffer) {
  *ring_buffer = base::SharedMemory::NULLHandle();
  EnterHostFromHostResource<PPB_Context3D_API> enter(context);
  if (enter.failed())
    return;

  if (!enter.object()->InitializeTrusted(size))
    return;

  int shm_handle;
  uint32_t shm_size;
  if (!enter.object()->GetRingBuffer(&shm_handle, &shm_size))
    return;
  *ring_buffer = TransportSHMHandleFromInt(dispatcher(), shm_handle);
}

void PPB_Context3D_Proxy::OnMsgGetState(const HostResource& context,
                                        gpu::CommandBuffer::State* state) {
  EnterHostFromHostResource<PPB_Context3D_API> enter(context);
  if (enter.failed())
    return;
  PP_Context3DTrustedState pp_state = enter.object()->GetState();
  *state = GPUStateFromPPState(pp_state);
}

void PPB_Context3D_Proxy::OnMsgFlush(const HostResource& context,
                                     int32 put_offset,
                                     int32 last_known_get,
                                     gpu::CommandBuffer::State* state) {
  EnterHostFromHostResource<PPB_Context3D_API> enter(context);
  if (enter.failed())
    return;
  PP_Context3DTrustedState pp_state = enter.object()->FlushSyncFast(
      put_offset, last_known_get);
  *state = GPUStateFromPPState(pp_state);
}

void PPB_Context3D_Proxy::OnMsgAsyncFlush(const HostResource& context,
                                          int32 put_offset) {
  EnterHostFromHostResource<PPB_Context3D_API> enter(context);
  if (enter.succeeded())
    enter.object()->Flush(put_offset);
}

void PPB_Context3D_Proxy::OnMsgCreateTransferBuffer(
    const HostResource& context,
    int32 size,
    int32* id) {
  EnterHostFromHostResource<PPB_Context3D_API> enter(context);
  if (enter.succeeded())
    *id = enter.object()->CreateTransferBuffer(size);
  else
    *id = 0;
}

void PPB_Context3D_Proxy::OnMsgDestroyTransferBuffer(
    const HostResource& context,
    int32 id) {
  EnterHostFromHostResource<PPB_Context3D_API> enter(context);
  if (enter.succeeded())
    enter.object()->DestroyTransferBuffer(id);
}

void PPB_Context3D_Proxy::OnMsgGetTransferBuffer(
    const HostResource& context,
    int32 id,
    base::SharedMemoryHandle* transfer_buffer,
    uint32* size) {
  *transfer_buffer = base::SharedMemory::NULLHandle();
  *size = 0;

  EnterHostFromHostResource<PPB_Context3D_API> enter(context);
  int shm_handle = 0;
  uint32_t shm_size = 0;
  if (enter.succeeded() &&
      enter.object()->GetTransferBuffer(id, &shm_handle, &shm_size)) {
    *transfer_buffer = TransportSHMHandleFromInt(dispatcher(), shm_handle);
    *size = shm_size;
  }
}

}  // namespace proxy
}  // namespace ppapi
