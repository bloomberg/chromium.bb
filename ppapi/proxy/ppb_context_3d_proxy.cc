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
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_resource.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_surface_3d_proxy.h"

namespace pp {
namespace proxy {

namespace {

PP_Resource Create(PP_Instance instance,
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
  linked_ptr<Context3D> context_3d(new Context3D(result));
  if (!context_3d->CreateImplementation())
    return 0;
  return PluginResourceTracker::GetInstance()->AddResource(context_3d);
}

PP_Bool IsContext3D(PP_Resource resource) {
  Context3D* object = PluginResource::GetAs<Context3D>(resource);
  return BoolToPPBool(!!object);
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
  Context3D* object = PluginResource::GetAs<Context3D>(context_id);
  if (!object)
    return PP_ERROR_BADRESOURCE;
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(
      object->instance());
  if (!dispatcher)
    return PP_ERROR_FAILED;

  // TODO(alokp): Support separate draw-read surfaces.
  DCHECK_EQ(draw, read);
  if (draw != read)
    return PP_GRAPHICS3DERROR_BAD_MATCH;

  Surface3D* draw_surface = PluginResource::GetAs<Surface3D>(draw);
  Surface3D* read_surface = PluginResource::GetAs<Surface3D>(read);
  if (draw && !draw_surface)
    return PP_ERROR_BADRESOURCE;
  if (read && !read_surface)
    return PP_ERROR_BADRESOURCE;
  HostResource host_draw =
      draw_surface ? draw_surface->host_resource() : HostResource();
  HostResource host_read =
      read_surface ? read_surface->host_resource() : HostResource();

  int32_t result;
  dispatcher->Send(new PpapiHostMsg_PPBContext3D_BindSurfaces(
      INTERFACE_ID_PPB_CONTEXT_3D,
      object->host_resource(),
      host_draw,
      host_read,
      &result));

  if (result == PP_OK)
    object->BindSurfaces(draw_surface, read_surface);

  return result;
}

int32_t GetBoundSurfaces(PP_Resource context,
                         PP_Resource* draw,
                         PP_Resource* read) {
  Context3D* object = PluginResource::GetAs<Context3D>(context);
  if (!object)
    return PP_ERROR_BADRESOURCE;

  Surface3D* draw_surface = object->get_draw_surface();
  Surface3D* read_surface = object->get_read_surface();

  *draw = draw_surface ? draw_surface->resource() : 0;
  *read = read_surface ? read_surface->resource() : 0;
  return PP_OK;
}


const PPB_Context3D_Dev context_3d_interface = {
  &Create,
  &IsContext3D,
  &GetAttrib,
  &BindSurfaces,
  &GetBoundSurfaces,
};

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

gpu::CommandBuffer::State GPUStateFromPPState(
    const PP_Context3DTrustedState& s) {
  gpu::CommandBuffer::State state;
  state.num_entries = s.num_entries;
  state.get_offset = s.get_offset;
  state.put_offset = s.put_offset;
  state.token = s.token;
  state.error = static_cast<gpu::error::Error>(s.error);
  return state;
}

// Size of the transfer buffer.
const int32 kCommandBufferSize = 1024 * 1024;
const int32 kTransferBufferSize = 1024 * 1024;

InterfaceProxy* CreateContext3DProxy(Dispatcher* dispatcher,
                                     const void* target_interface) {
  return new PPB_Context3D_Proxy(dispatcher, target_interface);
}

}  // namespace

class PepperCommandBuffer : public gpu::CommandBuffer {
 public:
  PepperCommandBuffer(const HostResource& resource,
                      PluginDispatcher* dispatcher);
  virtual ~PepperCommandBuffer();

  // CommandBuffer implementation:
  virtual bool Initialize(int32 size);
  virtual gpu::Buffer GetRingBuffer();
  virtual State GetState();
  virtual void Flush(int32 put_offset);
  virtual State FlushSync(int32 put_offset);
  virtual void SetGetOffset(int32 get_offset);
  virtual int32 CreateTransferBuffer(size_t size);
  virtual int32 RegisterTransferBuffer(base::SharedMemory* shared_memory,
                                       size_t size);
  virtual void DestroyTransferBuffer(int32 id);
  virtual gpu::Buffer GetTransferBuffer(int32 handle);
  virtual void SetToken(int32 token);
  virtual void SetParseError(gpu::error::Error error);

 private:
  bool Send(IPC::Message* msg);

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
    Send(new PpapiHostMsg_PPBContext3D_GetState(
        INTERFACE_ID_PPB_CONTEXT_3D, resource_, &last_state_));
  }

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

gpu::CommandBuffer::State PepperCommandBuffer::FlushSync(int32 put_offset) {
  // Send will flag state with lost context if IPC fails.
  if (last_state_.error == gpu::error::kNoError) {
    Send(new PpapiHostMsg_PPBContext3D_Flush(
        INTERFACE_ID_PPB_CONTEXT_3D, resource_, put_offset, &last_state_));
  }

  return last_state_;
}

void PepperCommandBuffer::SetGetOffset(int32 get_offset) {
  // Not implemented in proxy.
  NOTREACHED();
}

int32 PepperCommandBuffer::CreateTransferBuffer(size_t size) {
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
    size_t size) {
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

bool PepperCommandBuffer::Send(IPC::Message* msg) {
  DCHECK(last_state_.error == gpu::error::kNoError);

  if (dispatcher_->Send(msg))
    return true;

  last_state_.error = gpu::error::kLostContext;
  return false;
}

Context3D::Context3D(const HostResource& resource)
    : PluginResource(resource),
      draw_(NULL),
      read_(NULL),
      transfer_buffer_id_(0) {
}

Context3D::~Context3D() {
  if (draw_)
    draw_->set_context(NULL);
}

bool Context3D::CreateImplementation() {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance());
  if (!dispatcher)
    return false;

  command_buffer_.reset(new PepperCommandBuffer(host_resource(), dispatcher));

  if (!command_buffer_->Initialize(kCommandBufferSize))
    return false;

  helper_.reset(new gpu::gles2::GLES2CmdHelper(command_buffer_.get()));
  if (!helper_->Initialize(kCommandBufferSize))
    return false;

  transfer_buffer_id_ =
      command_buffer_->CreateTransferBuffer(kTransferBufferSize);
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
      false));

  return true;
}

void Context3D::BindSurfaces(Surface3D* draw, Surface3D* read) {
  if (draw != draw_) {
    if (draw_)
      draw_->set_context(NULL);
    if (draw) {
      draw->set_context(this);
      // Resize the backing texture to the size of the instance when it is
      // bound.
      // TODO(alokp): This should be the responsibility of plugins.
      PluginDispatcher* dispatcher =
          PluginDispatcher::GetForInstance(instance());
      DCHECK(dispatcher);
      InstanceData* data = dispatcher->GetInstanceData(instance());
      DCHECK(data);
      gles2_impl()->ResizeCHROMIUM(data->position.size.width,
                                   data->position.size.height);
    }
    draw_ = draw;
  }
  read_ = read;
}

PPB_Context3D_Proxy::PPB_Context3D_Proxy(Dispatcher* dispatcher,
                                         const void* target_interface)
    : InterfaceProxy(dispatcher, target_interface) {
}

PPB_Context3D_Proxy::~PPB_Context3D_Proxy() {
}

// static
const InterfaceProxy::Info* PPB_Context3D_Proxy::GetInfo() {
  static const Info info = {
    &context_3d_interface,
    PPB_CONTEXT_3D_DEV_INTERFACE,
    INTERFACE_ID_PPB_CONTEXT_3D,
    false,
    &CreateContext3DProxy,
  };
  return &info;
}

const PPB_Context3DTrusted_Dev*
PPB_Context3D_Proxy::ppb_context_3d_trusted() const {
  return static_cast<const PPB_Context3DTrusted_Dev*>(
      dispatcher()->GetLocalInterface(
          PPB_CONTEXT_3D_TRUSTED_DEV_INTERFACE));
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
                                      std::vector<int32_t> attribs,
                                      HostResource* result) {
  DCHECK(attribs.back() == 0);
  PP_Resource resource = ppb_context_3d_trusted()->CreateRaw(
      instance, config, 0, &attribs.front());
  result->SetHostResource(instance, resource);
}

void PPB_Context3D_Proxy::OnMsgBindSurfaces(const HostResource& context,
                                            const HostResource& draw,
                                            const HostResource& read,
                                            int32_t* result) {
  *result = ppb_context_3d_target()->BindSurfaces(context.host_resource(),
                                                  draw.host_resource(),
                                                  read.host_resource());
}

void PPB_Context3D_Proxy::OnMsgInitialize(
    const HostResource& context,
    int32 size,
    base::SharedMemoryHandle* ring_buffer) {
  const PPB_Context3DTrusted_Dev* context_3d_trusted = ppb_context_3d_trusted();
  *ring_buffer = base::SharedMemory::NULLHandle();
  if (!context_3d_trusted->Initialize(context.host_resource(), size))
    return;

  int shm_handle;
  uint32_t shm_size;
  if (!context_3d_trusted->GetRingBuffer(context.host_resource(),
                                         &shm_handle,
                                         &shm_size)) {
    return;
  }

  *ring_buffer = TransportSHMHandleFromInt(dispatcher(), shm_handle);
}

void PPB_Context3D_Proxy::OnMsgGetState(const HostResource& context,
                                        gpu::CommandBuffer::State* state) {
  PP_Context3DTrustedState pp_state =
      ppb_context_3d_trusted()->GetState(context.host_resource());
  *state = GPUStateFromPPState(pp_state);
}

void PPB_Context3D_Proxy::OnMsgFlush(const HostResource& context,
                                     int32 put_offset,
                                     gpu::CommandBuffer::State* state) {
  PP_Context3DTrustedState pp_state =
      ppb_context_3d_trusted()->FlushSync(context.host_resource(), put_offset);
  *state = GPUStateFromPPState(pp_state);
}

void PPB_Context3D_Proxy::OnMsgAsyncFlush(const HostResource& context,
                                          int32 put_offset) {
  ppb_context_3d_trusted()->Flush(context.host_resource(), put_offset);
}

void PPB_Context3D_Proxy::OnMsgCreateTransferBuffer(
    const HostResource& context,
    int32 size,
    int32* id) {
  *id = ppb_context_3d_trusted()->CreateTransferBuffer(
      context.host_resource(), size);
}

void PPB_Context3D_Proxy::OnMsgDestroyTransferBuffer(
    const HostResource& context,
    int32 id) {
  ppb_context_3d_trusted()->DestroyTransferBuffer(context.host_resource(), id);
}

void PPB_Context3D_Proxy::OnMsgGetTransferBuffer(
    const HostResource& context,
    int32 id,
    base::SharedMemoryHandle* transfer_buffer,
    uint32* size) {
  *transfer_buffer = base::SharedMemory::NULLHandle();
  int shm_handle;
  uint32_t shm_size;
  if (!ppb_context_3d_trusted()->GetTransferBuffer(context.host_resource(),
                                                   id,
                                                   &shm_handle,
                                                   &shm_size)) {
    return;
  }
  *transfer_buffer = TransportSHMHandleFromInt(dispatcher(), shm_handle);
  *size = shm_size;
}

}  // namespace proxy
}  // namespace pp
