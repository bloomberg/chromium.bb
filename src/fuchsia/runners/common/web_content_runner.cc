// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/common/web_content_runner.h"

#include <fuchsia/sys/cpp/fidl.h>
#include <lib/fdio/directory.h>
#include <lib/fidl/cpp/binding_set.h>
#include <lib/sys/cpp/component_context.h>
#include <lib/sys/cpp/service_directory.h>
#include <utility>

#include "base/bind.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/fuchsia/file_utils.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/fuchsia/process_context.h"
#include "base/fuchsia/scoped_service_binding.h"
#include "base/fuchsia/startup_context.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "fuchsia/engine/web_instance_host/web_instance_host.h"
#include "fuchsia/runners/buildflags.h"
#include "fuchsia/runners/common/web_component.h"
#include "url/gurl.h"

namespace {

bool IsChannelClosed(const zx::channel& channel) {
  zx_signals_t observed = 0u;
  zx_status_t status =
      channel.wait_one(ZX_ERR_PEER_CLOSED, zx::time(), &observed);
  return status == ZX_OK;
}

std::string CreateUniqueComponentName() {
  static int last_component_id_ = 0;
  return base::StringPrintf("web-component:%d", ++last_component_id_);
}

}  // namespace

WebContentRunner::WebContentRunner(
    cr_fuchsia::WebInstanceHost* web_instance_host,
    GetContextParamsCallback get_context_params_callback)
    : web_instance_host_(web_instance_host),
      get_context_params_callback_(std::move(get_context_params_callback)) {
  DCHECK(web_instance_host_);
  DCHECK(get_context_params_callback_);
}

WebContentRunner::WebContentRunner(
    cr_fuchsia::WebInstanceHost* web_instance_host,
    fuchsia::web::CreateContextParams context_params)
    : web_instance_host_(web_instance_host) {
  CreateWebInstanceAndContext(std::move(context_params));
}

WebContentRunner::~WebContentRunner() = default;

fidl::InterfaceRequestHandler<fuchsia::web::FrameHost>
WebContentRunner::GetFrameHostRequestHandler() {
  return [this](fidl::InterfaceRequest<fuchsia::web::FrameHost> request) {
    EnsureWebInstanceAndContext();
    fdio_service_connect_at(web_instance_services_.channel().get(),
                            fuchsia::web::FrameHost::Name_,
                            request.TakeChannel().release());
  };
}

void WebContentRunner::CreateFrameWithParams(
    fuchsia::web::CreateFrameParams params,
    fidl::InterfaceRequest<fuchsia::web::Frame> request) {
  EnsureWebInstanceAndContext();

  context_->CreateFrameWithParams(std::move(params), std::move(request));
}

void WebContentRunner::StartComponent(
    fuchsia::sys::Package package,
    fuchsia::sys::StartupInfo startup_info,
    fidl::InterfaceRequest<fuchsia::sys::ComponentController>
        controller_request) {
  GURL url(package.resolved_url);
  if (!url.is_valid()) {
    LOG(ERROR) << "Rejected invalid URL: " << url;
    return;
  }

  std::unique_ptr<WebComponent> component = std::make_unique<WebComponent>(
      CreateUniqueComponentName(), this,
      std::make_unique<base::StartupContext>(std::move(startup_info)),
      std::move(controller_request));
#if BUILDFLAG(WEB_RUNNER_REMOTE_DEBUGGING_PORT) != 0
  component->EnableRemoteDebugging();
#endif
  component->StartComponent();
  component->LoadUrl(url, std::vector<fuchsia::net::http::Header>());
  RegisterComponent(std::move(component));
}

WebComponent* WebContentRunner::GetAnyComponent() {
  if (components_.empty())
    return nullptr;

  return components_.begin()->get();
}

void WebContentRunner::DestroyComponent(WebComponent* component) {
  components_.erase(components_.find(component));
  if (components_.empty() && on_empty_callback_)
    std::move(on_empty_callback_).Run();
}

void WebContentRunner::RegisterComponent(
    std::unique_ptr<WebComponent> component) {
  components_.insert(std::move(component));
}

void WebContentRunner::SetOnEmptyCallback(base::OnceClosure on_empty) {
  on_empty_callback_ = std::move(on_empty);
}

void WebContentRunner::DestroyWebContext() {
  DCHECK(get_context_params_callback_);
  context_ = nullptr;
}

void WebContentRunner::EnsureWebInstanceAndContext() {
  // Synchronously check whether the web.Context channel has closed, to reduce
  // the chance of issuing CreateFrameWithParams() to an already-closed channel.
  // This avoids potentially flaking a test - see crbug.com/1173418.
  if (context_ && IsChannelClosed(context_.channel()))
    context_.Unbind();

  if (!context_) {
    DCHECK(get_context_params_callback_);
    CreateWebInstanceAndContext(get_context_params_callback_.Run());
  }
}

void WebContentRunner::CreateWebInstanceAndContext(
    fuchsia::web::CreateContextParams params) {
  web_instance_host_->CreateInstanceForContext(
      std::move(params), web_instance_services_.NewRequest());
  zx_status_t result = fdio_service_connect_at(
      web_instance_services_.channel().get(), fuchsia::web::Context::Name_,
      context_.NewRequest().TakeChannel().release());
  ZX_LOG_IF(ERROR, result != ZX_OK, result)
      << "fdio_service_connect_at(web.Context)";
  context_.set_error_handler([](zx_status_t status) {
    ZX_LOG(ERROR, status) << "Connection to web.Context lost.";
  });
}
