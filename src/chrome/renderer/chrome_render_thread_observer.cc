// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/chrome_render_thread_observer.h"

#include <stddef.h>

#include <limits>
#include <memory>
#include <set>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/metrics/histogram.h"
#include "base/metrics/statistics_recorder.h"
#include "base/no_destructor.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/post_task.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/common/cache_stats_recorder.mojom.h"
#include "chrome/common/child_process_logging.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/media/media_resource_provider.h"
#include "chrome/common/net/net_resource_provider.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "components/visitedlink/renderer/visitedlink_reader.h"
#include "content/public/child/child_thread.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/resource_usage_reporter_type_converters.h"
#include "content/public/renderer/render_thread.h"
#include "content/public/renderer/render_view.h"
#include "content/public/renderer/render_view_visitor.h"
#include "content/public/renderer/resource_dispatcher_delegate.h"
#include "extensions/buildflags/buildflags.h"
#include "ipc/ipc_sync_channel.h"
#include "media/base/localized_strings.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "net/base/net_errors.h"
#include "net/base/net_module.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/web_document.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_security_policy.h"
#include "third_party/blink/public/web/web_view.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/renderer/extensions/extension_localization_peer.h"
#endif

#if defined(OS_CHROMEOS)
#include "chrome/renderer/chromeos_merge_session_loader_throttle.h"
#endif

using blink::WebCache;
using blink::WebSecurityPolicy;
using blink::WebString;
using content::RenderThread;

namespace {

const int kCacheStatsDelayMS = 2000;

class RendererResourceDelegate : public content::ResourceDispatcherDelegate {
 public:
  RendererResourceDelegate() {}

  void OnRequestComplete() override {
    // Update the browser about our cache.
    // Rate limit informing the host of our cache stats.
    if (!weak_factory_.HasWeakPtrs()) {
      base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
          FROM_HERE,
          base::BindOnce(&RendererResourceDelegate::InformHostOfCacheStats,
                         weak_factory_.GetWeakPtr()),
          base::TimeDelta::FromMilliseconds(kCacheStatsDelayMS));
    }
  }

  std::unique_ptr<content::RequestPeer> OnReceivedResponse(
      std::unique_ptr<content::RequestPeer> current_peer,
      const std::string& mime_type,
      const GURL& url) override {
#if BUILDFLAG(ENABLE_EXTENSIONS)
    return ExtensionLocalizationPeer::CreateExtensionLocalizationPeer(
        std::move(current_peer), RenderThread::Get(), mime_type, url);
#else
    return current_peer;
#endif
  }

 private:
  void InformHostOfCacheStats() {
    WebCache::UsageStats stats;
    WebCache::GetUsageStats(&stats);
    if (!cache_stats_recorder_) {
      RenderThread::Get()->GetChannel()->GetRemoteAssociatedInterface(
          &cache_stats_recorder_);
    }
    cache_stats_recorder_->RecordCacheStats(stats.capacity, stats.size);
  }

  mojo::AssociatedRemote<chrome::mojom::CacheStatsRecorder>
      cache_stats_recorder_;

  base::WeakPtrFactory<RendererResourceDelegate> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RendererResourceDelegate);
};

#if defined(OS_CHROMEOS)
scoped_refptr<base::SequencedTaskRunner> GetCallbackGroupTaskRunner() {
  content::ChildThread* child_thread = content::ChildThread::Get();
  if (child_thread)
    return child_thread->GetIOTaskRunner();

  // This will happen when running via tests.
  return base::SequencedTaskRunnerHandle::Get();
}
#endif  // defined(OS_CHROMEOS)

}  // namespace

bool ChromeRenderThreadObserver::is_incognito_process_ = false;

#if defined(OS_CHROMEOS)
// static
scoped_refptr<ChromeRenderThreadObserver::ChromeOSListener>
ChromeRenderThreadObserver::ChromeOSListener::Create(
    mojo::PendingReceiver<chrome::mojom::ChromeOSListener>
        chromeos_listener_receiver) {
  scoped_refptr<ChromeOSListener> helper = new ChromeOSListener();
  content::ChildThread::Get()->GetIOTaskRunner()->PostTask(
      FROM_HERE, base::BindOnce(&ChromeOSListener::BindOnIOThread, helper,
                                std::move(chromeos_listener_receiver)));
  return helper;
}

bool ChromeRenderThreadObserver::ChromeOSListener::IsMergeSessionRunning()
    const {
  base::AutoLock lock(lock_);
  return merge_session_running_;
}

void ChromeRenderThreadObserver::ChromeOSListener::RunWhenMergeSessionFinished(
    DelayedCallbackGroup::Callback callback) {
  base::AutoLock lock(lock_);
  DCHECK(merge_session_running_);
  session_merged_callbacks_->Add(std::move(callback));
}

void ChromeRenderThreadObserver::ChromeOSListener::MergeSessionComplete() {
  {
    base::AutoLock lock(lock_);
    merge_session_running_ = false;
  }
  session_merged_callbacks_->RunAll();
}

ChromeRenderThreadObserver::ChromeOSListener::ChromeOSListener()
    : session_merged_callbacks_(base::MakeRefCounted<DelayedCallbackGroup>(
          MergeSessionLoaderThrottle::GetMergeSessionTimeout(),
          GetCallbackGroupTaskRunner())),
      merge_session_running_(true) {}

ChromeRenderThreadObserver::ChromeOSListener::~ChromeOSListener() {}

void ChromeRenderThreadObserver::ChromeOSListener::BindOnIOThread(
    mojo::PendingReceiver<chrome::mojom::ChromeOSListener>
        chromeos_listener_receiver) {
  receiver_.Bind(std::move(chromeos_listener_receiver));
}
#endif  // defined(OS_CHROMEOS)

chrome::mojom::DynamicParams* GetDynamicConfigParams() {
  static base::NoDestructor<chrome::mojom::DynamicParams> dynamic_params;
  return dynamic_params.get();
}

ChromeRenderThreadObserver::ChromeRenderThreadObserver()
    : visited_link_reader_(new visitedlink::VisitedLinkReader) {
  RenderThread* thread = RenderThread::Get();
  resource_delegate_.reset(new RendererResourceDelegate());
  thread->SetResourceDispatcherDelegate(resource_delegate_.get());

  // Configure modules that need access to resources.
  net::NetModule::SetResourceProvider(ChromeNetResourceProvider);
  media::SetLocalizedStringProvider(ChromeMediaLocalizedStringProvider);
}

ChromeRenderThreadObserver::~ChromeRenderThreadObserver() {}

// static
const chrome::mojom::DynamicParams&
ChromeRenderThreadObserver::GetDynamicParams() {
  return *GetDynamicConfigParams();
}

void ChromeRenderThreadObserver::RegisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->AddInterface(base::Bind(
      &ChromeRenderThreadObserver::OnRendererConfigurationAssociatedRequest,
      base::Unretained(this)));
}

void ChromeRenderThreadObserver::UnregisterMojoInterfaces(
    blink::AssociatedInterfaceRegistry* associated_interfaces) {
  associated_interfaces->RemoveInterface(
      chrome::mojom::RendererConfiguration::Name_);
}

void ChromeRenderThreadObserver::SetInitialConfiguration(
    bool is_incognito_process,
    mojo::PendingReceiver<chrome::mojom::ChromeOSListener>
        chromeos_listener_receiver) {
  is_incognito_process_ = is_incognito_process;
#if defined(OS_CHROMEOS)
  if (chromeos_listener_receiver) {
    chromeos_listener_ =
        ChromeOSListener::Create(std::move(chromeos_listener_receiver));
  }
#endif  // defined(OS_CHROMEOS)
}

void ChromeRenderThreadObserver::SetConfiguration(
    chrome::mojom::DynamicParamsPtr params) {
  *GetDynamicConfigParams() = std::move(*params);
}

void ChromeRenderThreadObserver::SetContentSettingRules(
    const RendererContentSettingRules& rules) {
  content_setting_rules_ = rules;
}

void ChromeRenderThreadObserver::OnRendererConfigurationAssociatedRequest(
    mojo::PendingAssociatedReceiver<chrome::mojom::RendererConfiguration>
        receiver) {
  renderer_configuration_receivers_.Add(this, std::move(receiver));
}

const RendererContentSettingRules*
ChromeRenderThreadObserver::content_setting_rules() const {
  return &content_setting_rules_;
}
