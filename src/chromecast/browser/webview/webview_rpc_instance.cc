// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/webview/webview_rpc_instance.h"

#include "base/bind.h"
#include "base/callback.h"
#include "chromecast/browser/cast_browser_context.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/webview/platform_views_rpc_instance.h"
#include "chromecast/browser/webview/webview_browser_context.h"
#include "chromecast/browser/webview/webview_controller.h"
#include "third_party/grpc/src/include/grpcpp/grpcpp.h"
#include "third_party/grpc/src/include/grpcpp/security/server_credentials.h"
#include "third_party/grpc/src/include/grpcpp/server.h"
#include "third_party/grpc/src/include/grpcpp/server_builder.h"

namespace chromecast {

WebviewRpcInstance::WebviewRpcInstance(
    webview::PlatformViewsService::AsyncService* service,
    grpc::ServerCompletionQueue* cq,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner,
    WebviewWindowManager* window_manager,
    bool enabled_for_dev)
    : PlatformViewsRpcInstance(cq, task_runner, window_manager),
      platform_views_service_(service),
      enabled_for_dev_(enabled_for_dev) {
  platform_views_service_->RequestCreateWebview(&ctx_, &io_, cq_, cq_,
                                                &init_callback_);
}

WebviewRpcInstance::~WebviewRpcInstance() {}

void WebviewRpcInstance::CreateNewInstance() {
  new WebviewRpcInstance(platform_views_service_, cq_, task_runner_,
                         window_manager_, enabled_for_dev_);
}

bool WebviewRpcInstance::Initialize() {
  if (request_->type_case() != webview::WebviewRequest::kCreate)
    return false;

  // This needs to be done on a valid thread.
  task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WebviewRpcInstance::CreateWebview, base::Unretained(this),
                     request_->create().webview_id(),
                     request_->create().window_id(),
                     request_->create().incognito()));
  return true;
}

void WebviewRpcInstance::CreateWebview(int app_id,
                                       int window_id,
                                       bool incognito) {
  app_id_ = app_id;
  window_id_ = window_id;
  // Only start listening for window events after the Webview is created. It
  // doesn't make sense to listen before since there wouldn't be a Webview to
  // parent.
  window_manager_->AddObserver(this);

  content::BrowserContext* browser_context =
      shell::CastBrowserProcess::GetInstance()->browser_context();
  if (incognito) {
    controller_ = std::make_unique<WebviewController>(
        std::make_unique<WebviewBrowserContext>(browser_context), this,
        enabled_for_dev_);
  } else {
    controller_ = std::make_unique<WebviewController>(browser_context, this,
                                                      enabled_for_dev_);
  }

  // Begin reading again.
  io_.Read(request_.get(), &read_callback_);
}

}  // namespace chromecast
