// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/chromoting_instance.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/string_split.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "media/base/media.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/rect.h"
// TODO(wez): Remove this when crbug.com/86353 is complete.
#include "ppapi/cpp/private/var_private.h"
#include "remoting/base/constants.h"
#include "remoting/base/util.h"
#include "remoting/client/client_config.h"
#include "remoting/client/chromoting_client.h"
#include "remoting/client/frame_consumer_proxy.h"
#include "remoting/client/plugin/chromoting_scriptable_object.h"
#include "remoting/client/plugin/pepper_input_handler.h"
#include "remoting/client/plugin/pepper_view.h"
#include "remoting/client/plugin/pepper_xmpp_proxy.h"
#include "remoting/client/rectangle_update_decoder.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/input_event_tracker.h"
#include "remoting/protocol/mouse_input_filter.h"

// Windows defines 'PostMessage', so we have to undef it.
#if defined(PostMessage)
#undef PostMessage
#endif

namespace remoting {

namespace {

const int kPerfStatsIntervalMs = 1000;

std::string ConnectionStateToString(ChromotingInstance::ConnectionState state) {
  switch (state) {
    case ChromotingInstance::STATE_CONNECTING:
      return "CONNECTING";
    case ChromotingInstance::STATE_INITIALIZING:
      return "INITIALIZING";
    case ChromotingInstance::STATE_CONNECTED:
      return "CONNECTED";
    case ChromotingInstance::STATE_CLOSED:
      return "CLOSED";
    case ChromotingInstance::STATE_FAILED:
      return "FAILED";
  }
  NOTREACHED();
  return "";
}

std::string ConnectionErrorToString(ChromotingInstance::ConnectionError error) {
  switch (error) {
    case ChromotingInstance::ERROR_NONE:
      return "NONE";
    case ChromotingInstance::ERROR_HOST_IS_OFFLINE:
      return "HOST_IS_OFFLINE";
    case ChromotingInstance::ERROR_SESSION_REJECTED:
      return "SESSION_REJECTED";
    case ChromotingInstance::ERROR_INCOMPATIBLE_PROTOCOL:
      return "INCOMPATIBLE_PROTOCOL";
    case ChromotingInstance::ERROR_NETWORK_FAILURE:
      return "NETWORK_FAILURE";
    case ChromotingInstance::ERROR_HOST_OVERLOAD:
      return "HOST_OVERLOAD";
  }
  NOTREACHED();
  return "";
}

}  // namespace

// This flag blocks LOGs to the UI if we're already in the middle of logging
// to the UI. This prevents a potential infinite loop if we encounter an error
// while sending the log message to the UI.
static bool g_logging_to_plugin = false;
static bool g_has_logging_instance = false;
static ChromotingInstance* g_logging_instance = NULL;
static logging::LogMessageHandlerFunction g_logging_old_handler = NULL;

static base::LazyInstance<base::Lock>::Leaky
    g_logging_lock = LAZY_INSTANCE_INITIALIZER;

// String sent in the "hello" message to the plugin to describe features.
const char ChromotingInstance::kApiFeatures[] =
    "highQualityScaling injectKeyEvent sendClipboardItem remapKey trapKey "
    "notifyClientDimensions pauseVideo";

bool ChromotingInstance::ParseAuthMethods(const std::string& auth_methods_str,
                                          ClientConfig* config) {
  if (auth_methods_str == "v1_token") {
    config->use_v1_authenticator = true;
  } else {
    config->use_v1_authenticator = false;

    std::vector<std::string> auth_methods;
    base::SplitString(auth_methods_str, ',', &auth_methods);
    for (std::vector<std::string>::iterator it = auth_methods.begin();
         it != auth_methods.end(); ++it) {
      protocol::AuthenticationMethod authentication_method =
          protocol::AuthenticationMethod::FromString(*it);
      if (authentication_method.is_valid())
        config->authentication_methods.push_back(authentication_method);
    }
    if (config->authentication_methods.empty()) {
      LOG(ERROR) << "No valid authentication methods specified.";
      return false;
    }
  }

  return true;
}

ChromotingInstance::ChromotingInstance(PP_Instance pp_instance)
    : pp::InstancePrivate(pp_instance),
      initialized_(false),
      plugin_message_loop_(
          new PluginMessageLoopProxy(&plugin_thread_delegate_)),
      context_(plugin_message_loop_),
      thread_proxy_(new ScopedThreadProxy(plugin_message_loop_)) {
  RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE | PP_INPUTEVENT_CLASS_WHEEL);
  RequestFilteringInputEvents(PP_INPUTEVENT_CLASS_KEYBOARD);

  // Resister this instance to handle debug log messsages.
  RegisterLoggingInstance();

  // Send hello message.
  scoped_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  data->SetInteger("apiVersion", kApiVersion);
  data->SetString("apiFeatures", kApiFeatures);
  data->SetInteger("apiMinVersion", kApiMinMessagingVersion);
  PostChromotingMessage("hello", data.Pass());
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
  // RectangleUpdateDecoder runs on a separate thread so for now we wrap
  // PepperView with a ref-counted proxy object.
  scoped_refptr<FrameConsumerProxy> consumer_proxy =
      new FrameConsumerProxy(plugin_message_loop_);
  rectangle_decoder_ = new RectangleUpdateDecoder(
      context_.decode_message_loop(), consumer_proxy);
  view_.reset(new PepperView(this, &context_, rectangle_decoder_.get()));
  consumer_proxy->Attach(view_->AsWeakPtr());

  return true;
}

void ChromotingInstance::HandleMessage(const pp::Var& message) {
  if (!message.is_string()) {
    LOG(ERROR) << "Received a message that is not a string.";
    return;
  }

  scoped_ptr<base::Value> json(
      base::JSONReader::Read(message.AsString(),
                             base::JSON_ALLOW_TRAILING_COMMAS));
  base::DictionaryValue* message_dict = NULL;
  std::string method;
  base::DictionaryValue* data = NULL;
  if (!json.get() ||
      !json->GetAsDictionary(&message_dict) ||
      !message_dict->GetString("method", &method) ||
      !message_dict->GetDictionary("data", &data)) {
    LOG(ERROR) << "Received invalid message:" << message.AsString();
    return;
  }

  if (method == "connect") {
    ClientConfig config;
    std::string auth_methods;
    if (!data->GetString("hostJid", &config.host_jid) ||
        !data->GetString("hostPublicKey", &config.host_public_key) ||
        !data->GetString("localJid", &config.local_jid) ||
        !data->GetString("sharedSecret", &config.shared_secret) ||
        !data->GetString("authenticationMethods", &auth_methods) ||
        !ParseAuthMethods(auth_methods, &config) ||
        !data->GetString("authenticationTag", &config.authentication_tag)) {
      LOG(ERROR) << "Invalid connect() data.";
      return;
    }

    Connect(config);
  } else if (method == "disconnect") {
    Disconnect();
  } else if (method == "incomingIq") {
    std::string iq;
    if (!data->GetString("iq", &iq)) {
      LOG(ERROR) << "Invalid onIq() data.";
      return;
    }
    OnIncomingIq(iq);
  } else if (method == "releaseAllKeys") {
    ReleaseAllKeys();
  } else if (method == "injectKeyEvent") {
    int usb_keycode = 0;
    bool is_pressed = false;
    if (!data->GetInteger("usbKeycode", &usb_keycode) ||
        !data->GetBoolean("pressed", &is_pressed)) {
      LOG(ERROR) << "Invalid injectKeyEvent.";
      return;
    }

    protocol::KeyEvent event;
    event.set_usb_keycode(usb_keycode);
    event.set_pressed(is_pressed);
    // Even though new hosts will ignore keycode, it's a required field.
    event.set_keycode(0);
    InjectKeyEvent(event);
  } else if (method == "remapKey") {
    int from_keycode = 0;
    int to_keycode = 0;
    if (!data->GetInteger("fromKeycode", &from_keycode) ||
        !data->GetInteger("toKeycode", &to_keycode)) {
      LOG(ERROR) << "Invalid remapKey.";
      return;
    }

    RemapKey(from_keycode, to_keycode);
  } else if (method == "trapKey") {
    int keycode = 0;
    bool trap = false;
    if (!data->GetInteger("keycode", &keycode) ||
        !data->GetBoolean("trap", &trap)) {
      LOG(ERROR) << "Invalid trapKey.";
      return;
    }

    TrapKey(keycode, trap);
  } else if (method == "sendClipboardItem") {
    std::string mime_type;
    std::string item;
    if (!data->GetString("mimeType", &mime_type) ||
        !data->GetString("item", &item)) {
      LOG(ERROR) << "Invalid sendClipboardItem() data.";
      return;
    }
    SendClipboardItem(mime_type, item);
  } else if (method == "notifyClientDimensions") {
    int width = 0;
    int height = 0;
    if (!data->GetInteger("width", &width) ||
        !data->GetInteger("height", &height)) {
      LOG(ERROR) << "Invalid notifyClientDimensions.";
      return;
    }
    NotifyClientDimensions(width, height);
  } else if (method == "pauseVideo") {
    bool pause = false;
    if (!data->GetBoolean("pause", &pause)) {
      LOG(ERROR) << "Invalid pauseVideo.";
      return;
    }
    PauseVideo(pause);
  }
}

void ChromotingInstance::DidChangeView(const pp::Rect& position,
                                       const pp::Rect& clip) {
  DCHECK(plugin_message_loop_->BelongsToCurrentThread());

  SkISize new_size = SkISize::Make(position.width(), position.height());
  SkIRect new_clip =
    SkIRect::MakeXYWH(clip.x(), clip.y(), clip.width(), clip.height());

  view_->SetView(new_size, new_clip);

  if (mouse_input_filter_.get()) {
    mouse_input_filter_->set_input_size(view_->get_view_size());
  }
}

bool ChromotingInstance::HandleInputEvent(const pp::InputEvent& event) {
  DCHECK(plugin_message_loop_->BelongsToCurrentThread());

  if (!IsConnected())
    return false;

  // TODO(wez): When we have a good hook into Host dimensions changes, move
  // this there.
  // If |input_handler_| is valid, then |mouse_input_filter_| must also be
  // since they are constructed together as part of the input pipeline
  mouse_input_filter_->set_output_size(view_->get_screen_size());

  return input_handler_->HandleInputEvent(event);
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

void ChromotingInstance::SetDesktopSize(int width, int height) {
  scoped_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  data->SetInteger("width", width);
  data->SetInteger("height", height);
  PostChromotingMessage("onDesktopSize", data.Pass());

  GetScriptableObject()->SetDesktopSize(width, height);
}

void ChromotingInstance::SetConnectionState(
    ConnectionState state,
    ConnectionError error) {
  scoped_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  data->SetString("state", ConnectionStateToString(state));
  data->SetString("error", ConnectionErrorToString(error));
  PostChromotingMessage("onConnectionStatus", data.Pass());

  GetScriptableObject()->SetConnectionStatus(state, error);
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

void ChromotingInstance::Connect(const ClientConfig& config) {
  DCHECK(plugin_message_loop_->BelongsToCurrentThread());

  host_connection_.reset(new protocol::ConnectionToHost(
      context_.network_message_loop(), this, true));
  client_.reset(new ChromotingClient(config, &context_, host_connection_.get(),
                                     view_.get(), rectangle_decoder_.get(),
                                     base::Closure()));

  // Construct the input pipeline
  mouse_input_filter_.reset(
      new protocol::MouseInputFilter(host_connection_->input_stub()));
  mouse_input_filter_->set_input_size(view_->get_view_size());
  input_tracker_.reset(
      new protocol::InputEventTracker(mouse_input_filter_.get()));

#if defined(OS_MACOSX)
  // On Mac we need an extra filter to inject missing keyup events.
  // See remoting/client/plugin/mac_key_event_processor.h for more details.
  mac_key_event_processor_.reset(
      new MacKeyEventProcessor(input_tracker_.get()));
  key_mapper_.set_input_stub(mac_key_event_processor_.get());
#else
  key_mapper_.set_input_stub(input_tracker_.get());
#endif
  input_handler_.reset(
      new PepperInputHandler(&key_mapper_));

  LOG(INFO) << "Connecting to " << config.host_jid
            << ". Local jid: " << config.local_jid << ".";

  // Setup the XMPP Proxy.
  xmpp_proxy_ = new PepperXmppProxy(
      base::Bind(&ChromotingInstance::SendOutgoingIq, AsWeakPtr()),
      plugin_message_loop_,
      context_.network_message_loop());

  // Kick off the connection.
  client_->Start(xmpp_proxy_);

  // Start timer that periodically sends perf stats.
  plugin_message_loop_->PostDelayedTask(
      FROM_HERE, base::Bind(&ChromotingInstance::SendPerfStats, AsWeakPtr()),
      base::TimeDelta::FromMilliseconds(kPerfStatsIntervalMs));

  VLOG(1) << "Connection status: Initializing";
  SetConnectionState(STATE_INITIALIZING, ERROR_NONE);
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
  input_tracker_.reset();
  mouse_input_filter_.reset();
  host_connection_.reset();

  SetConnectionState(STATE_CLOSED, ERROR_NONE);
}

void ChromotingInstance::OnIncomingIq(const std::string& iq) {
  xmpp_proxy_->OnIq(iq);
}

void ChromotingInstance::ReleaseAllKeys() {
  if (IsConnected())
    input_tracker_->ReleaseAll();
}

void ChromotingInstance::InjectKeyEvent(const protocol::KeyEvent& event) {
  // Inject after the KeyEventMapper, so the event won't get mapped or trapped.
  if (IsConnected())
    input_tracker_->InjectKeyEvent(event);
}

void ChromotingInstance::RemapKey(uint32 in_usb_keycode,
                                  uint32 out_usb_keycode) {
  key_mapper_.RemapKey(in_usb_keycode, out_usb_keycode);
}

void ChromotingInstance::TrapKey(uint32 usb_keycode, bool trap) {
  key_mapper_.TrapKey(usb_keycode, trap);
}

void ChromotingInstance::SendClipboardItem(const std::string& mime_type,
                                           const std::string& item) {
  if (!IsConnected()) {
    return;
  }
  protocol::ClipboardEvent event;
  event.set_mime_type(mime_type);
  event.set_data(item);
  host_connection_->clipboard_stub()->InjectClipboardEvent(event);
}

void ChromotingInstance::NotifyClientDimensions(int width, int height) {
  if (!IsConnected()) {
    return;
  }
  protocol::ClientDimensions client_dimensions;
  client_dimensions.set_width(width);
  client_dimensions.set_height(height);
  host_connection_->host_stub()->NotifyClientDimensions(client_dimensions);
}

void ChromotingInstance::PauseVideo(bool pause) {
  if (!IsConnected()) {
    return;
  }
  protocol::VideoControl video_control;
  video_control.set_enable(!pause);
  host_connection_->host_stub()->ControlVideo(video_control);
}

ChromotingStats* ChromotingInstance::GetStats() {
  if (!client_.get())
    return NULL;
  return client_->GetStats();
}

void ChromotingInstance::PostChromotingMessage(
    const std::string& method,
    scoped_ptr<base::DictionaryValue> data) {
  scoped_ptr<base::DictionaryValue> message(new base::DictionaryValue());
  message->SetString("method", method);
  message->Set("data", data.release());

  std::string message_json;
  base::JSONWriter::Write(message.get(), &message_json);
  PostMessage(pp::Var(message_json));
}

void ChromotingInstance::SendTrappedKey(uint32 usb_keycode, bool pressed) {
  scoped_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  data->SetInteger("usbKeycode", usb_keycode);
  data->SetBoolean("pressed", pressed);
  PostChromotingMessage("trappedKeyEvent", data.Pass());
}

void ChromotingInstance::SendOutgoingIq(const std::string& iq) {
  scoped_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  data->SetString("iq", iq);
  PostChromotingMessage("sendOutgoingIq", data.Pass());

  GetScriptableObject()->SendIq(iq);
}

void ChromotingInstance::SendPerfStats() {
  if (!client_.get()) {
    return;
  }

  plugin_message_loop_->PostDelayedTask(
      FROM_HERE, base::Bind(&ChromotingInstance::SendPerfStats, AsWeakPtr()),
      base::TimeDelta::FromMilliseconds(kPerfStatsIntervalMs));

  scoped_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  ChromotingStats* stats = client_->GetStats();
  data->SetDouble("videoBandwidth", stats->video_bandwidth()->Rate());
  data->SetDouble("videoFrameRate", stats->video_frame_rate()->Rate());
  data->SetDouble("captureLatency", stats->video_capture_ms()->Average());
  data->SetDouble("encodeLatency", stats->video_encode_ms()->Average());
  data->SetDouble("decodeLatency", stats->video_decode_ms()->Average());
  data->SetDouble("renderLatency", stats->video_paint_ms()->Average());
  data->SetDouble("roundtripLatency", stats->round_trip_ms()->Average());
  PostChromotingMessage("onPerfStats", data.Pass());
}

void ChromotingInstance::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  scoped_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  data->SetString("mimeType", event.mime_type());
  data->SetString("item", event.data());
  PostChromotingMessage("injectClipboardItem", data.Pass());
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
  ChromotingScriptableObject* scriptable_object = GetScriptableObject();
  if (scriptable_object) {
    scoped_ptr<base::DictionaryValue> data(new base::DictionaryValue());
    data->SetString("message", message);
    PostChromotingMessage("logDebugMessage", data.Pass());

    scriptable_object->LogDebugInfo(message);
  }
  g_logging_to_plugin = false;
}

bool ChromotingInstance::IsConnected() {
  return host_connection_.get() &&
    (host_connection_->state() == protocol::ConnectionToHost::CONNECTED);
}

}  // namespace remoting
