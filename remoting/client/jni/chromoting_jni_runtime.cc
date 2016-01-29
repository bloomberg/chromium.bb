// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/jni/chromoting_jni_runtime.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/android/library_loader/library_loader_hooks.h"
#include "base/android/scoped_java_ref.h"
#include "base/command_line.h"
#include "base/memory/singleton.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "google_apis/google_api_keys.h"
#include "jni/JniInterface_jni.h"
#include "remoting/base/url_request_context_getter.h"
#include "remoting/client/jni/jni_touch_event_data.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::ToJavaByteArray;

namespace {

const int kBytesPerPixel = 4;

}  // namespace

namespace remoting {

bool RegisterChromotingJniRuntime(JNIEnv* env) {
  return remoting::RegisterNativesImpl(env);
}

// Implementation of stubs defined in JniInterface_jni.h. These are the entry
// points for JNI calls from Java into C++.

static void LoadNative(JNIEnv* env, const JavaParamRef<jclass>& clazz) {
  // The google_apis functions check the command-line arguments to make sure no
  // runtime API keys have been specified by the environment. Unfortunately, we
  // neither launch Chromium nor have a command line, so we need to prevent
  // them from DCHECKing out when they go looking.
  base::CommandLine::Init(0, nullptr);

  // Create the singleton now so that the Chromoting threads will be set up.
  remoting::ChromotingJniRuntime::GetInstance();
}

static ScopedJavaLocalRef<jstring> GetApiKey(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz) {
  return ConvertUTF8ToJavaString(env, google_apis::GetAPIKey().c_str());
}

static ScopedJavaLocalRef<jstring> GetClientId(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz) {
  return ConvertUTF8ToJavaString(
      env,
      google_apis::GetOAuth2ClientID(google_apis::CLIENT_REMOTING).c_str());
}

static ScopedJavaLocalRef<jstring> GetClientSecret(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz) {
  return ConvertUTF8ToJavaString(
      env,
      google_apis::GetOAuth2ClientSecret(google_apis::CLIENT_REMOTING).c_str());
}

static void Connect(JNIEnv* env,
                    const JavaParamRef<jclass>& clazz,
                    const JavaParamRef<jstring>& username,
                    const JavaParamRef<jstring>& authToken,
                    const JavaParamRef<jstring>& hostJid,
                    const JavaParamRef<jstring>& hostId,
                    const JavaParamRef<jstring>& hostPubkey,
                    const JavaParamRef<jstring>& pairId,
                    const JavaParamRef<jstring>& pairSecret,
                    const JavaParamRef<jstring>& capabilities,
                    const JavaParamRef<jstring>& flags) {
  remoting::ChromotingJniRuntime::GetInstance()->ConnectToHost(
      ConvertJavaStringToUTF8(env, username),
      ConvertJavaStringToUTF8(env, authToken),
      ConvertJavaStringToUTF8(env, hostJid),
      ConvertJavaStringToUTF8(env, hostId),
      ConvertJavaStringToUTF8(env, hostPubkey),
      ConvertJavaStringToUTF8(env, pairId),
      ConvertJavaStringToUTF8(env, pairSecret),
      ConvertJavaStringToUTF8(env, capabilities),
      ConvertJavaStringToUTF8(env, flags));
}

static void Disconnect(JNIEnv* env, const JavaParamRef<jclass>& clazz) {
  remoting::ChromotingJniRuntime::GetInstance()->DisconnectFromHost();
}

static void AuthenticationResponse(JNIEnv* env,
                                   const JavaParamRef<jclass>& clazz,
                                   const JavaParamRef<jstring>& pin,
                                   jboolean createPair,
                                   const JavaParamRef<jstring>& deviceName) {
  remoting::ChromotingJniRuntime::GetInstance()->session()->ProvideSecret(
      ConvertJavaStringToUTF8(env, pin).c_str(), createPair,
      ConvertJavaStringToUTF8(env, deviceName));
}

static void ScheduleRedraw(JNIEnv* env, const JavaParamRef<jclass>& clazz) {
  remoting::ChromotingJniRuntime::GetInstance()->session()->RedrawDesktop();
}

static void SendMouseEvent(JNIEnv* env,
                           const JavaParamRef<jclass>& clazz,
                           jint x,
                           jint y,
                           jint whichButton,
                           jboolean buttonDown) {
  // Button must be within the bounds of the MouseEvent_MouseButton enum.
  DCHECK(whichButton >= 0 && whichButton < 5);

  remoting::ChromotingJniRuntime::GetInstance()->session()->SendMouseEvent(
      x, y,
      static_cast<remoting::protocol::MouseEvent_MouseButton>(whichButton),
      buttonDown);
}

static void SendMouseWheelEvent(JNIEnv* env,
                                const JavaParamRef<jclass>& clazz,
                                jint delta_x,
                                jint delta_y) {
  remoting::ChromotingJniRuntime::GetInstance()->session()->SendMouseWheelEvent(
      delta_x, delta_y);
}

static jboolean SendKeyEvent(JNIEnv* env,
                             const JavaParamRef<jclass>& clazz,
                             jint scanCode,
                             jint keyCode,
                             jboolean keyDown) {
  return remoting::ChromotingJniRuntime::GetInstance()->session()->SendKeyEvent(
      scanCode, keyCode, keyDown);
}

static void SendTextEvent(JNIEnv* env,
                          const JavaParamRef<jclass>& clazz,
                          const JavaParamRef<jstring>& text) {
  remoting::ChromotingJniRuntime::GetInstance()->session()->SendTextEvent(
      ConvertJavaStringToUTF8(env, text));
}

static void SendTouchEvent(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
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

  remoting::ChromotingJniRuntime::GetInstance()->session()->SendTouchEvent(
      touch_event);
}

static void EnableVideoChannel(JNIEnv* env,
                               const JavaParamRef<jclass>& clazz,
                               jboolean enable) {
  remoting::ChromotingJniRuntime::GetInstance()->session()->EnableVideoChannel(
      enable);
}

static void OnThirdPartyTokenFetched(
    JNIEnv* env,
    const JavaParamRef<jclass>& clazz,
    const JavaParamRef<jstring>& token,
    const JavaParamRef<jstring>& shared_secret) {
  ChromotingJniRuntime* runtime = remoting::ChromotingJniRuntime::GetInstance();
  runtime->network_task_runner()->PostTask(FROM_HERE, base::Bind(
      &ChromotingJniInstance::HandleOnThirdPartyTokenFetched,
      runtime->session(),
      ConvertJavaStringToUTF8(env, token),
      ConvertJavaStringToUTF8(env, shared_secret)));
}

static void SendExtensionMessage(JNIEnv* env,
                                 const JavaParamRef<jclass>& clazz,
                                 const JavaParamRef<jstring>& type,
                                 const JavaParamRef<jstring>& data) {
  remoting::ChromotingJniRuntime::GetInstance()->session()->SendClientMessage(
      ConvertJavaStringToUTF8(env, type),
      ConvertJavaStringToUTF8(env, data));
}

// ChromotingJniRuntime implementation.

// static
ChromotingJniRuntime* ChromotingJniRuntime::GetInstance() {
  return base::Singleton<ChromotingJniRuntime>::get();
}

ChromotingJniRuntime::ChromotingJniRuntime() {
  // On Android, the UI thread is managed by Java, so we need to attach and
  // start a special type of message loop to allow Chromium code to run tasks.
  ui_loop_.reset(new base::MessageLoopForUI());
  ui_loop_->Start();

  // TODO(solb) Stop pretending to control the managed UI thread's lifetime.
  ui_task_runner_ = new AutoThreadTaskRunner(
      ui_loop_->task_runner(), base::MessageLoop::QuitWhenIdleClosure());
  network_task_runner_ = AutoThread::CreateWithType("native_net",
                                                    ui_task_runner_,
                                                    base::MessageLoop::TYPE_IO);
  display_task_runner_ = AutoThread::Create("native_disp",
                                            ui_task_runner_);

  url_requester_ =
      new URLRequestContextGetter(network_task_runner_, network_task_runner_);
}

ChromotingJniRuntime::~ChromotingJniRuntime() {
  // The singleton should only ever be destroyed on the main thread.
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // The session must be shut down first, since it depends on our other
  // components' still being alive.
  DisconnectFromHost();

  base::WaitableEvent done_event(false, false);
  network_task_runner_->PostTask(FROM_HERE, base::Bind(
      &ChromotingJniRuntime::DetachFromVmAndSignal,
      base::Unretained(this),
      &done_event));
  done_event.Wait();
  display_task_runner_->PostTask(FROM_HERE, base::Bind(
      &ChromotingJniRuntime::DetachFromVmAndSignal,
      base::Unretained(this),
      &done_event));
  done_event.Wait();
  base::android::LibraryLoaderExitHook();
  base::android::DetachFromVM();
}

void ChromotingJniRuntime::ConnectToHost(const std::string& username,
                                         const std::string& auth_token,
                                         const std::string& host_jid,
                                         const std::string& host_id,
                                         const std::string& host_pubkey,
                                         const std::string& pairing_id,
                                         const std::string& pairing_secret,
                                         const std::string& capabilities,
                                         const std::string& flags) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(!session_.get());
  session_ = new ChromotingJniInstance(this, username, auth_token, host_jid,
                                       host_id, host_pubkey, pairing_id,
                                       pairing_secret, capabilities, flags);
}

void ChromotingJniRuntime::DisconnectFromHost() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  if (session_.get()) {
    session_->Disconnect();
    session_ = nullptr;
  }
}

void ChromotingJniRuntime::OnConnectionState(
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_JniInterface_onConnectionState(env, state, error);
}

void ChromotingJniRuntime::DisplayAuthenticationPrompt(bool pairing_supported) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_JniInterface_displayAuthenticationPrompt(env, pairing_supported);
}

void ChromotingJniRuntime::CommitPairingCredentials(const std::string& host,
                                                    const std::string& id,
                                                    const std::string& secret) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jstring> j_host = ConvertUTF8ToJavaString(env, host);
  ScopedJavaLocalRef<jstring> j_id = ConvertUTF8ToJavaString(env, id);
  ScopedJavaLocalRef<jstring> j_secret = ConvertUTF8ToJavaString(env,secret);

  Java_JniInterface_commitPairingCredentials(
      env, j_host.obj(), j_id.obj(), j_secret.obj());
}

void ChromotingJniRuntime::FetchThirdPartyToken(const GURL& token_url,
                                                const std::string& client_id,
                                                const std::string& scope) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jstring> j_url =
      ConvertUTF8ToJavaString(env, token_url.spec());
  ScopedJavaLocalRef<jstring> j_client_id =
      ConvertUTF8ToJavaString(env, client_id);
  ScopedJavaLocalRef<jstring> j_scope = ConvertUTF8ToJavaString(env, scope);

  Java_JniInterface_fetchThirdPartyToken(
      env, j_url.obj(), j_client_id.obj(), j_scope.obj());
}

void ChromotingJniRuntime::SetCapabilities(const std::string& capabilities) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jstring> j_cap =
      ConvertUTF8ToJavaString(env, capabilities);

  Java_JniInterface_setCapabilities(env, j_cap.obj());
}

void ChromotingJniRuntime::HandleExtensionMessage(const std::string& type,
                                                  const std::string& message) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  JNIEnv* env = base::android::AttachCurrentThread();

  ScopedJavaLocalRef<jstring> j_type = ConvertUTF8ToJavaString(env, type);
  ScopedJavaLocalRef<jstring> j_message = ConvertUTF8ToJavaString(env, message);

  Java_JniInterface_handleExtensionMessage(env, j_type.obj(), j_message.obj());
}

base::android::ScopedJavaLocalRef<jobject> ChromotingJniRuntime::NewBitmap(
    int width, int height) {
  JNIEnv* env = base::android::AttachCurrentThread();
  return Java_JniInterface_newBitmap(env, width, height);
}

void ChromotingJniRuntime::UpdateFrameBitmap(jobject bitmap) {
  DCHECK(display_task_runner_->BelongsToCurrentThread());

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_JniInterface_setVideoFrame(env, bitmap);
}

void ChromotingJniRuntime::UpdateCursorShape(
    const protocol::CursorShapeInfo& cursor_shape) {
  DCHECK(display_task_runner_->BelongsToCurrentThread());

  // const_cast<> is safe as long as the Java updateCursorShape() method copies
  // the data out of the buffer without mutating it, and doesn't keep any
  // reference to the buffer afterwards. Unfortunately, there seems to be no way
  // to create a read-only ByteBuffer from a pointer-to-const.
  char* data = string_as_array(const_cast<std::string*>(&cursor_shape.data()));
  int cursor_total_bytes =
      cursor_shape.width() * cursor_shape.height() * kBytesPerPixel;

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> buffer(env,
      env->NewDirectByteBuffer(data, cursor_total_bytes));
  Java_JniInterface_updateCursorShape(env,
                                      cursor_shape.width(),
                                      cursor_shape.height(),
                                      cursor_shape.hotspot_x(),
                                      cursor_shape.hotspot_y(),
                                      buffer.obj());
}

void ChromotingJniRuntime::RedrawCanvas() {
  DCHECK(display_task_runner_->BelongsToCurrentThread());

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_JniInterface_redrawGraphicsInternal(env);
}

void ChromotingJniRuntime::DetachFromVmAndSignal(base::WaitableEvent* waiter) {
  base::android::DetachFromVM();
  waiter->Signal();
}
}  // namespace remoting
