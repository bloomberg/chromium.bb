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
#include "base/strings/string_split.h"
#include "base/synchronization/lock.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "jingle/glue/thread_wrapper.h"
#include "media/base/media.h"
#include "net/socket/ssl_server_socket.h"
#include "ppapi/cpp/completion_callback.h"
#include "ppapi/cpp/dev/url_util_dev.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/mouse_cursor.h"
#include "ppapi/cpp/rect.h"
#include "remoting/base/constants.h"
#include "remoting/base/util.h"
#include "remoting/client/chromoting_client.h"
#include "remoting/client/client_config.h"
#include "remoting/client/frame_consumer_proxy.h"
#include "remoting/client/plugin/pepper_audio_player.h"
#include "remoting/client/plugin/pepper_input_handler.h"
#include "remoting/client/plugin/pepper_port_allocator.h"
#include "remoting/client/plugin/pepper_view.h"
#include "remoting/client/plugin/pepper_xmpp_proxy.h"
#include "remoting/client/rectangle_update_decoder.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/host_stub.h"
#include "remoting/protocol/libjingle_transport_factory.h"

// Windows defines 'PostMessage', so we have to undef it.
#if defined(PostMessage)
#undef PostMessage
#endif

namespace remoting {

namespace {

// 32-bit BGRA is 4 bytes per pixel.
const int kBytesPerPixel = 4;

// Default DPI to assume for old clients that use notifyClientDimensions.
const int kDefaultDPI = 96;

// Interval at which to sample performance statistics.
const int kPerfStatsIntervalMs = 1000;

// URL scheme used by Chrome apps and extensions.
const char kChromeExtensionUrlScheme[] = "chrome-extension";

std::string ConnectionStateToString(protocol::ConnectionToHost::State state) {
  // Values returned by this function must match the
  // remoting.ClientSession.State enum in JS code.
  switch (state) {
    case protocol::ConnectionToHost::INITIALIZING:
      return "INITIALIZING";
    case protocol::ConnectionToHost::CONNECTING:
      return "CONNECTING";
    case protocol::ConnectionToHost::CONNECTED:
      return "CONNECTED";
    case protocol::ConnectionToHost::CLOSED:
      return "CLOSED";
    case protocol::ConnectionToHost::FAILED:
      return "FAILED";
  }
  NOTREACHED();
  return "";
}

// TODO(sergeyu): Ideally we should just pass ErrorCode to the webapp
// and let it handle it, but it would be hard to fix it now because
// client plugin and webapp versions may not be in sync. It should be
// easy to do after we are finished moving the client plugin to NaCl.
std::string ConnectionErrorToString(protocol::ErrorCode error) {
  // Values returned by this function must match the
  // remoting.ClientSession.Error enum in JS code.
  switch (error) {
    case protocol::OK:
      return "NONE";

    case protocol::PEER_IS_OFFLINE:
      return "HOST_IS_OFFLINE";

    case protocol::SESSION_REJECTED:
    case protocol::AUTHENTICATION_FAILED:
      return "SESSION_REJECTED";

    case protocol::INCOMPATIBLE_PROTOCOL:
      return "INCOMPATIBLE_PROTOCOL";

    case protocol::HOST_OVERLOAD:
      return "HOST_OVERLOAD";

    case protocol::CHANNEL_CONNECTION_ERROR:
    case protocol::SIGNALING_ERROR:
    case protocol::SIGNALING_TIMEOUT:
    case protocol::UNKNOWN_ERROR:
      return "NETWORK_FAILURE";
  }
  DLOG(FATAL) << "Unknown error code" << error;
  return  "";
}

// This flag blocks LOGs to the UI if we're already in the middle of logging
// to the UI. This prevents a potential infinite loop if we encounter an error
// while sending the log message to the UI.
bool g_logging_to_plugin = false;
bool g_has_logging_instance = false;
base::LazyInstance<scoped_refptr<base::SingleThreadTaskRunner> >::Leaky
    g_logging_task_runner = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<base::WeakPtr<ChromotingInstance> >::Leaky
    g_logging_instance = LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<base::Lock>::Leaky
    g_logging_lock = LAZY_INSTANCE_INITIALIZER;
logging::LogMessageHandlerFunction g_logging_old_handler = NULL;

}  // namespace

// String sent in the "hello" message to the plugin to describe features.
const char ChromotingInstance::kApiFeatures[] =
    "highQualityScaling injectKeyEvent sendClipboardItem remapKey trapKey "
    "notifyClientDimensions notifyClientResolution pauseVideo pauseAudio "
    "asyncPin";

bool ChromotingInstance::ParseAuthMethods(const std::string& auth_methods_str,
                                          ClientConfig* config) {
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

  return true;
}

ChromotingInstance::ChromotingInstance(PP_Instance pp_instance)
    : pp::Instance(pp_instance),
      initialized_(false),
      plugin_task_runner_(
          new PluginThreadTaskRunner(&plugin_thread_delegate_)),
      context_(plugin_task_runner_),
      input_tracker_(&mouse_input_filter_),
#if defined(OS_MACOSX)
      // On Mac we need an extra filter to inject missing keyup events.
      // See remoting/client/plugin/mac_key_event_processor.h for more details.
      mac_key_event_processor_(&input_tracker_),
      key_mapper_(&mac_key_event_processor_),
#else
      key_mapper_(&input_tracker_),
#endif
      input_handler_(&key_mapper_),
      use_async_pin_dialog_(false),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
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
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  // Unregister this instance so that debug log messages will no longer be sent
  // to it. This will stop all logging in all Chromoting instances.
  UnregisterLoggingInstance();

  // PepperView must be destroyed before the client.
  view_.reset();

  if (client_.get()) {
    client_->Stop(base::Bind(&PluginThreadTaskRunner::Quit,
                  plugin_task_runner_));
  } else {
    plugin_task_runner_->Quit();
  }

  // Ensure that nothing touches the plugin thread delegate after this point.
  plugin_task_runner_->DetachAndRunShutdownLoop();

  // Stopping the context shuts down all chromoting threads.
  context_.Stop();
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

  // Check that the calling content is part of an app or extension.
  if (!IsCallerAppOrExtension()) {
    LOG(ERROR) << "Not an app or extension";
    return false;
  }

  // Enable support for SSL server sockets, which must be done as early as
  // possible, preferably before any NSS SSL sockets (client or server) have
  // been created.
  // It's possible that the hosting process has already made use of SSL, in
  // which case, there may be a slight race.
  net::EnableSSLServerSockets();

  // Start all the threads.
  context_.Start();

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
        !data->GetString("authenticationMethods", &auth_methods) ||
        !ParseAuthMethods(auth_methods, &config) ||
        !data->GetString("authenticationTag", &config.authentication_tag)) {
      LOG(ERROR) << "Invalid connect() data.";
      return;
    }
    if (use_async_pin_dialog_) {
      config.fetch_secret_callback =
          base::Bind(&ChromotingInstance::FetchSecretFromDialog,
                     this->AsWeakPtr());
    } else {
      std::string shared_secret;
      if (!data->GetString("sharedSecret", &shared_secret)) {
        LOG(ERROR) << "sharedSecret not specified in connect().";
        return;
      }
      config.fetch_secret_callback =
          base::Bind(&ChromotingInstance::FetchSecretFromString, shared_secret);
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
  } else if (method == "notifyClientDimensions" ||
             method == "notifyClientResolution") {
    // notifyClientResolution's width and height are in pixels,
    // notifyClientDimension's in DIPs, but since for the latter
    // we assume 96dpi, DIPs and pixels are equivalent.
    int width = 0;
    int height = 0;
    if (!data->GetInteger("width", &width) ||
        !data->GetInteger("height", &height) ||
        width <= 0 || height <= 0) {
      LOG(ERROR) << "Invalid " << method << ".";
      return;
    }

    // notifyClientResolution requires that DPI be specified.
    // For notifyClientDimensions we assume 96dpi.
    int x_dpi = kDefaultDPI;
    int y_dpi = kDefaultDPI;
    if (method == "notifyClientResolution" &&
        (!data->GetInteger("x_dpi", &x_dpi) ||
         !data->GetInteger("y_dpi", &y_dpi) ||
         x_dpi <= 0 || y_dpi <= 0)) {
      LOG(ERROR) << "Invalid notifyClientResolution.";
      return;
    }

    NotifyClientResolution(width, height, x_dpi, y_dpi);
  } else if (method == "pauseVideo") {
    bool pause = false;
    if (!data->GetBoolean("pause", &pause)) {
      LOG(ERROR) << "Invalid pauseVideo.";
      return;
    }
    PauseVideo(pause);
  } else if (method == "pauseAudio") {
    bool pause = false;
    if (!data->GetBoolean("pause", &pause)) {
      LOG(ERROR) << "Invalid pauseAudio.";
      return;
    }
    PauseAudio(pause);
  } else if (method == "useAsyncPinDialog") {
    use_async_pin_dialog_ = true;
  } else if (method == "onPinFetched") {
    std::string pin;
    if (!data->GetString("pin", &pin)) {
      LOG(ERROR) << "Invalid onPinFetched.";
      return;
    }
    OnPinFetched(pin);
  }
}

void ChromotingInstance::DidChangeView(const pp::View& view) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  plugin_view_ = view;
  if (view_) {
    view_->SetView(view);
    mouse_input_filter_.set_input_size(view_->get_view_size_dips());
  }
}

bool ChromotingInstance::HandleInputEvent(const pp::InputEvent& event) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  if (!IsConnected())
    return false;

  return input_handler_.HandleInputEvent(event);
}

void ChromotingInstance::SetDesktopSize(const SkISize& size,
                                        const SkIPoint& dpi) {
  mouse_input_filter_.set_output_size(size);

  scoped_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  data->SetInteger("width", size.width());
  data->SetInteger("height", size.height());
  if (dpi.x())
    data->SetInteger("x_dpi", dpi.x());
  if (dpi.y())
    data->SetInteger("y_dpi", dpi.y());
  PostChromotingMessage("onDesktopSize", data.Pass());
}

void ChromotingInstance::OnConnectionState(
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error) {
  scoped_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  data->SetString("state", ConnectionStateToString(state));
  data->SetString("error", ConnectionErrorToString(error));
  PostChromotingMessage("onConnectionStatus", data.Pass());
}

void ChromotingInstance::OnConnectionReady(bool ready) {
  scoped_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  data->SetBoolean("ready", ready);
  PostChromotingMessage("onConnectionReady", data.Pass());
}

void ChromotingInstance::FetchSecretFromDialog(
    const protocol::SecretFetchedCallback& secret_fetched_callback) {
  // Once the Session object calls this function, it won't continue the
  // authentication until the callback is called (or connection is canceled).
  // So, it's impossible to reach this with a callback already registered.
  DCHECK(secret_fetched_callback_.is_null());
  secret_fetched_callback_ = secret_fetched_callback;
  scoped_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  PostChromotingMessage("fetchPin", data.Pass());
}

void ChromotingInstance::FetchSecretFromString(
    const std::string& shared_secret,
    const protocol::SecretFetchedCallback& secret_fetched_callback) {
  secret_fetched_callback.Run(shared_secret);
}

protocol::ClipboardStub* ChromotingInstance::GetClipboardStub() {
  // TODO(sergeyu): Move clipboard handling to a separate class.
  // crbug.com/138108
  return this;
}

protocol::CursorShapeStub* ChromotingInstance::GetCursorShapeStub() {
  // TODO(sergeyu): Move cursor shape code to a separate class.
  // crbug.com/138108
  return this;
}

void ChromotingInstance::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  scoped_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  data->SetString("mimeType", event.mime_type());
  data->SetString("item", event.data());
  PostChromotingMessage("injectClipboardItem", data.Pass());
}

void ChromotingInstance::SetCursorShape(
    const protocol::CursorShapeInfo& cursor_shape) {
  if (!cursor_shape.has_data() ||
      !cursor_shape.has_width() ||
      !cursor_shape.has_height() ||
      !cursor_shape.has_hotspot_x() ||
      !cursor_shape.has_hotspot_y()) {
    return;
  }

  int width = cursor_shape.width();
  int height = cursor_shape.height();

  if (width < 0 || height < 0) {
    return;
  }

  if (width > 32 || height > 32) {
    VLOG(2) << "Cursor too large for SetCursor: "
            << width << "x" << height << " > 32x32";
    return;
  }

  uint32 cursor_total_bytes = width * height * kBytesPerPixel;
  if (cursor_shape.data().size() < cursor_total_bytes) {
    VLOG(2) << "Expected " << cursor_total_bytes << " bytes for a "
            << width << "x" << height << " cursor. Only received "
            << cursor_shape.data().size() << " bytes";
    return;
  }

  if (pp::ImageData::GetNativeImageDataFormat() !=
      PP_IMAGEDATAFORMAT_BGRA_PREMUL) {
    VLOG(2) << "Unable to set cursor shape - non-native image format";
    return;
  }

  int hotspot_x = cursor_shape.hotspot_x();
  int hotspot_y = cursor_shape.hotspot_y();

  pp::ImageData cursor_image(this, PP_IMAGEDATAFORMAT_BGRA_PREMUL,
                             pp::Size(width, height), false);

  int bytes_per_row = width * kBytesPerPixel;
  const uint8* src_row_data = reinterpret_cast<const uint8*>(
      cursor_shape.data().data());
  uint8* dst_row_data = reinterpret_cast<uint8*>(cursor_image.data());
  for (int row = 0; row < height; row++) {
    memcpy(dst_row_data, src_row_data, bytes_per_row);
    src_row_data += bytes_per_row;
    dst_row_data += cursor_image.stride();
  }

  pp::MouseCursor::SetCursor(this, PP_MOUSECURSOR_TYPE_CUSTOM,
                             cursor_image,
                             pp::Point(hotspot_x, hotspot_y));
}

void ChromotingInstance::OnFirstFrameReceived() {
  scoped_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  PostChromotingMessage("onFirstFrameReceived", data.Pass());
}

void ChromotingInstance::Connect(const ClientConfig& config) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  jingle_glue::JingleThreadWrapper::EnsureForCurrentMessageLoop();

  // RectangleUpdateDecoder runs on a separate thread so for now we wrap
  // PepperView with a ref-counted proxy object.
  scoped_refptr<FrameConsumerProxy> consumer_proxy =
      new FrameConsumerProxy(plugin_task_runner_);

  host_connection_.reset(new protocol::ConnectionToHost(true));
  scoped_ptr<AudioPlayer> audio_player(new PepperAudioPlayer(this));
  client_.reset(new ChromotingClient(config, &context_,
                                     host_connection_.get(), this,
                                     consumer_proxy, audio_player.Pass()));

  view_.reset(new PepperView(this, &context_, client_->GetFrameProducer()));
  consumer_proxy->Attach(view_->AsWeakPtr());
  if (!plugin_view_.is_null()) {
    view_->SetView(plugin_view_);
  }

  // Connect the input pipeline to the protocol stub & initialize components.
  mouse_input_filter_.set_input_stub(host_connection_->input_stub());
  mouse_input_filter_.set_input_size(view_->get_view_size_dips());

  LOG(INFO) << "Connecting to " << config.host_jid
            << ". Local jid: " << config.local_jid << ".";

  // Setup the XMPP Proxy.
  xmpp_proxy_ = new PepperXmppProxy(
      base::Bind(&ChromotingInstance::SendOutgoingIq, AsWeakPtr()),
      plugin_task_runner_, context_.main_task_runner());

  scoped_ptr<cricket::HttpPortAllocatorBase> port_allocator(
      PepperPortAllocator::Create(this));
  scoped_ptr<protocol::TransportFactory> transport_factory(
      new protocol::LibjingleTransportFactory(port_allocator.Pass(), false));

  // Kick off the connection.
  client_->Start(xmpp_proxy_, transport_factory.Pass());

  // Start timer that periodically sends perf stats.
  plugin_task_runner_->PostDelayedTask(
      FROM_HERE, base::Bind(&ChromotingInstance::SendPerfStats, AsWeakPtr()),
      base::TimeDelta::FromMilliseconds(kPerfStatsIntervalMs));
}

void ChromotingInstance::Disconnect() {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  // PepperView must be destroyed before the client.
  view_.reset();

  LOG(INFO) << "Disconnecting from host.";
  if (client_.get()) {
    // TODO(sergeyu): Should we disconnect asynchronously?
    base::WaitableEvent done_event(true, false);
    client_->Stop(base::Bind(&base::WaitableEvent::Signal,
                             base::Unretained(&done_event)));
    done_event.Wait();
    client_.reset();
  }

  // Disconnect the input pipeline and teardown the connection.
  mouse_input_filter_.set_input_stub(NULL);
  host_connection_.reset();
}

void ChromotingInstance::OnIncomingIq(const std::string& iq) {
  xmpp_proxy_->OnIq(iq);
}

void ChromotingInstance::ReleaseAllKeys() {
  if (IsConnected())
    input_tracker_.ReleaseAll();
}

void ChromotingInstance::InjectKeyEvent(const protocol::KeyEvent& event) {
  // Inject after the KeyEventMapper, so the event won't get mapped or trapped.
  if (IsConnected())
    input_tracker_.InjectKeyEvent(event);
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

void ChromotingInstance::NotifyClientResolution(int width,
                                                int height,
                                                int x_dpi,
                                                int y_dpi) {
  if (!IsConnected()) {
    return;
  }

  protocol::ClientResolution client_resolution;
  client_resolution.set_width(width);
  client_resolution.set_height(height);
  client_resolution.set_x_dpi(x_dpi);
  client_resolution.set_y_dpi(y_dpi);

  // Include the legacy width & height in DIPs for use by older hosts.
  client_resolution.set_dips_width((width * kDefaultDPI) / x_dpi);
  client_resolution.set_dips_height((height * kDefaultDPI) / y_dpi);

  host_connection_->host_stub()->NotifyClientResolution(client_resolution);
}

void ChromotingInstance::PauseVideo(bool pause) {
  if (!IsConnected()) {
    return;
  }
  protocol::VideoControl video_control;
  video_control.set_enable(!pause);
  host_connection_->host_stub()->ControlVideo(video_control);
}

void ChromotingInstance::PauseAudio(bool pause) {
  if (!IsConnected()) {
    return;
  }
  protocol::AudioControl audio_control;
  audio_control.set_enable(!pause);
  host_connection_->host_stub()->ControlAudio(audio_control);
}

void ChromotingInstance::OnPinFetched(const std::string& pin) {
  if (!secret_fetched_callback_.is_null()) {
    secret_fetched_callback_.Run(pin);
    secret_fetched_callback_.Reset();
  } else {
    VLOG(1) << "Ignored OnPinFetched received without a pending fetch.";
  }
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
}

void ChromotingInstance::SendPerfStats() {
  if (!client_.get()) {
    return;
  }

  plugin_task_runner_->PostDelayedTask(
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
  g_logging_instance.Get() = weak_factory_.GetWeakPtr();
  g_logging_task_runner.Get() = plugin_task_runner_;
  g_has_logging_instance = true;
}

void ChromotingInstance::UnregisterLoggingInstance() {
  base::AutoLock lock(g_logging_lock.Get());

  // Don't unregister unless we're the currently registered instance.
  if (this != g_logging_instance.Get().get())
    return;

  // Unregister this instance for logging.
  g_has_logging_instance = false;
  g_logging_instance.Get().reset();
  g_logging_task_runner.Get() = NULL;

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
    scoped_refptr<base::SingleThreadTaskRunner> logging_task_runner;
    base::WeakPtr<ChromotingInstance> logging_instance;

    {
      base::AutoLock lock(g_logging_lock.Get());
      // If we're on the logging thread and |g_logging_to_plugin| is set then
      // this LOG message came from handling a previous LOG message and we
      // should skip it to avoid an infinite loop of LOG messages.
      if (!g_logging_task_runner.Get()->BelongsToCurrentThread() ||
          !g_logging_to_plugin) {
        logging_task_runner = g_logging_task_runner.Get();
        logging_instance = g_logging_instance.Get();
      }
    }

    if (logging_task_runner.get()) {
      std::string message = remoting::GetTimestampString();
      message += (str.c_str() + message_start);

      logging_task_runner->PostTask(
          FROM_HERE, base::Bind(&ChromotingInstance::ProcessLogToUI,
                                logging_instance, message));
    }
  }

  if (g_logging_old_handler)
    return (g_logging_old_handler)(severity, file, line, message_start, str);
  return false;
}

void ChromotingInstance::ProcessLogToUI(const std::string& message) {
  DCHECK(plugin_task_runner_->BelongsToCurrentThread());

  // This flag (which is set only here) is used to prevent LogToUI from posting
  // new tasks while we're in the middle of servicing a LOG call. This can
  // happen if the call to LogDebugInfo tries to LOG anything.
  // Since it is read on the plugin thread, we don't need to lock to set it.
  g_logging_to_plugin = true;
  scoped_ptr<base::DictionaryValue> data(new base::DictionaryValue());
  data->SetString("message", message);
  PostChromotingMessage("logDebugMessage", data.Pass());
  g_logging_to_plugin = false;
}

bool ChromotingInstance::IsCallerAppOrExtension() {
  const pp::URLUtil_Dev* url_util = pp::URLUtil_Dev::Get();
  if (!url_util)
    return false;

  PP_URLComponents_Dev url_components;
  pp::Var url_var = url_util->GetDocumentURL(this, &url_components);
  if (!url_var.is_string())
    return false;

  std::string url = url_var.AsString();
  std::string url_scheme = url.substr(url_components.scheme.begin,
                                      url_components.scheme.len);
  return url_scheme == kChromeExtensionUrlScheme;
}

bool ChromotingInstance::IsConnected() {
  return host_connection_.get() &&
    (host_connection_->state() == protocol::ConnectionToHost::CONNECTED);
}

}  // namespace remoting
