// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_JNI_CHROMOTING_JNI_RUNTIME_H_
#define REMOTING_CLIENT_JNI_CHROMOTING_JNI_RUNTIME_H_

#include <jni.h>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/at_exit.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/auto_thread.h"
#include "remoting/client/jni/chromoting_jni_instance.h"
#include "remoting/protocol/connection_to_host.h"

template<typename T> struct DefaultSingletonTraits;

namespace remoting {

bool RegisterJni(JNIEnv* env);

// Houses the global resources on which the Chromoting components run
// (e.g. message loops and task runners). Proxies outgoing JNI calls from its
// ChromotingJniInstance member to Java. All its methods should be invoked
// exclusively from the UI thread unless otherwise noted.
class ChromotingJniRuntime {
 public:
  // This class is instantiated at process initialization and persists until
  // we close. Its components are reused across |ChromotingJniInstance|s.
  static ChromotingJniRuntime* GetInstance();

  scoped_refptr<AutoThreadTaskRunner> ui_task_runner() {
    return ui_task_runner_;
  }

  scoped_refptr<AutoThreadTaskRunner> network_task_runner() {
    return network_task_runner_;
  }

  scoped_refptr<AutoThreadTaskRunner> display_task_runner() {
    return display_task_runner_;
  }

  scoped_refptr<net::URLRequestContextGetter> url_requester() {
    return url_requester_;
  }

  // Initiates a connection with the specified host. Only call when a host
  // connection is active (i.e. between a call to Connect() and the
  // corresponding call to Disconnect()). To skip the attempt at pair-based
  // authentication, leave |pairing_id| and |pairing_secret| as empty strings.
  void ConnectToHost(const char* username,
                     const char* auth_token,
                     const char* host_jid,
                     const char* host_id,
                     const char* host_pubkey,
                     const char* pairing_id,
                     const char* pairing_secret);

  // Terminates any ongoing connection attempt and cleans up by nullifying
  // |session_|. This is a no-op unless |session| is currently non-null.
  void DisconnectFromHost();

  // Returns the client for the currently-active session. Do not call if
  // |session| is null.
  scoped_refptr<ChromotingJniInstance> session() {
    DCHECK(session_);
    return session_;
  }

  // Notifies Java code of the current connection status. Call on UI thread.
  void OnConnectionState(protocol::ConnectionToHost::State state,
                         protocol::ErrorCode error);

  // Pops up a dialog box asking the user to enter a PIN. Call on UI thread.
  void DisplayAuthenticationPrompt(bool pairing_supported);

  // Saves new pairing credentials to permanent storage. Call on UI thread.
  void CommitPairingCredentials(const std::string& host,
                                const std::string& id,
                                const std::string& secret);

  // Pops up a third party login page to fetch token required for
  // authentication. Call on UI thread.
  void FetchThirdPartyToken(const GURL& token_url,
                            const std::string& client_id,
                            const std::string& scope);

  // Creates a new Bitmap object to store a video frame.
  base::android::ScopedJavaLocalRef<jobject> NewBitmap(
      webrtc::DesktopSize size);

  // Updates video frame bitmap. |bitmap| must be an instance of
  // android.graphics.Bitmap. Call on the display thread.
  void UpdateFrameBitmap(jobject bitmap);

  // Updates cursor shape. Call on display thread.
  void UpdateCursorShape(const protocol::CursorShapeInfo& cursor_shape);

  // Draws the latest image buffer onto the canvas. Call on the display thread.
  void RedrawCanvas();

 private:
  ChromotingJniRuntime();

  // Forces a DisconnectFromHost() in case there is any active or failed
  // connection, then proceeds to tear down the Chromium dependencies on which
  // all sessions depended. Because destruction only occurs at application exit
  // after all connections have terminated, it is safe to make unretained
  // cross-thread calls on the class.
  virtual ~ChromotingJniRuntime();

  // Detaches JVM from the current thread, then signals. Doesn't own |waiter|.
  void DetachFromVmAndSignal(base::WaitableEvent* waiter);

  // Used by the Chromium libraries to clean up the base and net libraries' JNI
  // bindings. It must persist for the lifetime of the singleton.
  scoped_ptr<base::AtExitManager> at_exit_manager_;

  // Chromium code's connection to the Java message loop.
  scoped_ptr<base::MessageLoopForUI> ui_loop_;

  // References to native threads.
  scoped_refptr<AutoThreadTaskRunner> ui_task_runner_;
  scoped_refptr<AutoThreadTaskRunner> network_task_runner_;
  scoped_refptr<AutoThreadTaskRunner> display_task_runner_;

  scoped_refptr<net::URLRequestContextGetter> url_requester_;

  // Contains all connection-specific state.
  scoped_refptr<ChromotingJniInstance> session_;

  friend struct DefaultSingletonTraits<ChromotingJniRuntime>;

  DISALLOW_COPY_AND_ASSIGN(ChromotingJniRuntime);
};

}  // namespace remoting

#endif
