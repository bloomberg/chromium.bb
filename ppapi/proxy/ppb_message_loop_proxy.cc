// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppb_message_loop_proxy.h"

#include <vector>

#include "base/bind.h"
#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "ppapi/c/dev/ppb_message_loop_dev.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/proxy/plugin_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/resource.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_message_loop_api.h"

using ppapi::thunk::PPB_MessageLoop_API;

namespace ppapi {
namespace proxy {

namespace {

typedef thunk::EnterResource<PPB_MessageLoop_API> EnterMessageLoop;

class MessageLoopResource : public Resource, public PPB_MessageLoop_API {
 public:
  MessageLoopResource(PP_Instance instance);
  virtual ~MessageLoopResource();

  // Resource overrides.
  virtual PPB_MessageLoop_API* AsPPB_MessageLoop_API() OVERRIDE;

  // PPB_MessageLoop_API implementation.
  virtual int32_t AttachToCurrentThread() OVERRIDE;
  virtual int32_t Run() OVERRIDE;
  virtual int32_t PostWork(PP_CompletionCallback callback,
                           int64_t delay_ms) OVERRIDE;
  virtual int32_t PostQuit(PP_Bool should_destroy) OVERRIDE;

  void DetachFromThread();

 private:
  struct TaskInfo {
    tracked_objects::Location from_here;
    base::Closure closure;
    int64 delay_ms;
  };

  // Returns true if the object is associated with the current thread.
  bool IsCurrent() const;

  // Handles posting to the message loop if there is one, or the pending queue
  // if there isn't.
  // NOTE: The given closure will be run *WITHOUT* acquiring the Proxy lock.
  //       This only makes sense for user code and completely thread-safe
  //       proxy operations (e.g., MessageLoop::QuitClosure).
  void PostClosure(const tracked_objects::Location& from_here,
                   const base::Closure& closure,
                   int64 delay_ms);

  // TLS destructor function.
  static void ReleaseMessageLoop(void* value);

  // Created when we attach to the current thread, since MessageLoop assumes
  // that it's created on the thread it will run on.
  scoped_ptr<MessageLoop> loop_;

  // Number of invocations of Run currently on the stack.
  int nested_invocations_;

  // Set to true when the message loop is destroyed to prevent forther
  // posting of work.
  bool destroyed_;

  // Set to true if all message loop invocations should exit and that the
  // loop should be destroyed once it reaches the outermost Run invocation.
  bool should_destroy_;

  // Since we allow tasks to be posted before the message loop is actually
  // created (when it's associated with a thread), we keep tasks posted here
  // until that happens. Once the loop_ is created, this is unused.
  std::vector<TaskInfo> pending_tasks_;

  DISALLOW_COPY_AND_ASSIGN(MessageLoopResource);
};

MessageLoopResource::MessageLoopResource(PP_Instance instance)
    : Resource(OBJECT_IS_PROXY, instance),
      nested_invocations_(0),
      destroyed_(false),
      should_destroy_(false) {
}

MessageLoopResource::~MessageLoopResource() {
}

PPB_MessageLoop_API* MessageLoopResource::AsPPB_MessageLoop_API() {
  return this;
}

int32_t MessageLoopResource::AttachToCurrentThread() {
  PluginGlobals* globals = PluginGlobals::Get();

  base::ThreadLocalStorage::Slot* slot = globals->msg_loop_slot();
  if (!slot) {
    slot = new base::ThreadLocalStorage::Slot(&ReleaseMessageLoop);
    globals->set_msg_loop_slot(slot);
  } else {
    if (slot->Get())
      return PP_ERROR_INPROGRESS;
  }
  // TODO(brettw) check that the current thread can support a message loop.

  // Take a ref to the MessageLoop on behalf of the TLS. Note that this is an
  // internal ref and not a plugin ref so the plugin can't accidentally
  // release it. This is released by ReleaseMessageLoop().
  AddRef();
  slot->Set(this);

  loop_.reset(new MessageLoop(MessageLoop::TYPE_DEFAULT));

  // Post all pending work to the message loop.
  for (size_t i = 0; i < pending_tasks_.size(); i++) {
    const TaskInfo& info = pending_tasks_[i];
    PostClosure(info.from_here, info.closure, info.delay_ms);
  }
  pending_tasks_.clear();

  return PP_OK;
}

int32_t MessageLoopResource::Run() {
  if (!IsCurrent())
    return PP_ERROR_WRONG_THREAD;
  // TODO(brettw) prevent this from happening on the main thread & return
  // PP_ERROR_BLOCKS_MAIN_THREAD.  Maybe have a special constructor for that
  // one?

  nested_invocations_++;
  CallWhileUnlocked(base::Bind(&MessageLoop::Run,
                               base::Unretained(loop_.get())));
  nested_invocations_--;

  if (should_destroy_ && nested_invocations_ == 0) {
    loop_.reset();
    destroyed_ = true;
  }
  return PP_OK;
}

int32_t MessageLoopResource::PostWork(PP_CompletionCallback callback,
                                      int64_t delay_ms) {
  if (!callback.func)
    return PP_ERROR_BADARGUMENT;
  if (destroyed_)
    return PP_ERROR_FAILED;
  PostClosure(FROM_HERE,
              base::Bind(callback.func, callback.user_data,
                         static_cast<int32_t>(PP_OK)),
              delay_ms);
  return PP_OK;
}

int32_t MessageLoopResource::PostQuit(PP_Bool should_destroy) {
  if (PP_ToBool(should_destroy))
    should_destroy_ = true;

  if (IsCurrent())
    loop_->Quit();
  else
    PostClosure(FROM_HERE, MessageLoop::QuitClosure(), 0);
  return PP_OK;
}

void MessageLoopResource::DetachFromThread() {
  // Note that the message loop must be destroyed on the thread is was created
  // on.
  loop_.reset();

  // Cancel out the AddRef in AttachToCurrentThread().
  Release();
  // DANGER: may delete this.
}

bool MessageLoopResource::IsCurrent() const {
  PluginGlobals* globals = PluginGlobals::Get();
  if (!globals->msg_loop_slot())
    return false;  // Can't be current if there's nothing in the slot.
  return static_cast<const void*>(globals->msg_loop_slot()->Get()) ==
         static_cast<const void*>(this);
}

void MessageLoopResource::PostClosure(
    const tracked_objects::Location& from_here,
    const base::Closure& closure,
    int64 delay_ms) {
  if (loop_.get()) {
    loop_->PostDelayedTask(from_here,
                           closure,
                           base::TimeDelta::FromMilliseconds(delay_ms));
  } else {
    TaskInfo info;
    info.from_here = FROM_HERE;
    info.closure = closure;
    info.delay_ms = delay_ms;
    pending_tasks_.push_back(info);
  }
}

// static
void MessageLoopResource::ReleaseMessageLoop(void* value) {
  static_cast<MessageLoopResource*>(value)->DetachFromThread();
}

// -----------------------------------------------------------------------------

PP_Resource Create(PP_Instance instance) {
  // Validate the instance.
  PluginDispatcher* dispatcher = PluginDispatcher::GetForInstance(instance);
  if (!dispatcher)
    return 0;
  return (new MessageLoopResource(instance))->GetReference();
}

PP_Resource GetForMainThread() {
  // TODO(brettw).
  return 0;
}

PP_Resource GetCurrent() {
  PluginGlobals* globals = PluginGlobals::Get();
  if (!globals->msg_loop_slot())
    return 0;
  MessageLoopResource* loop = reinterpret_cast<MessageLoopResource*>(
      globals->msg_loop_slot()->Get());
  return loop->GetReference();
}

int32_t AttachToCurrentThread(PP_Resource message_loop) {
  EnterMessageLoop enter(message_loop, true);
  if (enter.succeeded())
    return enter.object()->AttachToCurrentThread();
  return PP_ERROR_BADRESOURCE;
}

int32_t Run(PP_Resource message_loop) {
  EnterMessageLoop enter(message_loop, true);
  if (enter.succeeded())
    return enter.object()->Run();
  return PP_ERROR_BADRESOURCE;
}

int32_t PostWork(PP_Resource message_loop,
                 PP_CompletionCallback callback,
                 int64_t delay_ms) {
  EnterMessageLoop enter(message_loop, true);
  if (enter.succeeded())
    return enter.object()->PostWork(callback, delay_ms);
  return PP_ERROR_BADRESOURCE;
}

int32_t PostQuit(PP_Resource message_loop, PP_Bool should_destroy) {
  EnterMessageLoop enter(message_loop, true);
  if (enter.succeeded())
    return enter.object()->PostQuit(should_destroy);
  return PP_ERROR_BADRESOURCE;
}

const PPB_MessageLoop_Dev_0_1 ppb_message_loop_interface = {
  &Create,
  &GetForMainThread,
  &GetCurrent,
  &AttachToCurrentThread,
  &Run,
  &PostWork,
  &PostQuit
};

}  // namespace

PPB_MessageLoop_Proxy::PPB_MessageLoop_Proxy(Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher) {
}

PPB_MessageLoop_Proxy::~PPB_MessageLoop_Proxy() {
}

// static
const PPB_MessageLoop_Dev_0_1* PPB_MessageLoop_Proxy::GetInterface() {
  return &ppb_message_loop_interface;
}

}  // namespace proxy
}  // namespace ppapi
