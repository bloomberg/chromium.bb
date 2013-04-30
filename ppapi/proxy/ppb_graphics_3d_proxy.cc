// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_graphics_3d_proxy.h"

#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/common/command_buffer.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/enter_proxy.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/ppapi_command_buffer_proxy.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/resource_creation_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Graphics3D_API;
using ppapi::thunk::ResourceCreationAPI;

namespace ppapi {
namespace proxy {

namespace {

const int32 kCommandBufferSize = 1024 * 1024;
const int32 kTransferBufferSize = 1024 * 1024;

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

// This class just wraps a CommandBuffer and optionally locks around every
// method. This is used to ensure that we have the Proxy lock any time we enter
// PpapiCommandBufferProxy.
//
// Note, for performance reasons, most of this code is not truly thread
// safe in the sense of multiple threads concurrently rendering to the same
// Graphics3D context; this isn't allowed, and will likely either crash or
// result in undefined behavior.  It is assumed that the thread which creates
// the Graphics3D context will be the thread on which subsequent gl rendering
// will be done.
//
// TODO(nfullagar): At some point, allow multiple threads to concurrently render
// each to its own context.  First step is to allow a single thread (either main
// thread or background thread) to render to a single Graphics3D context.
class Graphics3D::LockingCommandBuffer : public gpu::CommandBuffer {
 public:
  explicit LockingCommandBuffer(gpu::CommandBuffer* gpu_command_buffer)
      : gpu_command_buffer_(gpu_command_buffer), need_to_lock_(true) {
  }
  virtual ~LockingCommandBuffer() {
  }
  void set_need_to_lock(bool need_to_lock) { need_to_lock_ = need_to_lock; }
  bool need_to_lock() const { return need_to_lock_; }

 private:
  // MaybeLock acquires the proxy lock on construction if and only if
  // need_to_lock is true. If it acquired the lock, it releases it on
  // destruction. If need_to_lock is false, then the lock must already be held.
  struct MaybeLock {
    explicit MaybeLock(bool need_to_lock) : locked_(need_to_lock) {
      if (need_to_lock)
        ppapi::ProxyLock::Acquire();
      else
        ppapi::ProxyLock::AssertAcquired();
    }
    ~MaybeLock() {
      if (locked_)
        ppapi::ProxyLock::Release();
    }
   private:
    bool locked_;
  };

  // gpu::CommandBuffer implementation:
  virtual bool Initialize() OVERRIDE {
    MaybeLock lock(need_to_lock_);
    return gpu_command_buffer_->Initialize();
  }
  virtual State GetState() OVERRIDE {
    MaybeLock lock(need_to_lock_);
    return gpu_command_buffer_->GetState();
  }
  virtual State GetLastState() OVERRIDE {
    // During a normal scene, the vast majority of calls are to GetLastState().
    // We don't allow multi-threaded rendering on the same contex, so for
    // performance reasons, avoid the global lock for this entry point.  We can
    // get away with this here because the underlying implementation of
    // GetLastState() is trivial and does not involve global or shared state
    // between other contexts.
    // TODO(nfullagar): We can probably skip MaybeLock for other methods, but
    // the performance gain may not be worth it.
    //
    // MaybeLock lock(need_to_lock_);
    return gpu_command_buffer_->GetLastState();
  }
  virtual int32 GetLastToken() OVERRIDE {
    return GetLastState().token;
  }
  virtual void Flush(int32 put_offset) OVERRIDE {
    MaybeLock lock(need_to_lock_);
    gpu_command_buffer_->Flush(put_offset);
  }
  virtual State FlushSync(int32 put_offset, int32 last_known_get) OVERRIDE {
    MaybeLock lock(need_to_lock_);
    return gpu_command_buffer_->FlushSync(put_offset, last_known_get);
  }
  virtual void SetGetBuffer(int32 transfer_buffer_id) OVERRIDE {
    MaybeLock lock(need_to_lock_);
    gpu_command_buffer_->SetGetBuffer(transfer_buffer_id);
  }
  virtual void SetGetOffset(int32 get_offset) OVERRIDE {
    MaybeLock lock(need_to_lock_);
    gpu_command_buffer_->SetGetOffset(get_offset);
  }
  virtual gpu::Buffer CreateTransferBuffer(size_t size,
                                           int32* id) OVERRIDE {
    MaybeLock lock(need_to_lock_);
    return gpu_command_buffer_->CreateTransferBuffer(size, id);
  }
  virtual void DestroyTransferBuffer(int32 id) OVERRIDE {
    MaybeLock lock(need_to_lock_);
    gpu_command_buffer_->DestroyTransferBuffer(id);
  }
  virtual gpu::Buffer GetTransferBuffer(int32 id) OVERRIDE {
    MaybeLock lock(need_to_lock_);
    return gpu_command_buffer_->GetTransferBuffer(id);
  }
  virtual void SetToken(int32 token) OVERRIDE {
    MaybeLock lock(need_to_lock_);
    gpu_command_buffer_->SetToken(token);
  }
  virtual void SetParseError(gpu::error::Error error) OVERRIDE {
    MaybeLock lock(need_to_lock_);
    gpu_command_buffer_->SetParseError(error);
  }
  virtual void SetContextLostReason(
      gpu::error::ContextLostReason reason) OVERRIDE {
    MaybeLock lock(need_to_lock_);
    gpu_command_buffer_->SetContextLostReason(reason);
  }
  virtual uint32 InsertSyncPoint() OVERRIDE {
    MaybeLock lock(need_to_lock_);
    return gpu_command_buffer_->InsertSyncPoint();
  }

  // Weak pointer - see class Graphics3D for the scopted_ptr.
  gpu::CommandBuffer* gpu_command_buffer_;

  bool need_to_lock_;
};

Graphics3D::Graphics3D(const HostResource& resource)
    : PPB_Graphics3D_Shared(resource),
      num_already_locked_calls_(0) {
}

Graphics3D::~Graphics3D() {
  DestroyGLES2Impl();
}

bool Graphics3D::Init(gpu::gles2::GLES2Implementation* share_gles2) {
  PluginDispatcher* dispatcher = PluginDispatcher::GetForResource(this);
  if (!dispatcher)
    return false;

  command_buffer_.reset(
      new PpapiCommandBufferProxy(host_resource(), dispatcher));
  locking_command_buffer_.reset(
      new LockingCommandBuffer(command_buffer_.get()));

  ScopedNoLocking already_locked(this);
  if (!command_buffer_->Initialize())
    return false;

  return CreateGLES2Impl(kCommandBufferSize, kTransferBufferSize,
                         share_gles2);
}

PP_Bool Graphics3D::InitCommandBuffer() {
  return PP_FALSE;
}

PP_Bool Graphics3D::SetGetBuffer(int32_t /* transfer_buffer_id */) {
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

uint32_t Graphics3D::InsertSyncPoint() {
  NOTREACHED();
  return 0;
}

gpu::CommandBuffer* Graphics3D::GetCommandBuffer() {
  return locking_command_buffer_.get();
}

int32 Graphics3D::DoSwapBuffers() {
  // gles2_impl()->SwapBuffers() results in CommandBuffer calls, and we already
  // have the proxy lock.
  ScopedNoLocking already_locked(this);

  gles2_impl()->SwapBuffers();
  IPC::Message* msg = new PpapiHostMsg_PPBGraphics3D_SwapBuffers(
      API_ID_PPB_GRAPHICS_3D, host_resource());
  msg->set_unblock(true);
  PluginDispatcher::GetForResource(this)->Send(msg);

  return PP_OK_COMPLETIONPENDING;
}

void Graphics3D::PushAlreadyLocked() {
  ppapi::ProxyLock::AssertAcquired();
  if (num_already_locked_calls_ == 0)
    locking_command_buffer_->set_need_to_lock(false);
  ++num_already_locked_calls_;
}

void Graphics3D::PopAlreadyLocked() {
  // We must have Pushed before we can Pop.
  DCHECK(!locking_command_buffer_->need_to_lock());
  DCHECK_GT(num_already_locked_calls_, 0);
  ppapi::ProxyLock::AssertAcquired();
  --num_already_locked_calls_;
  if (num_already_locked_calls_ == 0)
    locking_command_buffer_->set_need_to_lock(true);
}

PPB_Graphics3D_Proxy::PPB_Graphics3D_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      callback_factory_(this) {
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

  HostResource share_host;
  gpu::gles2::GLES2Implementation* share_gles2 = NULL;
  if (share_context != 0) {
    EnterResourceNoLock<PPB_Graphics3D_API> enter(share_context, true);
    if (enter.failed())
      return PP_ERROR_BADARGUMENT;

    PPB_Graphics3D_Shared* share_graphics =
        static_cast<PPB_Graphics3D_Shared*>(enter.object());
    share_host = share_graphics->host_resource();
    share_gles2 = share_graphics->gles2_impl();
  }

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
      API_ID_PPB_GRAPHICS_3D, instance, share_host, attribs, &result));
  if (result.is_null())
    return 0;

  scoped_refptr<Graphics3D> graphics_3d(new Graphics3D(result));
  if (!graphics_3d->Init(share_gles2))
    return 0;
  return graphics_3d->GetReference();
}

bool PPB_Graphics3D_Proxy::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPB_Graphics3D_Proxy, msg)
#if !defined(OS_NACL)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_Create,
                        OnMsgCreate)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_InitCommandBuffer,
                        OnMsgInitCommandBuffer)
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_SetGetBuffer,
                        OnMsgSetGetBuffer)
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
    IPC_MESSAGE_HANDLER(PpapiHostMsg_PPBGraphics3D_InsertSyncPoint,
                        OnMsgInsertSyncPoint)
#endif  // !defined(OS_NACL)

    IPC_MESSAGE_HANDLER(PpapiMsg_PPBGraphics3D_SwapBuffersACK,
                        OnMsgSwapBuffersACK)
    IPC_MESSAGE_UNHANDLED(handled = false)

  IPC_END_MESSAGE_MAP()
  // FIXME(brettw) handle bad messages!
  return handled;
}

#if !defined(OS_NACL)
void PPB_Graphics3D_Proxy::OnMsgCreate(PP_Instance instance,
                                       HostResource share_context,
                                       const std::vector<int32_t>& attribs,
                                       HostResource* result) {
  if (attribs.empty() ||
      attribs.back() != PP_GRAPHICS3DATTRIB_NONE ||
      !(attribs.size() & 1))
    return;  // Bad message.

  thunk::EnterResourceCreation enter(instance);

  if (enter.succeeded()) {
    result->SetHostResource(
      instance,
      enter.functions()->CreateGraphics3DRaw(instance,
                                             share_context.host_resource(),
                                             &attribs.front()));
  }
}

void PPB_Graphics3D_Proxy::OnMsgInitCommandBuffer(
    const HostResource& context) {
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.failed())
    return;

  if (!enter.object()->InitCommandBuffer())
    return;
}

void PPB_Graphics3D_Proxy::OnMsgSetGetBuffer(
    const HostResource& context,
    int32 transfer_buffer_id) {
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.succeeded())
    enter.object()->SetGetBuffer(transfer_buffer_id);
}

void PPB_Graphics3D_Proxy::OnMsgGetState(const HostResource& context,
                                         gpu::CommandBuffer::State* state,
                                         bool* success) {
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.failed()) {
    *success = false;
    return;
  }
  PP_Graphics3DTrustedState pp_state = enter.object()->GetState();
  *state = GPUStateFromPPState(pp_state);
  *success = true;
}

void PPB_Graphics3D_Proxy::OnMsgFlush(const HostResource& context,
                                      int32 put_offset,
                                      int32 last_known_get,
                                      gpu::CommandBuffer::State* state,
                                      bool* success) {
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.failed()) {
    *success = false;
    return;
  }
  PP_Graphics3DTrustedState pp_state = enter.object()->FlushSyncFast(
      put_offset, last_known_get);
  *state = GPUStateFromPPState(pp_state);
  *success = true;
}

void PPB_Graphics3D_Proxy::OnMsgAsyncFlush(const HostResource& context,
                                           int32 put_offset) {
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.succeeded())
    enter.object()->Flush(put_offset);
}

void PPB_Graphics3D_Proxy::OnMsgCreateTransferBuffer(
    const HostResource& context,
    uint32 size,
    int32* id) {
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.succeeded())
    *id = enter.object()->CreateTransferBuffer(size);
  else
    *id = -1;
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
    ppapi::proxy::SerializedHandle* transfer_buffer) {
  transfer_buffer->set_null_shmem();

  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  int shm_handle = 0;
  uint32_t shm_size = 0;
  if (enter.succeeded() &&
      enter.object()->GetTransferBuffer(id, &shm_handle, &shm_size)) {
    transfer_buffer->set_shmem(
        TransportSHMHandleFromInt(dispatcher(), shm_handle),
        shm_size);
  }
}

void PPB_Graphics3D_Proxy::OnMsgSwapBuffers(const HostResource& context) {
  EnterHostFromHostResourceForceCallback<PPB_Graphics3D_API> enter(
      context, callback_factory_,
      &PPB_Graphics3D_Proxy::SendSwapBuffersACKToPlugin, context);
  if (enter.succeeded())
    enter.SetResult(enter.object()->SwapBuffers(enter.callback()));
}

void PPB_Graphics3D_Proxy::OnMsgInsertSyncPoint(const HostResource& context,
                                                uint32* sync_point) {
  *sync_point = 0;
  EnterHostFromHostResource<PPB_Graphics3D_API> enter(context);
  if (enter.succeeded())
    *sync_point = enter.object()->InsertSyncPoint();
}
#endif  // !defined(OS_NACL)

void PPB_Graphics3D_Proxy::OnMsgSwapBuffersACK(const HostResource& resource,
                                              int32_t pp_error) {
  EnterPluginFromHostResource<PPB_Graphics3D_API> enter(resource);
  if (enter.succeeded())
    static_cast<Graphics3D*>(enter.object())->SwapBuffersACK(pp_error);
}

#if !defined(OS_NACL)
void PPB_Graphics3D_Proxy::SendSwapBuffersACKToPlugin(
    int32_t result,
    const HostResource& context) {
  dispatcher()->Send(new PpapiMsg_PPBGraphics3D_SwapBuffersACK(
      API_ID_PPB_GRAPHICS_3D, context, result));
}
#endif  // !defined(OS_NACL)

}  // namespace proxy
}  // namespace ppapi

