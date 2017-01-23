// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_JNI_CHROMOTING_JNI_RUNTIME_H_
#define REMOTING_CLIENT_JNI_CHROMOTING_JNI_RUNTIME_H_

#include <jni.h>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/macros.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/auto_thread.h"
#include "remoting/base/telemetry_log_writer.h"
#include "remoting/client/chromoting_client_runtime.h"
#include "remoting/client/jni/chromoting_jni_instance.h"
#include "remoting/protocol/connection_to_host.h"

namespace base {
template<typename T> struct DefaultSingletonTraits;
}

namespace remoting {

bool RegisterChromotingJniRuntime(JNIEnv* env);

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
    return runtime_->ui_task_runner();
  }

  scoped_refptr<AutoThreadTaskRunner> network_task_runner() {
    return runtime_->network_task_runner();
  }

  scoped_refptr<AutoThreadTaskRunner> display_task_runner() {
    return runtime_->display_task_runner();
  }

  scoped_refptr<net::URLRequestContextGetter> url_requester() {
    return runtime_->url_requester();
  }

  // Returns the log writer that can be used by ClientTelemetryLogger to send
  // out logs.
  // Method must be called and returned object must be used on the network
  // thread.
  TelemetryLogWriter* GetLogWriter();

  // Fetch OAuth token for the telemetry logger. Call on UI thread.
  void FetchAuthToken();

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

  // Starts the logger on the network thread.
  void StartLoggerOnNetworkThread();

  // Chromium code's connection to the app message loop. Once created the
  // MessageLoop will live for the life of the program.
  std::unique_ptr<base::MessageLoopForUI> ui_loop_;

  // Contains threads.
  //
  std::unique_ptr<ChromotingClientRuntime> runtime_;

  // For logging session stage changes and stats.
  std::unique_ptr<TelemetryLogWriter> log_writer_;

  friend struct base::DefaultSingletonTraits<ChromotingJniRuntime>;

  DISALLOW_COPY_AND_ASSIGN(ChromotingJniRuntime);
};

}  // namespace remoting

#endif
