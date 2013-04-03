// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(ajwong): We need to come up with a better description of the
// responsibilities for each thread.

#ifndef REMOTING_CLIENT_PLUGIN_CHROMOTING_INSTANCE_H_
#define REMOTING_CLIENT_PLUGIN_CHROMOTING_INSTANCE_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_rect.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/cpp/var.h"
#include "third_party/skia/include/core/SkPoint.h"
#include "third_party/skia/include/core/SkSize.h"

// Windows defines 'PostMessage', so we have to undef it before we
// include instance_private.h
#if defined(PostMessage)
#undef PostMessage
#endif

#include "ppapi/cpp/instance.h"
#include "remoting/client/client_context.h"
#include "remoting/client/client_user_interface.h"
#include "remoting/client/key_event_mapper.h"
#include "remoting/client/plugin/mac_key_event_processor.h"
#include "remoting/client/plugin/pepper_input_handler.h"
#include "remoting/client/plugin/pepper_plugin_thread_delegate.h"
#include "remoting/proto/event.pb.h"
#include "remoting/protocol/clipboard_stub.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/cursor_shape_stub.h"
#include "remoting/protocol/input_event_tracker.h"
#include "remoting/protocol/mouse_input_filter.h"
#include "remoting/protocol/negotiating_client_authenticator.h"

namespace base {
class DictionaryValue;
}  // namespace base

namespace pp {
class InputEvent;
class Module;
}  // namespace pp

namespace remoting {

class ChromotingClient;
class ChromotingStats;
class ClientContext;
class FrameConsumerProxy;
class PepperAudioPlayer;
class PepperView;
class PepperXmppProxy;
class RectangleUpdateDecoder;

struct ClientConfig;

class ChromotingInstance :
      public ClientUserInterface,
      public protocol::ClipboardStub,
      public protocol::CursorShapeStub,
      public pp::Instance,
      public base::SupportsWeakPtr<ChromotingInstance> {
 public:
  // Plugin API version. This should be incremented whenever the API
  // interface changes.
  static const int kApiVersion = 7;

  // Plugin API features. This allows orthogonal features to be supported
  // without bumping the API version.
  static const char kApiFeatures[];

  // Backward-compatibility version used by for the messaging
  // interface. Should be updated whenever we remove support for
  // an older version of the API.
  static const int kApiMinMessagingVersion = 5;

  // Backward-compatibility version used by for the ScriptableObject
  // interface. Should be updated whenever we remove support for
  // an older version of the API.
  static const int kApiMinScriptableVersion = 5;

  // Helper method to parse authentication_methods parameter.
  static bool ParseAuthMethods(const std::string& auth_methods,
                               ClientConfig* config);

  explicit ChromotingInstance(PP_Instance instance);
  virtual ~ChromotingInstance();

  // pp::Instance interface.
  virtual void DidChangeView(const pp::View& view) OVERRIDE;
  virtual bool Init(uint32_t argc, const char* argn[],
                    const char* argv[]) OVERRIDE;
  virtual void HandleMessage(const pp::Var& message) OVERRIDE;
  virtual bool HandleInputEvent(const pp::InputEvent& event) OVERRIDE;

  // ClientUserInterface interface.
  virtual void OnConnectionState(protocol::ConnectionToHost::State state,
                                 protocol::ErrorCode error) OVERRIDE;
  virtual void OnConnectionReady(bool ready) OVERRIDE;
  virtual protocol::ClipboardStub* GetClipboardStub() OVERRIDE;
  virtual protocol::CursorShapeStub* GetCursorShapeStub() OVERRIDE;

  // protocol::ClipboardStub interface.
  virtual void InjectClipboardEvent(
      const protocol::ClipboardEvent& event) OVERRIDE;

  // protocol::CursorShapeStub interface.
  virtual void SetCursorShape(
      const protocol::CursorShapeInfo& cursor_shape) OVERRIDE;

  // Called by PepperView.
  void SetDesktopSize(const SkISize& size, const SkIPoint& dpi);
  void OnFirstFrameReceived();

  // Return statistics record by ChromotingClient.
  // If no connection is currently active then NULL will be returned.
  ChromotingStats* GetStats();

  // Registers a global log message handler that redirects the log output to
  // our plugin instance.
  // This is called by the plugin's PPP_InitializeModule.
  // Note that no logging will be processed unless a ChromotingInstance has been
  // registered for logging (see RegisterLoggingInstance).
  static void RegisterLogMessageHandler();

  // Registers this instance so it processes messages sent by the global log
  // message handler. This overwrites any previously registered instance.
  void RegisterLoggingInstance();

  // Unregisters this instance so that debug log messages will no longer be sent
  // to it. If this instance is not the currently registered logging instance,
  // then the currently registered instance will stay in effect.
  void UnregisterLoggingInstance();

  // A Log Message Handler that is called after each LOG message has been
  // processed. This must be of type LogMessageHandlerFunction defined in
  // base/logging.h.
  static bool LogToUI(int severity, const char* file, int line,
                      size_t message_start, const std::string& str);

 private:
  FRIEND_TEST_ALL_PREFIXES(ChromotingInstanceTest, TestCaseSetup);

  // Used as the |FetchSecretCallback| for IT2Me (or Me2Me from old webapps).
  // Immediately calls |secret_fetched_callback| with |shared_secret|.
  static void FetchSecretFromString(
      const std::string& shared_secret,
      const protocol::SecretFetchedCallback& secret_fetched_callback);

  // Message handlers for messages that come from JavaScript. Called
  // from HandleMessage().
  void Connect(const ClientConfig& config);
  void Disconnect();
  void OnIncomingIq(const std::string& iq);
  void ReleaseAllKeys();
  void InjectKeyEvent(const protocol::KeyEvent& event);
  void RemapKey(uint32 in_usb_keycode, uint32 out_usb_keycode);
  void TrapKey(uint32 usb_keycode, bool trap);
  void SendClipboardItem(const std::string& mime_type, const std::string& item);
  void NotifyClientResolution(int width, int height, int x_dpi, int y_dpi);
  void PauseVideo(bool pause);
  void PauseAudio(bool pause);
  void OnPinFetched(const std::string& pin);

  // Helper method to post messages to the webapp.
  void PostChromotingMessage(const std::string& method,
                             scoped_ptr<base::DictionaryValue> data);

  // Posts trapped keys to the web-app to handle.
  void SendTrappedKey(uint32 usb_keycode, bool pressed);

  // Callback for PepperXmppProxy.
  void SendOutgoingIq(const std::string& iq);

  void SendPerfStats();

  void ProcessLogToUI(const std::string& message);

  // Returns true if the hosting content has the chrome-extension:// scheme.
  bool IsCallerAppOrExtension();

  // Returns true if there is a ConnectionToHost and it is connected.
  bool IsConnected();

  // Used as the |FetchSecretCallback| for Me2Me connections.
  // Uses the PIN request dialog in the webapp to obtain the shared secret.
  void FetchSecretFromDialog(
      const protocol::SecretFetchedCallback& secret_fetched_callback);

  bool initialized_;

  PepperPluginThreadDelegate plugin_thread_delegate_;
  scoped_refptr<PluginThreadTaskRunner> plugin_task_runner_;
  ClientContext context_;
  scoped_refptr<RectangleUpdateDecoder> rectangle_decoder_;
  scoped_ptr<PepperView> view_;
  pp::View plugin_view_;

  scoped_ptr<protocol::ConnectionToHost> host_connection_;
  scoped_ptr<ChromotingClient> client_;

  // Input pipeline components, in reverse order of distance from input source.
  protocol::MouseInputFilter mouse_input_filter_;
  protocol::InputEventTracker input_tracker_;
#if defined(OS_MACOSX)
  MacKeyEventProcessor mac_key_event_processor_;
#endif
  KeyEventMapper key_mapper_;
  PepperInputHandler input_handler_;

  // XmppProxy is a refcounted interface used to perform thread-switching and
  // detaching between objects whose lifetimes are controlled by pepper, and
  // jingle_glue objects. This is used when if we start a sandboxed jingle
  // connection.
  scoped_refptr<PepperXmppProxy> xmpp_proxy_;

  // PIN Fetcher.
  bool use_async_pin_dialog_;
  protocol::SecretFetchedCallback secret_fetched_callback_;

  base::WeakPtrFactory<ChromotingInstance> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ChromotingInstance);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_PLUGIN_CHROMOTING_INSTANCE_H_
