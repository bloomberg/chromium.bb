// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/jni/chromoting_jni_runtime.h"

#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/singleton.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "media/base/yuv_convert.h"
#include "net/android/net_jni_registrar.h"
#include "remoting/base/url_request_context.h"
#include "third_party/webrtc/modules/desktop_capture/desktop_frame.h"

namespace {

// Class and package name of the Java class supporting the methods we call.
const char* const kJavaClass = "org/chromium/chromoting/jni/JniInterface";

const int kBytesPerPixel = 4;

}  // namespace

namespace remoting {

// static
ChromotingJniRuntime* ChromotingJniRuntime::GetInstance() {
  return Singleton<ChromotingJniRuntime>::get();
}

ChromotingJniRuntime::ChromotingJniRuntime() {
  // Obtain a reference to the Java environment. (Future calls to this function
  // made from the same thread return the same stored reference instead of
  // repeating the work of attaching to the JVM.)
  JNIEnv* env = base::android::AttachCurrentThread();

  // The base and networks stacks must be registered with JNI in order to work
  // on Android. An AtExitManager cleans this up at process exit.
  at_exit_manager_.reset(new base::AtExitManager());
  base::android::RegisterJni(env);
  net::android::RegisterJni(env);

  // On Android, the UI thread is managed by Java, so we need to attach and
  // start a special type of message loop to allow Chromium code to run tasks.
  LOG(INFO) << "Starting main message loop";
  ui_loop_.reset(new base::MessageLoopForUI());
  ui_loop_->Start();

  LOG(INFO) << "Spawning additional threads";
  // TODO(solb) Stop pretending to control the managed UI thread's lifetime.
  ui_task_runner_ = new AutoThreadTaskRunner(ui_loop_->message_loop_proxy(),
                                             base::MessageLoop::QuitClosure());
  network_task_runner_ = AutoThread::CreateWithType("native_net",
                                                    ui_task_runner_,
                                                    base::MessageLoop::TYPE_IO);
  display_task_runner_ = AutoThread::Create("native_disp",
                                            ui_task_runner_);

  url_requester_ = new URLRequestContextGetter(network_task_runner_);

  // Allows later decoding of video frames.
  media::InitializeCPUSpecificYUVConversions();

  class_ = static_cast<jclass>(env->NewGlobalRef(env->FindClass(kJavaClass)));
}

ChromotingJniRuntime::~ChromotingJniRuntime() {
  // The singleton should only ever be destroyed on the main thread.
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // The session must be shut down first, since it depends on our other
  // components' still being alive.
  DisconnectFromHost();

  JNIEnv* env = base::android::AttachCurrentThread();
  env->DeleteGlobalRef(class_);

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
  base::android::DetachFromVM();
}

void ChromotingJniRuntime::ConnectToHost(const char* username,
                                  const char* auth_token,
                                  const char* host_jid,
                                  const char* host_id,
                                  const char* host_pubkey,
                                  const char* pairing_id,
                                  const char* pairing_secret) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(!session_);
  session_ = new ChromotingJniInstance(this,
                                       username,
                                       auth_token,
                                       host_jid,
                                       host_id,
                                       host_pubkey,
                                       pairing_id,
                                       pairing_secret);
}

void ChromotingJniRuntime::DisconnectFromHost() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  if (session_) {
    session_->Cleanup();
    session_ = NULL;
  }
}

void ChromotingJniRuntime::ReportConnectionStatus(
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  JNIEnv* env = base::android::AttachCurrentThread();
  env->CallStaticVoidMethod(
    class_,
    env->GetStaticMethodID(class_, "reportConnectionStatus", "(II)V"),
    state,
    error);
}

void ChromotingJniRuntime::DisplayAuthenticationPrompt(bool pairing_supported) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  JNIEnv* env = base::android::AttachCurrentThread();
  env->CallStaticVoidMethod(
      class_,
      env->GetStaticMethodID(class_, "displayAuthenticationPrompt", "(Z)V"),
      pairing_supported);
}

void ChromotingJniRuntime::CommitPairingCredentials(const std::string& host,
                                                    const std::string& id,
                                                    const std::string& secret) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  JNIEnv* env = base::android::AttachCurrentThread();
  jstring host_jstr = env->NewStringUTF(host.c_str());
  jbyteArray id_arr = env->NewByteArray(id.size());
  env->SetByteArrayRegion(id_arr, 0, id.size(),
      reinterpret_cast<const jbyte*>(id.c_str()));
  jbyteArray secret_arr = env->NewByteArray(secret.size());
  env->SetByteArrayRegion(secret_arr, 0, secret.size(),
      reinterpret_cast<const jbyte*>(secret.c_str()));

  env->CallStaticVoidMethod(
      class_,
      env->GetStaticMethodID(
          class_,
          "commitPairingCredentials",
          "(Ljava/lang/String;[B[B)V"),
      host_jstr,
      id_arr,
      secret_arr);

  // Because we passed them as arguments, their corresponding Java objects were
  // GCd as soon as the managed method returned, so we mustn't release it here.
}

base::android::ScopedJavaLocalRef<jobject> ChromotingJniRuntime::NewBitmap(
    webrtc::DesktopSize size) {
  JNIEnv* env = base::android::AttachCurrentThread();

  jobject bitmap = env->CallStaticObjectMethod(
      class_,
      env->GetStaticMethodID(
          class_,
          "newBitmap",
          "(II)Landroid/graphics/Bitmap;"),
      size.width(),
      size.height());
  return base::android::ScopedJavaLocalRef<jobject>(env, bitmap);
}

void ChromotingJniRuntime::UpdateFrameBitmap(jobject bitmap) {
  DCHECK(display_task_runner_->BelongsToCurrentThread());

  JNIEnv* env = base::android::AttachCurrentThread();

  env->CallStaticVoidMethod(
      class_,
      env->GetStaticMethodID(
          class_,
          "setVideoFrame",
          "(Landroid/graphics/Bitmap;)V"),
      bitmap);
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
  env->CallStaticVoidMethod(
      class_,
      env->GetStaticMethodID(
          class_,
          "updateCursorShape",
          "(IIIILjava/nio/ByteBuffer;)V"),
      cursor_shape.width(),
      cursor_shape.height(),
      cursor_shape.hotspot_x(),
      cursor_shape.hotspot_y(),
      buffer.obj());
}

void ChromotingJniRuntime::RedrawCanvas() {
  DCHECK(display_task_runner_->BelongsToCurrentThread());

  JNIEnv* env = base::android::AttachCurrentThread();
  env->CallStaticVoidMethod(
      class_,
      env->GetStaticMethodID(class_, "redrawGraphicsInternal", "()V"));
}

void ChromotingJniRuntime::DetachFromVmAndSignal(base::WaitableEvent* waiter) {
  base::android::DetachFromVM();
  waiter->Signal();
}

}  // namespace remoting
