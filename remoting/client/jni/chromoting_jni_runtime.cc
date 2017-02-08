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
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/task_scheduler/task_scheduler.h"
#include "jni/JniInterface_jni.h"
#include "remoting/base/chromium_url_request.h"
#include "remoting/base/url_request_context_getter.h"
#include "remoting/client/jni/jni_touch_event_data.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaParamRef;
using base::android::ToJavaByteArray;

namespace {

const char kTelemetryBaseUrl[] = "https://remoting-pa.googleapis.com/v1/events";

}  // namespace

namespace remoting {

bool RegisterChromotingJniRuntime(JNIEnv* env) {
  return remoting::RegisterNativesImpl(env);
}

// Implementation of stubs defined in JniInterface_jni.h. These are the entry
// points for JNI calls from Java into C++.

static void LoadNative(JNIEnv* env, const JavaParamRef<jclass>& clazz) {
  base::CommandLine::Init(0, nullptr);

  // TODO(sergeyu): Consider adding separate pools for different task classes.
  const int kMaxBackgroundThreads = 5;
  base::TaskScheduler::CreateAndSetSimpleTaskScheduler(kMaxBackgroundThreads);

  // Create the singleton now so that the Chromoting threads will be set up.
  remoting::ChromotingJniRuntime::GetInstance();
}

static void HandleAuthTokenOnNetworkThread(const std::string& token) {
  ChromotingJniRuntime* runtime = remoting::ChromotingJniRuntime::GetInstance();
  DCHECK(runtime->network_task_runner()->BelongsToCurrentThread());
  runtime->GetLogWriter()->SetAuthToken(token);
}

static void OnAuthTokenFetched(JNIEnv* env,
                         const JavaParamRef<jclass>& clazz,
                         const JavaParamRef<jstring>& token) {
  ChromotingJniRuntime* runtime = remoting::ChromotingJniRuntime::GetInstance();
  runtime->network_task_runner()->PostTask(
      FROM_HERE, base::Bind(&HandleAuthTokenOnNetworkThread,
                            ConvertJavaStringToUTF8(env, token)));
}

// ChromotingJniRuntime implementation.

// static
ChromotingJniRuntime* ChromotingJniRuntime::GetInstance() {
  return base::Singleton<ChromotingJniRuntime>::get();
}

ChromotingJniRuntime::ChromotingJniRuntime() {
  // Grab or create the threads.
  // TODO(nicholss): We could runtime this as a constructor argument when jni
  // runtime is not no longer a singleton.

  if (!base::MessageLoop::current()) {
    VLOG(1) << "Starting main message loop";
    // On Android, the UI thread is managed by Java, so we need to attach and
    // start a special type of message loop to allow Chromium code to run tasks.
    ui_loop_.reset(new base::MessageLoopForUI());
    ui_loop_->Start();
  } else {
    VLOG(1) << "Using existing main message loop";
    ui_loop_.reset(base::MessageLoopForUI::current());
  }

  // Pass the main ui loop already attached to be used for creating threads.
  runtime_ = ChromotingClientRuntime::Create(ui_loop_.get());
  network_task_runner()->PostTask(
      FROM_HERE, base::Bind(&ChromotingJniRuntime::StartLoggerOnNetworkThread,
                            base::Unretained(this)));
}

ChromotingJniRuntime::~ChromotingJniRuntime() {
  // The singleton should only ever be destroyed on the main thread.
  DCHECK(ui_task_runner()->BelongsToCurrentThread());

  base::WaitableEvent done_event(
      base::WaitableEvent::ResetPolicy::AUTOMATIC,
      base::WaitableEvent::InitialState::NOT_SIGNALED);
  network_task_runner()->PostTask(
      FROM_HERE, base::Bind(&ChromotingJniRuntime::DetachFromVmAndSignal,
                            base::Unretained(this), &done_event));
  done_event.Wait();
  display_task_runner()->PostTask(
      FROM_HERE, base::Bind(&ChromotingJniRuntime::DetachFromVmAndSignal,
                            base::Unretained(this), &done_event));
  done_event.Wait();

  // Block until tasks blocking shutdown have completed their execution.
  base::TaskScheduler::GetInstance()->Shutdown();

  base::android::LibraryLoaderExitHook();
  base::android::DetachFromVM();
}

TelemetryLogWriter* ChromotingJniRuntime::GetLogWriter() {
  DCHECK(runtime_->network_task_runner()->BelongsToCurrentThread());
  return log_writer_.get();
}

void ChromotingJniRuntime::FetchAuthToken() {
  if (!ui_task_runner()->BelongsToCurrentThread()) {
    ui_task_runner()->PostTask(
        FROM_HERE, base::Bind(&ChromotingJniRuntime::FetchAuthToken,
                              base::Unretained(this)));
    return;
  }
  JNIEnv* env = base::android::AttachCurrentThread();

  Java_JniInterface_fetchAuthToken(env);
}

void ChromotingJniRuntime::DetachFromVmAndSignal(base::WaitableEvent* waiter) {
  base::android::DetachFromVM();
  waiter->Signal();
}

void ChromotingJniRuntime::StartLoggerOnNetworkThread() {
  DCHECK(network_task_runner()->BelongsToCurrentThread());
  log_writer_.reset(new TelemetryLogWriter(
      kTelemetryBaseUrl,
      base::MakeUnique<ChromiumUrlRequestFactory>(runtime_->url_requester())));
  log_writer_->SetAuthClosure(
      base::Bind(&ChromotingJniRuntime::FetchAuthToken,
                 base::Unretained(this)));
}

}  // namespace remoting
