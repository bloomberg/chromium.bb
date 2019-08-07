// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/exported/renderer_webapplicationcachehost_impl.h"

#include "third_party/blink/public/common/loader/url_loader_factory_bundle.h"
#include "third_party/blink/public/web/web_console_message.h"
#include "third_party/blink/public/web/web_local_frame.h"
#include "third_party/blink/public/web/web_local_frame_client.h"
#include "third_party/blink/public/web/web_view.h"
#include "third_party/blink/renderer/platform/web_test_support.h"

namespace blink {

RendererWebApplicationCacheHostImpl::RendererWebApplicationCacheHostImpl(
    WebLocalFrame* web_local_frame,
    WebApplicationCacheHostClient* client,
    const base::UnguessableToken& appcache_host_id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : WebApplicationCacheHostImpl(web_local_frame,
                                  client,
                                  appcache_host_id,
                                  std::move(task_runner)),
      web_local_frame_(web_local_frame) {}

void RendererWebApplicationCacheHostImpl::LogMessage(
    mojom::blink::ConsoleMessageLevel log_level,
    const String& message) {
  if (WebTestSupport::IsRunningWebTest())
    return;

  if (!web_local_frame_ || !web_local_frame_->View() ||
      !web_local_frame_->View()->MainFrame())
    return;

  WebFrame* main_frame = web_local_frame_->View()->MainFrame();
  if (!main_frame->IsWebLocalFrame())
    return;
  // TODO(michaeln): Make app cache host per-frame and correctly report to the
  // involved frame.
  main_frame->ToWebLocalFrame()->AddMessageToConsole(WebConsoleMessage(
      static_cast<mojom::blink::ConsoleMessageLevel>(log_level), message));
}

void RendererWebApplicationCacheHostImpl::SetSubresourceFactory(
    network::mojom::blink::URLLoaderFactoryPtr url_loader_factory) {
  auto info = std::make_unique<URLLoaderFactoryBundleInfo>();
  info->appcache_factory_info().set_handle(
      url_loader_factory.PassInterface().PassHandle());
  web_local_frame_->Client()->UpdateSubresourceFactory(std::move(info));
}

std::unique_ptr<WebApplicationCacheHost>
WebApplicationCacheHost::CreateWebApplicationCacheHostForFrame(
    WebLocalFrame* frame,
    WebApplicationCacheHostClient* client,
    const base::UnguessableToken& appcache_host_id,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  auto renderer_webapplicationcachehost_impl =
      std::make_unique<RendererWebApplicationCacheHostImpl>(
          frame, client, appcache_host_id, std::move(task_runner));
  return std::move(renderer_webapplicationcachehost_impl);
}

}  // namespace blink
