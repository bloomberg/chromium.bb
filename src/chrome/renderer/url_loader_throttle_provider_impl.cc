// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/url_loader_throttle_provider_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/common/google_url_loader_throttle.h"
#include "chrome/renderer/chrome_content_renderer_client.h"
#include "chrome/renderer/chrome_render_frame_observer.h"
#include "chrome/renderer/chrome_render_thread_observer.h"
#include "chrome/renderer/prerender/prerender_helper.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_params.h"
#include "chrome/renderer/subresource_redirect/subresource_redirect_url_loader_throttle.h"
#include "components/safe_browsing/content/renderer/renderer_url_loader_throttle.h"
#include "components/safe_browsing/core/features.h"
#include "content/public/common/content_features.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/common/loader/resource_type_util.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "extensions/common/switches.h"
#include "extensions/renderer/extension_throttle_manager.h"
#include "extensions/renderer/guest_view/mime_handler_view/mime_handler_view_container.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/renderer/chromeos_merge_session_loader_throttle.h"
#endif  // defined(OS_CHROMEOS)

namespace {

#if BUILDFLAG(ENABLE_EXTENSIONS)
std::unique_ptr<extensions::ExtensionThrottleManager>
CreateExtensionThrottleManager() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          extensions::switches::kDisableExtensionsHttpThrottling)) {
    return nullptr;
  }
  return std::make_unique<extensions::ExtensionThrottleManager>();
}

void SetExtensionThrottleManagerTestPolicy(
    extensions::ExtensionThrottleManager* extension_throttle_manager) {
  std::unique_ptr<net::BackoffEntry::Policy> policy(
      new net::BackoffEntry::Policy{
          // Number of initial errors (in sequence) to ignore before
          // applying exponential back-off rules.
          1,

          // Initial delay for exponential back-off in ms.
          10 * 60 * 1000,

          // Factor by which the waiting time will be multiplied.
          10,

          // Fuzzing percentage. ex: 10% will spread requests randomly
          // between 90%-100% of the calculated time.
          0.1,

          // Maximum amount of time we are willing to delay our request in ms.
          15 * 60 * 1000,

          // Time to keep an entry from being discarded even when it
          // has no significant state, -1 to never discard.
          -1,

          // Don't use initial delay unless the last request was an error.
          false,
      });
  extension_throttle_manager->SetBackoffPolicyForTests(std::move(policy));
}
#endif

}  // namespace

URLLoaderThrottleProviderImpl::URLLoaderThrottleProviderImpl(
    blink::ThreadSafeBrowserInterfaceBrokerProxy* broker,
    content::URLLoaderThrottleProviderType type,
    ChromeContentRendererClient* chrome_content_renderer_client)
    : type_(type),
      chrome_content_renderer_client_(chrome_content_renderer_client) {
  DETACH_FROM_THREAD(thread_checker_);
  broker->GetInterface(safe_browsing_remote_.InitWithNewPipeAndPassReceiver());
}

URLLoaderThrottleProviderImpl::~URLLoaderThrottleProviderImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

URLLoaderThrottleProviderImpl::URLLoaderThrottleProviderImpl(
    const URLLoaderThrottleProviderImpl& other)
    : type_(other.type_),
      chrome_content_renderer_client_(other.chrome_content_renderer_client_) {
  DETACH_FROM_THREAD(thread_checker_);
  if (other.safe_browsing_) {
    other.safe_browsing_->Clone(
        safe_browsing_remote_.InitWithNewPipeAndPassReceiver());
  }
  // An ad_delay_factory_ is created, rather than cloning the existing one.
}

std::unique_ptr<content::URLLoaderThrottleProvider>
URLLoaderThrottleProviderImpl::Clone() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (safe_browsing_remote_)
    safe_browsing_.Bind(std::move(safe_browsing_remote_));
  return base::WrapUnique(new URLLoaderThrottleProviderImpl(*this));
}

std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
URLLoaderThrottleProviderImpl::CreateThrottles(
    int render_frame_id,
    const blink::WebURLRequest& request) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  std::vector<std::unique_ptr<blink::URLLoaderThrottle>> throttles;

  const network::mojom::RequestDestination request_destination =
      request.GetRequestDestination();

  // Some throttles have already been added in the browser for frame resources.
  // Don't add them for frame requests.
  bool is_frame_resource =
      blink::IsRequestDestinationFrame(request_destination);

  DCHECK(!is_frame_resource ||
         type_ == content::URLLoaderThrottleProviderType::kFrame);

  if (!is_frame_resource) {
    if (safe_browsing_remote_)
      safe_browsing_.Bind(std::move(safe_browsing_remote_));
    throttles.push_back(
        std::make_unique<safe_browsing::RendererURLLoaderThrottle>(
            safe_browsing_.get(), render_frame_id));
  }

  if (type_ == content::URLLoaderThrottleProviderType::kFrame &&
      !is_frame_resource) {
    auto throttle =
        prerender::PrerenderHelper::MaybeCreateThrottle(render_frame_id);
    if (throttle)
      throttles.push_back(std::move(throttle));
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (type_ == content::URLLoaderThrottleProviderType::kFrame &&
      request_destination == network::mojom::RequestDestination::kObject) {
    content::RenderFrame* render_frame =
        content::RenderFrame::FromRoutingID(render_frame_id);
    auto mime_handlers =
        extensions::MimeHandlerViewContainer::FromRenderFrame(render_frame);
    GURL gurl(request.Url());
    for (auto* handler : mime_handlers) {
      auto throttle = handler->MaybeCreatePluginThrottle(gurl);
      if (throttle) {
        throttles.push_back(std::move(throttle));
        break;
      }
    }
  }

  if (!extension_throttle_manager_)
    extension_throttle_manager_ = CreateExtensionThrottleManager();

  if (extension_throttle_manager_) {
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            extensions::switches::kSetExtensionThrottleTestParams)) {
      SetExtensionThrottleManagerTestPolicy(extension_throttle_manager_.get());
    }

    std::unique_ptr<blink::URLLoaderThrottle> throttle =
        extension_throttle_manager_->MaybeCreateURLLoaderThrottle(request);
    if (throttle)
      throttles.push_back(std::move(throttle));
  }
#endif

#if defined(OS_ANDROID)
  std::string client_data_header;
  if (!is_frame_resource && render_frame_id != MSG_ROUTING_NONE) {
    client_data_header =
        ChromeRenderFrameObserver::GetCCTClientHeader(render_frame_id);
  }
#endif

  throttles.push_back(std::make_unique<GoogleURLLoaderThrottle>(
#if defined(OS_ANDROID)
      client_data_header,
#endif
      ChromeRenderThreadObserver::GetDynamicParams()));

#if defined(OS_CHROMEOS)
  throttles.push_back(std::make_unique<MergeSessionLoaderThrottle>(
      chrome_content_renderer_client_->GetChromeObserver()
          ->chromeos_listener()));
#endif  // defined(OS_CHROMEOS)

  auto throttle = subresource_redirect::SubresourceRedirectURLLoaderThrottle::
      MaybeCreateThrottle(request, render_frame_id);
  if (throttle)
    throttles.push_back(std::move(throttle));

  return throttles;
}

void URLLoaderThrottleProviderImpl::SetOnline(bool is_online) {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (extension_throttle_manager_)
    extension_throttle_manager_->SetOnline(is_online);
#endif
}
