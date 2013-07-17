// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/jni/chromoting_jni.h"

#include "base/android/base_jni_registrar.h"
#include "base/android/jni_android.h"
#include "base/memory/singleton.h"
#include "net/android/net_jni_registrar.h"
#include "remoting/base/url_request_context.h"
#include "remoting/client/jni/chromoting_jni_instance.h"

namespace {
// Class and package name of the Java class supporting the methods we call.
const char* const JAVA_CLASS = "org/chromium/chromoting/jni/JNIInterface";
}  // namespace

namespace remoting {

// static
ChromotingJni* ChromotingJni::GetInstance() {
  return Singleton<ChromotingJni>::get();
}

ChromotingJni::ChromotingJni() {
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

  url_requester_ = new URLRequestContextGetter(ui_task_runner_,
                                               network_task_runner_);

  class_ = static_cast<jclass>(env->NewGlobalRef(env->FindClass(JAVA_CLASS)));
}

ChromotingJni::~ChromotingJni() {
  // The singleton should only ever be destroyed on the main thread.
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  // The session must be shut down first, since it depends on our other
  // components' still being alive.
  DisconnectFromHost();

  JNIEnv* env = base::android::AttachCurrentThread();
  env->DeleteGlobalRef(class_);
  // TODO(solb): crbug.com/259594 Detach all threads from JVM here.
}

void ChromotingJni::ConnectToHost(const char* username,
                                  const char* auth_token,
                                  const char* host_jid,
                                  const char* host_id,
                                  const char* host_pubkey) {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  DCHECK(!session_);
  session_ = new ChromotingJniInstance(username,
                                       auth_token,
                                       host_jid,
                                       host_id,
                                       host_pubkey);
}

void ChromotingJni::DisconnectFromHost() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());
  if (session_) {
    session_->Cleanup();
    session_ = NULL;
  }
}

void ChromotingJni::ReportConnectionStatus(
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

void ChromotingJni::DisplayAuthenticationPrompt() {
  DCHECK(ui_task_runner_->BelongsToCurrentThread());

  JNIEnv* env = base::android::AttachCurrentThread();
  env->CallStaticVoidMethod(
      class_,
      env->GetStaticMethodID(class_, "displayAuthenticationPrompt", "()V"));
}

}  // namespace remoting
