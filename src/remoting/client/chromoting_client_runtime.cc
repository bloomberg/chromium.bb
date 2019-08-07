// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/chromoting_client_runtime.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_current.h"
#include "base/task/thread_pool/thread_pool.h"
#include "build/build_config.h"
#include "mojo/core/embedder/embedder.h"
#include "remoting/base/chromium_url_request.h"
#include "remoting/base/telemetry_log_writer.h"
#include "remoting/base/url_request_context_getter.h"
#include "remoting/client/oauth_token_getter_proxy.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/transitional_url_loader_factory_owner.h"

namespace {

const char kTelemetryBaseUrl[] = "https://remoting-pa.googleapis.com/v1/events";

}  // namespace

namespace remoting {

// static
ChromotingClientRuntime* ChromotingClientRuntime::GetInstance() {
  return base::Singleton<ChromotingClientRuntime>::get();
}

ChromotingClientRuntime::ChromotingClientRuntime() {
  base::ThreadPool::CreateAndStartWithDefaultParams("Remoting");

  DCHECK(!base::MessageLoopCurrent::Get());

  VLOG(1) << "Starting main message loop";
  ui_loop_.reset(new base::MessageLoopForUI());
#if defined(OS_IOS)
  // TODO(ranj): Attach on BindToCurrentThread().
  ui_loop_->Attach();
#endif

#if defined(DEBUG)
  net::URLFetcher::SetIgnoreCertificateRequests(true);
#endif  // DEBUG

  // |ui_loop_| runs on the main thread, so |ui_task_runner_| will run on the
  // main thread.  We can not kill the main thread when the message loop becomes
  // idle so the callback function does nothing (as opposed to the typical
  // base::MessageLoop::QuitClosure())
  ui_task_runner_ =
      new AutoThreadTaskRunner(ui_loop_->task_runner(), base::DoNothing());
  audio_task_runner_ = AutoThread::Create("native_audio", ui_task_runner_);
  display_task_runner_ = AutoThread::Create("native_disp", ui_task_runner_);
  network_task_runner_ = AutoThread::CreateWithType(
      "native_net", ui_task_runner_, base::MessageLoop::TYPE_IO);

  mojo::core::Init();
}

ChromotingClientRuntime::~ChromotingClientRuntime() {
  if (delegate_) {
    delegate_->RuntimeWillShutdown();
  } else {
    DLOG(ERROR) << "ClientRuntime Delegate is null.";
  }

  // Block until tasks blocking shutdown have completed their execution.
  base::ThreadPool::GetInstance()->Shutdown();

  if (delegate_) {
    delegate_->RuntimeDidShutdown();
  }
}

void ChromotingClientRuntime::Init(
    ChromotingClientRuntime::Delegate* delegate) {
  DCHECK(delegate);
  DCHECK(!delegate_);
  delegate_ = delegate;
  url_requester_ = new URLRequestContextGetter(network_task_runner_);
  log_writer_ = std::make_unique<TelemetryLogWriter>(kTelemetryBaseUrl,
                                                     CreateOAuthTokenGetter());
  network_task_runner()->PostTask(
      FROM_HERE,
      base::BindOnce(&ChromotingClientRuntime::InitializeOnNetworkThread,
                     base::Unretained(this)));
}

std::unique_ptr<OAuthTokenGetter>
ChromotingClientRuntime::CreateOAuthTokenGetter() {
  return std::make_unique<OAuthTokenGetterProxy>(
      delegate_->oauth_token_getter(), ui_task_runner());
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromotingClientRuntime::url_loader_factory() {
  DCHECK(network_task_runner()->BelongsToCurrentThread());
  return url_loader_factory_owner_->GetURLLoaderFactory();
}

void ChromotingClientRuntime::InitializeOnNetworkThread() {
  DCHECK(network_task_runner()->BelongsToCurrentThread());
  url_loader_factory_owner_ =
      std::make_unique<network::TransitionalURLLoaderFactoryOwner>(
          url_requester_);
  log_writer_->Init(
      std::make_unique<ChromiumUrlRequestFactory>(url_loader_factory()));
}

}  // namespace remoting
