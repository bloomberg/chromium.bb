// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/jni/jni_client.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/logging.h"
#include "jni/Client_jni.h"
#include "remoting/client/jni/chromoting_jni_instance.h"
#include "remoting/client/jni/chromoting_jni_runtime.h"
#include "remoting/client/jni/jni_touch_event_data.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;

namespace {

const int kBytesPerPixel = 4;
}

namespace remoting {

JniClient::JniClient(jobject java_client)
    : java_client_(java_client), weak_factory_(this) {}

JniClient::~JniClient() {
  DCHECK(runtime()->ui_task_runner()->BelongsToCurrentThread());
}

void JniClient::ConnectToHost(const std::string& username,
                              const std::string& auth_token,
                              const std::string& host_jid,
                              const std::string& host_id,
                              const std::string& host_pubkey,
                              const std::string& pairing_id,
                              const std::string& pairing_secret,
                              const std::string& capabilities,
                              const std::string& flags) {
  DCHECK(runtime()->ui_task_runner()->BelongsToCurrentThread());
  DCHECK(!session_);
  session_ = new ChromotingJniInstance(
      runtime(), this, username, auth_token, host_jid, host_id, host_pubkey,
      pairing_id, pairing_secret, capabilities, flags);
  session_->Connect();
}

void JniClient::DisconnectFromHost() {
  DCHECK(runtime()->ui_task_runner()->BelongsToCurrentThread());
  if (session_) {
    session_->Disconnect();
    session_ = nullptr;
  }
}

void JniClient::OnConnectionState(protocol::ConnectionToHost::State state,
                                  protocol::ErrorCode error) {
  DCHECK(runtime()->ui_task_runner()->BelongsToCurrentThread());

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_Client_onConnectionState(env, java_client_, state, error);
}

void JniClient::DisplayAuthenticationPrompt(bool pairing_supported) {
  DCHECK(runtime()->ui_task_runner()->BelongsToCurrentThread());

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_Client_displayAuthenticationPrompt(env, java_client_, pairing_supported);
}

void JniClient::CommitPairingCredentials(const std::string& host,
                                         const std::string& id,
                                         const std::string& secret) {
  DCHECK(runtime()->ui_task_runner()->BelongsToCurrentThread());

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_host = ConvertUTF8ToJavaString(env, host);
  ScopedJavaLocalRef<jstring> j_id = ConvertUTF8ToJavaString(env, id);
  ScopedJavaLocalRef<jstring> j_secret = ConvertUTF8ToJavaString(env, secret);

  Java_Client_commitPairingCredentials(env, java_client_, j_host.obj(),
                                       j_id.obj(), j_secret.obj());
}

void JniClient::FetchThirdPartyToken(const std::string& token_url,
                                     const std::string& client_id,
                                     const std::string& scope) {
  DCHECK(runtime()->ui_task_runner()->BelongsToCurrentThread());
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jstring> j_url = ConvertUTF8ToJavaString(env, token_url);
  ScopedJavaLocalRef<jstring> j_client_id =
      ConvertUTF8ToJavaString(env, client_id);
  ScopedJavaLocalRef<jstring> j_scope = ConvertUTF8ToJavaString(env, scope);

  Java_Client_fetchThirdPartyToken(env, java_client_, j_url.obj(),
                                   j_client_id.obj(), j_scope.obj());
}

void JniClient::SetCapabilities(const std::string& capabilities) {
  DCHECK(runtime()->ui_task_runner()->BelongsToCurrentThread());
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jstring> j_cap =
      ConvertUTF8ToJavaString(env, capabilities);

  Java_Client_setCapabilities(env, java_client_, j_cap.obj());
}

void JniClient::HandleExtensionMessage(const std::string& type,
                                       const std::string& message) {
  DCHECK(runtime()->ui_task_runner()->BelongsToCurrentThread());
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jstring> j_type = ConvertUTF8ToJavaString(env, type);
  ScopedJavaLocalRef<jstring> j_message = ConvertUTF8ToJavaString(env, message);

  Java_Client_handleExtensionMessage(env, java_client_, j_type.obj(),
                                     j_message.obj());
}

base::android::ScopedJavaLocalRef<jobject> JniClient::NewBitmap(int width,
                                                                int height) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_Client_newBitmap(env, width, height);
}

void JniClient::UpdateFrameBitmap(jobject bitmap) {
  DCHECK(runtime()->display_task_runner()->BelongsToCurrentThread());

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_Client_setVideoFrame(env, java_client_, bitmap);
}

void JniClient::UpdateCursorShape(
    const protocol::CursorShapeInfo& cursor_shape) {
  DCHECK(runtime()->display_task_runner()->BelongsToCurrentThread());

  // const_cast<> is safe as long as the Java updateCursorShape() method copies
  // the data out of the buffer without mutating it, and doesn't keep any
  // reference to the buffer afterwards. Unfortunately, there seems to be no way
  // to create a read-only ByteBuffer from a pointer-to-const.
  char* data = string_as_array(const_cast<std::string*>(&cursor_shape.data()));
  int cursor_total_bytes =
      cursor_shape.width() * cursor_shape.height() * kBytesPerPixel;

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> buffer(
      env, env->NewDirectByteBuffer(data, cursor_total_bytes));
  Java_Client_updateCursorShape(env, java_client_, cursor_shape.width(),
                                cursor_shape.height(), cursor_shape.hotspot_x(),
                                cursor_shape.hotspot_y(), buffer.obj());
}

void JniClient::RedrawCanvas() {
  DCHECK(runtime()->display_task_runner()->BelongsToCurrentThread());

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_Client_redrawGraphicsInternal(env, java_client_);
}

// static
bool JniClient::RegisterJni(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

void JniClient::Connect(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    const base::android::JavaParamRef<jstring>& username,
    const base::android::JavaParamRef<jstring>& authToken,
    const base::android::JavaParamRef<jstring>& hostJid,
    const base::android::JavaParamRef<jstring>& hostId,
    const base::android::JavaParamRef<jstring>& hostPubkey,
    const base::android::JavaParamRef<jstring>& pairId,
    const base::android::JavaParamRef<jstring>& pairSecret,
    const base::android::JavaParamRef<jstring>& capabilities,
    const base::android::JavaParamRef<jstring>& flags) {
  ConnectToHost(ConvertJavaStringToUTF8(env, username),
                ConvertJavaStringToUTF8(env, authToken),
                ConvertJavaStringToUTF8(env, hostJid),
                ConvertJavaStringToUTF8(env, hostId),
                ConvertJavaStringToUTF8(env, hostPubkey),
                ConvertJavaStringToUTF8(env, pairId),
                ConvertJavaStringToUTF8(env, pairSecret),
                ConvertJavaStringToUTF8(env, capabilities),
                ConvertJavaStringToUTF8(env, flags));
}

void JniClient::Disconnect(JNIEnv* env,
                           const base::android::JavaParamRef<jobject>& caller) {
  DisconnectFromHost();
}

void JniClient::AuthenticationResponse(
    JNIEnv* env,
    const JavaParamRef<jobject>& caller,
    const JavaParamRef<jstring>& pin,
    jboolean createPair,
    const JavaParamRef<jstring>& deviceName) {
  session_->ProvideSecret(ConvertJavaStringToUTF8(env, pin).c_str(), createPair,
                          ConvertJavaStringToUTF8(env, deviceName));
}

void JniClient::ScheduleRedraw(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller) {
  session_->RedrawDesktop();
}

void JniClient::SendMouseEvent(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    jint x,
    jint y,
    jint whichButton,
    jboolean buttonDown) {
  // Button must be within the bounds of the MouseEvent_MouseButton enum.
  DCHECK(whichButton >= 0 && whichButton < 5);

  session_->SendMouseEvent(
      x, y,
      static_cast<remoting::protocol::MouseEvent_MouseButton>(whichButton),
      buttonDown);
}

void JniClient::SendMouseWheelEvent(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    jint delta_x,
    jint delta_y) {
  session_->SendMouseWheelEvent(delta_x, delta_y);
}

jboolean JniClient::SendKeyEvent(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    jint scanCode,
    jint keyCode,
    jboolean keyDown) {
  return session_->SendKeyEvent(scanCode, keyCode, keyDown);
}

void JniClient::SendTextEvent(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    const JavaParamRef<jstring>& text) {
  session_->SendTextEvent(ConvertJavaStringToUTF8(env, text));
}

void JniClient::SendTouchEvent(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    jint eventType,
    const JavaParamRef<jobjectArray>& touchEventObjectArray) {
  protocol::TouchEvent touch_event;
  touch_event.set_event_type(
      static_cast<protocol::TouchEvent::TouchEventType>(eventType));

  // Iterate over the elements in the object array and transfer the data from
  // the java object to a native event object.
  jsize length = env->GetArrayLength(touchEventObjectArray);
  DCHECK_GE(length, 0);
  for (jsize i = 0; i < length; ++i) {
    protocol::TouchEventPoint* touch_point = touch_event.add_touch_points();

    ScopedJavaLocalRef<jobject> java_touch_event(
        env, env->GetObjectArrayElement(touchEventObjectArray, i));

    JniTouchEventData::CopyTouchPointData(env, java_touch_event, touch_point);
  }

  session_->SendTouchEvent(touch_event);
}

void JniClient::EnableVideoChannel(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    jboolean enable) {
  session_->EnableVideoChannel(enable);
}

void JniClient::OnThirdPartyTokenFetched(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    const JavaParamRef<jstring>& token,
    const JavaParamRef<jstring>& shared_secret) {
  runtime()->network_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ChromotingJniInstance::HandleOnThirdPartyTokenFetched,
                 session_, ConvertJavaStringToUTF8(env, token),
                 ConvertJavaStringToUTF8(env, shared_secret)));
}

void JniClient::SendExtensionMessage(
    JNIEnv* env,
    const base::android::JavaParamRef<jobject>& caller,
    const JavaParamRef<jstring>& type,
    const JavaParamRef<jstring>& data) {
  session_->SendClientMessage(ConvertJavaStringToUTF8(env, type),
                              ConvertJavaStringToUTF8(env, data));
}

void JniClient::Destroy(JNIEnv* env, const JavaParamRef<jobject>& caller) {
  // The session must be shut down first, since it depends on our other
  // components' still being alive.
  DisconnectFromHost();

  env->DeleteGlobalRef(java_client_);
  delete this;
}

base::WeakPtr<JniClient> JniClient::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

// static
ChromotingJniRuntime* JniClient::runtime() {
  return ChromotingJniRuntime::GetInstance();
}

static jlong Init(JNIEnv* env, const JavaParamRef<jobject>& caller) {
  jobject caller_ref = env->NewGlobalRef(caller);
  return reinterpret_cast<intptr_t>(new JniClient(caller_ref));
}

}  // namespace remoting
