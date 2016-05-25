// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_JNI_JNI_CLIENT_H_
#define REMOTING_CLIENT_JNI_JNI_CLIENT_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "remoting/protocol/connection_to_host.h"
#include "remoting/protocol/cursor_shape_stub.h"

namespace remoting {

class ChromotingJniRuntime;
class ChromotingJniInstance;

// Houses resources scoped to a session and exposes JNI interface to the
// Java client during a session. All its methods should be invoked exclusively
// from the UI thread unless otherwise noted.
class JniClient {
 public:
  JniClient(jobject java_client);

  // Initiates a connection with the specified host. Only call when a host
  // connection is active (i.e. between a call to Connect() and the
  // corresponding call to Disconnect()). To skip the attempt at pair-based
  // authentication, leave |pairing_id| and |pairing_secret| as empty strings.
  void ConnectToHost(const std::string& username,
                     const std::string& auth_token,
                     const std::string& host_jid,
                     const std::string& host_id,
                     const std::string& host_pubkey,
                     const std::string& pairing_id,
                     const std::string& pairing_secret,
                     const std::string& capabilities,
                     const std::string& flags);

  // Terminates any ongoing connection attempt and cleans up by nullifying
  // |session_|. This is a no-op unless |session| is currently non-null.
  void DisconnectFromHost();

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
  void FetchThirdPartyToken(const std::string& token_url,
                            const std::string& client_id,
                            const std::string& scope);

  // Pass on the set of negotiated capabilities to the client.
  void SetCapabilities(const std::string& capabilities);

  // Passes on the deconstructed ExtensionMessage to the client to handle
  // appropriately.
  void HandleExtensionMessage(const std::string& type,
                              const std::string& message);

  // Creates a new Bitmap object to store a video frame.
  base::android::ScopedJavaLocalRef<jobject> NewBitmap(int width, int height);

  // Updates video frame bitmap. |bitmap| must be an instance of
  // android.graphics.Bitmap. Call on the display thread.
  void UpdateFrameBitmap(jobject bitmap);

  // Updates cursor shape. Call on display thread.
  void UpdateCursorShape(const protocol::CursorShapeInfo& cursor_shape);

  // Draws the latest image buffer onto the canvas. Call on the display thread.
  void RedrawCanvas();

  // Register C++ methods exposed to Java using JNI.
  static bool RegisterJni(JNIEnv* env);

  // The following methods are exposed to Java via JNI.

  void Connect(JNIEnv* env,
               const base::android::JavaParamRef<jobject>& caller,
               const base::android::JavaParamRef<jstring>& username,
               const base::android::JavaParamRef<jstring>& authToken,
               const base::android::JavaParamRef<jstring>& hostJid,
               const base::android::JavaParamRef<jstring>& hostId,
               const base::android::JavaParamRef<jstring>& hostPubkey,
               const base::android::JavaParamRef<jstring>& pairId,
               const base::android::JavaParamRef<jstring>& pairSecret,
               const base::android::JavaParamRef<jstring>& capabilities,
               const base::android::JavaParamRef<jstring>& flags);

  void Disconnect(JNIEnv* env,
                  const base::android::JavaParamRef<jobject>& caller);

  void AuthenticationResponse(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      const base::android::JavaParamRef<jstring>& pin,
      jboolean createPair,
      const base::android::JavaParamRef<jstring>& deviceName);

  void ScheduleRedraw(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& caller);

  void SendMouseEvent(JNIEnv* env,
                      const base::android::JavaParamRef<jobject>& caller,
                      jint x,
                      jint y,
                      jint whichButton,
                      jboolean buttonDown);

  void SendMouseWheelEvent(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& caller,
                           jint delta_x,
                           jint delta_y);

  jboolean SendKeyEvent(JNIEnv* env,
                        const base::android::JavaParamRef<jobject>& caller,
                        jint scanCode,
                        jint keyCode,
                        jboolean keyDown);

  void SendTextEvent(JNIEnv* env,
                     const base::android::JavaParamRef<jobject>& caller,
                     const base::android::JavaParamRef<jstring>& text);

  void SendTouchEvent(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      jint eventType,
      const base::android::JavaParamRef<jobjectArray>& touchEventObjectArray);

  void EnableVideoChannel(JNIEnv* env,
                          const base::android::JavaParamRef<jobject>& caller,
                          jboolean enable);

  void OnThirdPartyTokenFetched(
      JNIEnv* env,
      const base::android::JavaParamRef<jobject>& caller,
      const base::android::JavaParamRef<jstring>& token,
      const base::android::JavaParamRef<jstring>& shared_secret);

  void SendExtensionMessage(JNIEnv* env,
                            const base::android::JavaParamRef<jobject>& caller,
                            const base::android::JavaParamRef<jstring>& type,
                            const base::android::JavaParamRef<jstring>& data);

  // Destroys this object. Called on UI thread. This function will delete the
  // |java_client_| reference and delete |this|.
  void Destroy(JNIEnv* env, const base::android::JavaParamRef<jobject>& caller);

  // Get the weak pointer of the object. Should be used on the UI thread.
  // TODO(yuweih): Instead of exposing a weak pointer in the public API,
  //               consider handling task posting inside the client.
  base::WeakPtr<JniClient> GetWeakPtr();

 private:
  // Please use Destroy() to delete this object.
  ~JniClient();

  // Helper function for getting the runtime instance.
  static ChromotingJniRuntime* runtime();

  // Reference to the Java client object.
  jobject java_client_;

  scoped_refptr<ChromotingJniInstance> session_;

  // Holds pointer for the UI thread.
  base::WeakPtrFactory<JniClient> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(JniClient);
};

}  // namespace remoting

#endif  // REMOTING_CLIENT_JNI_JNI_CLIENT_H_
