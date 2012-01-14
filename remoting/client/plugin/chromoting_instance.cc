// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/chromoting_instance.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "media/base/media.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/rect.h"
// TODO(wez): Remove this when crbug.com/86353 is complete.
#include "ppapi/cpp/private/var_private.h"
#include "remoting/base/util.h"
#include "remoting/client/client_config.h"
#include "remoting/client/chromoting_client.h"
#include "remoting/client/frame_consumer_proxy.h"
#include "remoting/client/mouse_input_filter.h"
#include "remoting/client/plugin/chromoting_scriptable_object.h"
#include "remoting/client/plugin/pepper_input_handler.h"
#include "remoting/client/plugin/pepper_view.h"
#include "remoting/client/plugin/pepper_xmpp_proxy.h"
#include "remoting/client/rectangle_update_decoder.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/key_event_tracker.h"

namespace remoting {

// This flag blocks LOGs to the UI if we're already in the middle of logging
// to the UI. This prevents a potential infinite loop if we encounter an error
// while sending the log message to the UI.
static bool g_logging_to_plugin = false;
static bool g_has_logging_instance = false;
static ChromotingInstance* g_logging_instance = NULL;
static logging::LogMessageHandlerFunction g_logging_old_handler = NULL;

const char ChromotingInstance::kMimeType[] = "pepper-application/x-chromoting";

static base::LazyInstance<base::Lock,
                          base::LeakyLazyInstanceTraits<base::Lock> >
    g_logging_lock = LAZY_INSTANCE_INITIALIZER;

ChromotingInstance::ChromotingInstance(PP_Instance pp_instance)
    : pp::InstancePrivate(pp_instance),
      initialized_(false),
      plugin_message_loop_(
          new PluginMessageLoopProxy(&plugin_thread_delegate_)),
      context_(plugin_message_loop_),
      scale_to_fit_(false),
      thread_proxy_(new ScopedThreadProxy(plugin_message_loop_)) {
  RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE | PP_INPUTEVENT_CLASS_WHEEL);
  RequestFilteringInputEvents(PP_INPUTEVENT_CLASS_KEYBOARD);

  // Resister this instance to handle debug log messsages.
  RegisterLoggingInstance();
}

ChromotingInstance::~ChromotingInstance() {
  DCHECK(plugin_message_loop_->BelongsToCurrentThread());

  // Detach the log proxy so we don't log anything else to the UI.
  // This needs to be done before the instance is unregistered for logging.
  thread_proxy_->Detach();

  // Unregister this instance so that debug log messages will no longer be sent
  // to it. This will stop all logging in all Chromoting instances.
  UnregisterLoggingInstance();

  if (client_.get()) {
    base::WaitableEvent done_event(true, false);
    client_->Stop(base::Bind(&base::WaitableEvent::Signal,
                             base::Unretained(&done_event)));
    done_event.Wait();
  }

  // Stopping the context shuts down all chromoting threads.
  context_.Stop();

  // Detach the |consumer_proxy_|, so that any queued tasks don't touch |view_|
  // after we've deleted it.
  if (consumer_proxy_.get()) {
    consumer_proxy_->Detach();
  }

  // Delete |thread_proxy_| before we detach |plugin_message_loop_|,
  // otherwise ScopedThreadProxy may DCHECK when being destroyed.
  thread_proxy_.reset();

  plugin_message_loop_->Detach();
}

bool ChromotingInstance::Init(uint32_t argc,
                              const char* argn[],
                              const char* argv[]) {
  CHECK(!initialized_);
  initialized_ = true;

  VLOG(1) << "Started ChromotingInstance::Init";

  // Check to make sure the media library is initialized.
  // http://crbug.com/91521.
  if (!media::IsMediaLibraryInitialized()) {
    LOG(ERROR) << "Media library not initialized.";
    return false;
  }

  // Start all the threads.
  context_.Start();

  // Create the chromoting objects that don't depend on the network connection.
  // Because we decode on a separate thread we need a FrameConsumerProxy to
  // bounce calls from the RectangleUpdateDecoder back to the plugin thread.
  view_.reset(new PepperView(this, &context_));
  consumer_proxy_ = new FrameConsumerProxy(view_.get(), plugin_message_loop_);
  rectangle_decoder_ = new RectangleUpdateDecoder(
      context_.decode_message_loop(), consumer_proxy_.get());

  // Default to a medium grey.
  view_->SetSolidFill(0xFFCDCDCD);

  return true;
}

void ChromotingInstance::Connect(const ClientConfig& config) {
  DCHECK(plugin_message_loop_->BelongsToCurrentThread());

  host_connection_.reset(new protocol::ConnectionToHost(
      context_.network_message_loop(), this, true));
  client_.reset(new ChromotingClient(config, &context_, host_connection_.get(),
                                     view_.get(), rectangle_decoder_.get(),
                                     base::Closure()));

  LOG(INFO) << "Connecting to " << config.host_jid
            << ". Local jid: " << config.local_jid << ".";

  // Setup the XMPP Proxy.
  ChromotingScriptableObject* scriptable_object = GetScriptableObject();
  scoped_refptr<PepperXmppProxy> xmpp_proxy =
      new PepperXmppProxy(scriptable_object->AsWeakPtr(),
                          plugin_message_loop_,
                          context_.network_message_loop());
  scriptable_object->AttachXmppProxy(xmpp_proxy);

  // Kick off the connection.
  client_->Start(xmpp_proxy);

  VLOG(1) << "Connection status: Initializing";
  GetScriptableObject()->SetConnectionStatus(
      ChromotingScriptableObject::STATUS_INITIALIZING,
      ChromotingScriptableObject::ERROR_NONE);
}

void ChromotingInstance::Disconnect() {
  DCHECK(plugin_message_loop_->BelongsToCurrentThread());

  LOG(INFO) << "Disconnecting from host.";
  if (client_.get()) {
    // TODO(sergeyu): Should we disconnect asynchronously?
    base::WaitableEvent done_event(true, false);
    client_->Stop(base::Bind(&base::WaitableEvent::Signal,
                             base::Unretained(&done_event)));
    done_event.Wait();
    client_.reset();
  }

  input_handler_.reset();
  key_event_tracker_.reset();
  mouse_input_filter_.reset();
  host_connection_.reset();

  GetScriptableObject()->SetConnectionStatus(
      ChromotingScriptableObject::STATUS_CLOSED,
      ChromotingScriptableObject::ERROR_NONE);
}

void ChromotingInstance::DidChangeView(const pp::Rect& position,
                                       const pp::Rect& clip) {
  DCHECK(plugin_message_loop_->BelongsToCurrentThread());

  SkISize new_size = SkISize::Make(position.width(), position.height());
  if (view_->SetViewSize(new_size)) {
    if (mouse_input_filter_.get()) {
      mouse_input_filter_->set_input_size(new_size);
    }
    rectangle_decoder_->SetOutputSize(new_size);
  }

  rectangle_decoder_->UpdateClipRect(
      SkIRect::MakeXYWH(clip.x(), clip.y(), clip.width(), clip.height()));
}

bool ChromotingInstance::HandleInputEvent(const pp::InputEvent& event) {
  DCHECK(plugin_message_loop_->BelongsToCurrentThread());

  // Never inject events if the end of the input pipeline doesn't exist.
  // If it does exist but the pipeline doesn't, construct a pipeline.
  // TODO(wez): This is really ugly.  We should create the pipeline when
  // the ConnectionToHost's InputStub exists.
  if (!host_connection_.get()) {
    return false;
  } else if (!input_handler_.get()) {
    protocol::InputStub* input_stub = host_connection_->input_stub();
    if (!input_stub)
      return false;
    mouse_input_filter_.reset(new MouseInputFilter(input_stub));
    mouse_input_filter_->set_input_size(view_->get_view_size());
    key_event_tracker_.reset(
        new protocol::KeyEventTracker(mouse_input_filter_.get()));
    input_handler_.reset(
        new PepperInputHandler(key_event_tracker_.get()));
  }

  // TODO(wez): When we have a good hook into Host dimensions changes, move
  // this there.
  mouse_input_filter_->set_output_size(view_->get_host_size());

  return input_handler_->HandleInputEvent(event);
}

ChromotingScriptableObject* ChromotingInstance::GetScriptableObject() {
  pp::VarPrivate object = GetInstanceObject();
  if (!object.is_undefined()) {
    pp::deprecated::ScriptableObject* so = object.AsScriptableObject();
    DCHECK(so != NULL);
    return static_cast<ChromotingScriptableObject*>(so);
  }
  LOG(ERROR) << "Unable to get ScriptableObject for Chromoting plugin.";
  return NULL;
}

// static
void ChromotingInstance::RegisterLogMessageHandler() {
  base::AutoLock lock(g_logging_lock.Get());

  VLOG(1) << "Registering global log handler";

  // Record previous handler so we can call it in a chain.
  g_logging_old_handler = logging::GetLogMessageHandler();

  // Set up log message handler.
  // This is not thread-safe so we need it within our lock.
  logging::SetLogMessageHandler(&LogToUI);
}

void ChromotingInstance::RegisterLoggingInstance() {
  base::AutoLock lock(g_logging_lock.Get());

  // Register this instance as the one that will handle all logging calls
  // and display them to the user.
  // If multiple plugins are run, then the last one registered will handle all
  // logging for all instances.
  g_logging_instance = this;
  g_has_logging_instance = true;
}

void ChromotingInstance::UnregisterLoggingInstance() {
  base::AutoLock lock(g_logging_lock.Get());

  // Don't unregister unless we're the currently registered instance.
  if (this != g_logging_instance)
    return;

  // Unregister this instance for logging.
  g_has_logging_instance = false;
  g_logging_instance = NULL;

  VLOG(1) << "Unregistering global log handler";
}

// static
bool ChromotingInstance::LogToUI(int severity, const char* file, int line,
                                 size_t message_start,
                                 const std::string& str) {
  // Note that we're reading |g_has_logging_instance| outside of a lock.
  // This lockless read is done so that we don't needlessly slow down global
  // logging with a lock for each log message.
  //
  // This lockless read is safe because:
  //
  // Misreading a false value (when it should be true) means that we'll simply
  // skip processing a few log messages.
  //
  // Misreading a true value (when it should be false) means that we'll take
  // the lock and check |g_logging_instance| unnecessarily. This is not
  // problematic because we always set |g_logging_instance| inside a lock.
  if (g_has_logging_instance) {
    // Do not LOG anything while holding this lock or else the code will
    // deadlock while trying to re-get the lock we're already in.
    base::AutoLock lock(g_logging_lock.Get());
    if (g_logging_instance &&
        // If |g_logging_to_plugin| is set and we're on the logging thread, then
        // this LOG message came from handling a previous LOG message and we
        // should skip it to avoid an infinite loop of LOG messages.
        // We don't have a lock around |g_in_processtoui|, but that's OK since
        // the value is only read/written on the logging thread.
        (!g_logging_instance->plugin_message_loop_->BelongsToCurrentThread() ||
         !g_logging_to_plugin)) {
      std::string message = remoting::GetTimestampString();
      message += (str.c_str() + message_start);
      // |thread_proxy_| is safe to use here because we detach it before
      // tearing down the |g_logging_instance|.
      g_logging_instance->thread_proxy_->PostTask(
          FROM_HERE, base::Bind(&ChromotingInstance::ProcessLogToUI,
                                base::Unretained(g_logging_instance), message));
    }
  }

  if (g_logging_old_handler)
    return (g_logging_old_handler)(severity, file, line, message_start, str);
  return false;
}

void ChromotingInstance::ProcessLogToUI(const std::string& message) {
  DCHECK(plugin_message_loop_->BelongsToCurrentThread());

  // This flag (which is set only here) is used to prevent LogToUI from posting
  // new tasks while we're in the middle of servicing a LOG call. This can
  // happen if the call to LogDebugInfo tries to LOG anything.
  g_logging_to_plugin = true;
  ChromotingScriptableObject* cso = GetScriptableObject();
  if (cso)
    cso->LogDebugInfo(message);
  g_logging_to_plugin = false;
}

pp::Var ChromotingInstance::GetInstanceObject() {
  if (instance_object_.is_undefined()) {
    ChromotingScriptableObject* object =
        new ChromotingScriptableObject(this, plugin_message_loop_);
    object->Init();

    // The pp::Var takes ownership of object here.
    instance_object_ = pp::VarPrivate(this, object);
  }

  return instance_object_;
}

ChromotingStats* ChromotingInstance::GetStats() {
  if (!client_.get())
    return NULL;
  return client_->GetStats();
}

void ChromotingInstance::ReleaseAllKeys() {
  if (key_event_tracker_.get()) {
    key_event_tracker_->ReleaseAllKeys();
  }
}

}  // namespace remoting
