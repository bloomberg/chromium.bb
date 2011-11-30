// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_graphics_3d_proxy.h"

#include "gpu/command_buffer/client/gles2_implementation.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::thunk::EnterFunctionNoLock;
using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Graphics3D_API;
using ppapi::thunk::ResourceCreationAPI;

namespace ppapi {
namespace proxy {

namespace {
const int32 kCommandBufferSize = 1024 * 1024;
const int32 kTransferBufferSize = 1024 * 1024;

class CommandBuffer : public gpu::CommandBuffer {
 public:
  CommandBuffer(const HostResource& resource, PluginDispatcher* dispatcher);
  virtual ~CommandBuffer();

  // gpu::CommandBuffer implementation:
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

  DISALLOW_COPY_AND_ASSIGN(CommandBuffer);
};

CommandBuffer::CommandBuffer(const HostResource& resource,
                             PluginDispatcher* dispatcher)
    : num_entries_(0),
      resource_(resource),
      dispatcher_(dispatcher) {
}

CommandBuffer::~CommandBuffer() {
  // Delete all the locally cached shared memory objects, closing the handle
  // in this process.
  for (TransferBufferMap::iterator it = transfer_buffers_.begin();
       it != transfer_buffers_.end(); ++it) {
    delete it->second.shared_memory;
    it->second.shared_memory = NULL;
  }
}

bool CommandBuffer::Initialize(int32 size) {
  DCHECK(!ring_buffer_.get());

  // Initialize the service. Assuming we are sandboxed, the GPU
  // process is responsible for duplicating the handle. This might not be true
  // for NaCl.
  base::SharedMemoryHandle handle;
  if (Send(new PpapiHostMsg_PPBGraphics3D_InitCommandBuffer(
          API_ID_PPB_GRAPHICS_3D, resource_, size, &handle)) &&
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

bool CommandBuffer::Initialize(base::SharedMemory* buffer, int32 size) {
  // Not implemented in proxy.
  NOTREACHED();
  return false;
}

gpu::Buffer CommandBuffer::GetRingBuffer() {
  // Return locally cached ring buffer.
  gpu::Buffer buffer;
  if (ring_buffer_.get()) {
    buffer.ptr = ring_buffer_->memory();
    buffer.size = num_entries_ * sizeof(gpu::CommandBufferEntry);
    buffer.shared_memory = ring_buffer_.get();
  } else {
    buffer.ptr = NULL;
    buffer.size = 0;
    buffer.shared_memory = NULL;
  }
  return buffer;
}

gpu::CommandBuffer::State CommandBuffer::GetState() {
  // Send will flag state with lost context if IPC fails.
  if (last_state_.error == gpu::error::kNoError) {
    gpu::CommandBuffer::State state;
    if (Send(new PpapiHostMsg_PPBGraphics3D_GetState(
             API_ID_PPB_GRAPHICS_3D, resource_, &state)))
      UpdateState(state);
  }

  return last_state_;
}

gpu::CommandBuffer::State CommandBuffer::GetLastState() {
  return last_state_;
}

void CommandBuffer::Flush(int32 put_offset) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  IPC::Message* message = new PpapiHostMsg_PPBGraphics3D_AsyncFlush(
      API_ID_PPB_GRAPHICS_3D, resource_, put_offset);

  // Do not let a synchronous flush hold up this message. If this handler is
  // deferred until after the synchronous flush completes, it will overwrite the
  // cached last_state_ with out-of-date data.
  message->set_unblock(true);
  Send(message);
}

gpu::CommandBuffer::State CommandBuffer::FlushSync(int32 put_offset,
                                                   int32 last_known_get) {
  if (last_known_get == last_state_.get_offset) {
    // Send will flag state with lost context if IPC fails.
    if (last_state_.error == gpu::error::kNoError) {
      gpu::CommandBuffer::State state;
      if (Send(new PpapiHostMsg_PPBGraphics3D_Flush(
              API_ID_PPB_GRAPHICS_3D, resource_, put_offset,
              last_known_get, &state)))
        UpdateState(state);
    }
  } else {
    Flush(put_offset);
  }

  return last_state_;
}

void CommandBuffer::SetGetOffset(int32 get_offset) {
  // Not implemented in proxy.
  NOTREACHED();
}

int32 CommandBuffer::CreateTransferBuffer(size_t size, int32 id_request) {
  if (last_state_.error == gpu::error::kNoError) {
    int32 id;
    if (Send(new PpapiHostMsg_PPBGraphics3D_CreateTransferBuffer(
            API_ID_PPB_GRAPHICS_3D, resource_, size, &id))) {
      return id;
    }
  }

  return -1;
}

int32 CommandBuffer::RegisterTransferBuffer(
    base::SharedMemory* shared_memory,
    size_t size,
    int32 id_request) {
  // Not implemented in proxy.
  NOTREACHED();
  return -1;
}

void CommandBuffer::DestroyTransferBuffer(int32 id) {
  if (last_state_.error != gpu::error::kNoError)
    return;

  // Remove the transfer buffer from the client side4 cache.
  TransferBufferMap::iterator it = transfer_buffers_.find(id);
  DCHECK(it != transfer_buffers_.end());

  // Delete the shared memory object, closing the handle in this process.
  delete it->second.shared_memory;

  transfer_buffers_.erase(it);

  Send(new PpapiHostMsg_PPBGraphics3D_DestroyTransferBuffer(
      API_ID_PPB_GRAPHICS_3D, resource_, id));
}

gpu::Buffer CommandBuffer::GetTransferBuffer(int32 id) {
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
  if (!Send(new PpapiHostMsg_PPBGraphics3D_GetTransferBuffer(
          API_ID_PPB_GRAPHICS_3D, resource_, id, &handle, &size))) {
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

void CommandBuffer::SetToken(int32 token) {
  NOTREACHED();
}

void CommandBuffer::SetParseError(gpu::error::Error error) {
  NOTREACHED();
}

void CommandBuffer::SetContextLostReason(
    gpu::error::ContextLostReason reason) {
  NOTREACHED();
}

bool CommandBuffer::Send(IPC::Message* msg) {
  DCHECK(last_state_.error == gpu::error::kNoError);

  if (dispatcher_->Send(msg))
    return true;

  last_state_.error = gpu::error::kLostContext;
  return false;
}

void CommandBuffer::UpdateState(const gpu::CommandBuffer::State& state) {
  // Handle wraparound. It works as long as we don't have more than 2B state
  // updates in flight across which reordering occurs.
  if (state.generation - last_state_.generation < 0x80000000U)
    last_state_ = state;
}

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

PP_Graphics3DTrustedState GetErrorState() {
  PP_Graphics3DTrustedState error_state = { 0 };
  error_state.error = PPB_GRAPHICS3D_TRUSTED_ERROR_GENERICERROR;
  return error_state;
}

gpu::CommandBuffer::State GPUStateFromPPState(
    const PP_Graphics3DTrustedState& s) {
  gpu::CommandBuffer::State state;
  state.num_entries = s.num_entries;
  state.get_offset = s.get_offset;
  state.put_offset = s.put_offset;
  state.token = s.token;
  state.error = static_cast<gpu::error::Error>(s.error);
  state.generation = s.generation;
  return state;
}

}  // namespace

Graphics3D::Graphics3D(const HostResource& resource)
    : Resource(resource) {
}

Graphics3D::~Graphics3D() {
  DestroyGLES2Impl();
}

bool Graphics3D::Init() {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForResource(this);
  if (!dispatcher)
    return false;

  command_buffer_.reset(new CommandBuffer(host_resource(), dispatcher));
  if (!command_buffer_->Initialize(kCommandBufferSize))
    return false;

  return CreateGLES2Impl(kCommandBufferSize, kTransferBufferSize);
}

PP_Bool Graphics3D::InitCommandBuffer(int32_t size) {
  return PP_FALSE;
}

PP_Bool Graphics3D::GetRingBuffer(int* shm_handle, uint32_t* shm_size) {
  return PP_FALSE;
}

PP_Graphics3DTrustedState Graphics3D::GetState() {
  return GetErrorState();
}

PP_Bool Graphics3D::Flush(int32_t put_offset) {
  return PP_FALSE;
}

PP_Graphics3DTrustedState Graphics3D::FlushSync(int32_t put_offset) {
  return GetErrorState();
}

int32_t Graphics3D::CreateTransferBuffer(uint32_t size) {
  return PP_FALSE;
}

PP_Bool Graphics3D::DestroyTransferBuffer(int32_t id) {
  return PP_FALSE;
}

PP_Bool Graphics3D::GetTransferBuffer(int32_t id,
                                      int* shm_handle,
                                      uint32_t* shm_size) {
  return PP_FALSE;
}

PP_Graphics3DTrustedState Graphics3D::FlushSyncFast(int32_t put_offset,
                                                    int32_t last_known_get) {
  return GetErrorState();
}

gpu::CommandBuffer* Graphics3D::GetCommandBuffer() {
  return command_buffer_.get();
}

int32 Graphics3D::DoSwapBuffers() {
  gles2_impl()->SwapBuffers();
  IPC::Message* msg = new PpapiHostMsg_PPBGraphics3D_SwapBuffers(
      API_ID_PPB_GRAPHICS_3D, host_resource());
  msg->set_unblock(true);
  PluginDispatcher::GetForResource(this)->Send(msg);

  return PP_OK_COMPLETIONPENDING;
}

PPB_Graphics3D_Proxy::PPB_Graphics3D_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

PPB_Graphics3D_Proxy::~PPB_Graphics3D_Proxy() {
}

// static
PP_Resource PPB_Graphics3D_Proxy::CreateProxyResource(
    PP_Instance instance,
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
    for (const int32_t* attr = attrib_list;
         attr[0] != PP_GRAPHICS3DATTRIB_NONE;
         attr += 2) {
      attribs.push_back(attr[0]);
      attribs.push_back(attr[1]);
    }
  }
  attribs.push_back(PP_GRAPHICS3DATTRIB_NONE);

  HostResource result;
  dispatcher->Send(new PpapiHostMsg_PPBGraphics3D_Create(
      API_ID_PPB_GRAPHICS_3D, instance, attribs, &result));
  if (result.is_null())
    return 0;

  scoped_refptr<Graphics3D> graphics_3d(new Graphics3D(result));
  if (!graphics_3d->Init())
    return 0;
  return graphics_3d->GetReference();
}

bool PPB_Graphics3D_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Graphics3D_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_Create,
                        OnMsgCreate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_InitCommandBuffer,
                        OnMsgInitCommandBuffer)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_GetState,
                        OnMsgGetState)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_Flush,
                        OnMsgFlush)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_AsyncFlush,
                        OnMsgAsyncFlush)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_CreateTransferBuffer,
                        OnMsgCreateTransferBuffer)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_DestroyTransferBuffer,
                        OnMsgDestroyTransferBuffer)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_GetTransferBuffer,
                        OnMsgGetTransferBuffer)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_SwapBuffers,
                        OnMsgSwapBuffers)

    IPC_MESSAGE_HANDLER(PpapiMsg_PPBGraphics3D_SwapBuffersACK,
                        OnMsgSwapBuffersACK)
    IPC_MESSAGE_UNHANDLED(handled = false)

  IPC_END_MESSAGE_MAP()
  // FIXME(brettw) handle bad messages!
  return handled;
}

void PPB_Graphics3D_Proxy::OnMsgCreate(PP_Instance instance,
                                       const std::vector<int32_t>& attribs,
                                       HostResource* result) {
  if (attribs.empty() || attribs.back() != PP_GRAPHICS3DATTRIB_NONE)
    return;  // Bad message.

  thunk::EnterResourceCreation enter(instance);
  if (enter.succeeded()) {
    result->SetHostResource(
        instance,
        enter.functions()->CreateGraphics3DRaw(instance, 0, &attribs.front()));
  }
}

void PPB_Graphics3D_Proxy::OnMsgInitCommandBuffer(
    const HostResource& context,
    int32 size,
    base::SharedMemoryHandle* ring_buffer) {
  *ring_buffer = base::SharedMemory::NULLHandle();
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.failed())
    return;

  if (!enter.object()->InitCommandBuffer(size))
    return;

  int shm_handle;
  uint32_t shm_size;
  if (!enter.object()->GetRingBuffer(&shm_handle, &shm_size))
    return;
  *ring_buffer = TransportSHMHandleFromInt(dispatcher(), shm_handle);
}

void PPB_Graphics3D_Proxy::OnMsgGetState(const HostResource& context,
                                         gpu::CommandBuffer::State* state) {
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.failed())
    return;
  PP_Graphics3DTrustedState pp_state = enter.object()->GetState();
  *state = GPUStateFromPPState(pp_state);
}

void PPB_Graphics3D_Proxy::OnMsgFlush(const HostResource& context,
                                      int32 put_offset,
                                      int32 last_known_get,
                                      gpu::CommandBuffer::State* state) {
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.failed())
    return;
  PP_Graphics3DTrustedState pp_state = enter.object()->FlushSyncFast(
      put_offset, last_known_get);
  *state = GPUStateFromPPState(pp_state);
}

void PPB_Graphics3D_Proxy::OnMsgAsyncFlush(const HostResource& context,
                                           int32 put_offset) {
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.succeeded())
    enter.object()->Flush(put_offset);
}

void PPB_Graphics3D_Proxy::OnMsgCreateTransferBuffer(
    const HostResource& context,
    int32 size,
    int32* id) {
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.succeeded())
    *id = enter.object()->CreateTransferBuffer(size);
  else
    *id = 0;
}

void PPB_Graphics3D_Proxy::OnMsgDestroyTransferBuffer(
    const HostResource& context,
    int32 id) {
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.succeeded())
    enter.object()->DestroyTransferBuffer(id);
}

void PPB_Graphics3D_Proxy::OnMsgGetTransferBuffer(
    const HostResource& context,
    int32 id,
    base::SharedMemoryHandle* transfer_buffer,
    uint32* size) {
  *transfer_buffer = base::SharedMemory::NULLHandle();
  *size = 0;

  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  int shm_handle = 0;
  uint32_t shm_size = 0;
  if (enter.succeeded() &&
      enter.object()->GetTransferBuffer(id, &shm_handle, &shm_size)) {
    *transfer_buffer = TransportSHMHandleFromInt(dispatcher(), shm_handle);
    *size = shm_size;
  }
}

void PPB_Graphics3D_Proxy::OnMsgSwapBuffers(const HostResource& context) {
  EnterHostFromHostResourceForceCallback<PPB_Graphics3D_API> enter(
      context, callback_factory_,
      &PPB_Graphics3D_Proxy::SendSwapBuffersACKToPlugin, context);
  if (enter.succeeded())
    enter.SetResult(enter.object()->SwapBuffers(enter.callback()));
}

void PPB_Graphics3D_Proxy::OnMsgSwapBuffersACK(const HostResource& resource,
                                              int32_t pp_error) {
  EnterPluginFromHostResource<PPB_Graphics3D_API> enter(resource);
  if (enter.succeeded())
    static_cast<Graphics3D*>(enter.object())->SwapBuffersACK(pp_error);
}

void PPB_Graphics3D_Proxy::SendSwapBuffersACKToPlugin(
    int32_t result,
    const HostResource& context) {
  dispatcher()->Send(new PpapiMsg_PPBGraphics3D_SwapBuffersACK(
      API_ID_PPB_GRAPHICS_3D, context, result));
}

}  // namespace proxy
}  // namespace ppapi

