// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "weblayer/browser/safe_browsing/safe_browsing_service.h"

#include "base/bind.h"
#include "base/path_service.h"
#include "base/task/post_task.h"
#include "components/safe_browsing/android/remote_database_manager.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_bridge.h"
#include "components/safe_browsing/content/browser/browser_url_loader_throttle.h"
#include "components/safe_browsing/content/browser/mojo_safe_browsing_impl.h"
#include "components/safe_browsing/core/browser/safe_browsing_network_context.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/resource_context.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "weblayer/browser/safe_browsing/safe_browsing_navigation_throttle.h"
#include "weblayer/browser/safe_browsing/url_checker_delegate_impl.h"

namespace weblayer {

namespace {

network::mojom::NetworkContextParamsPtr CreateDefaultNetworkContextParams(
    const std::string& user_agent) {
  network::mojom::NetworkContextParamsPtr network_context_params =
      network::mojom::NetworkContextParams::New();
  network_context_params->user_agent = user_agent;
  return network_context_params;
}

// Helper method that checks the RenderProcessHost is still alive before hopping
// over to the IO thread.
void MaybeCreateSafeBrowsing(
    int rph_id,
    content::ResourceContext* resource_context,
    base::RepeatingCallback<scoped_refptr<safe_browsing::UrlCheckerDelegate>()>
        get_checker_delegate,
    mojo::PendingReceiver<safe_browsing::mojom::SafeBrowsing> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  content::RenderProcessHost* render_process_host =
      content::RenderProcessHost::FromID(rph_id);
  if (!render_process_host)
    return;

  base::PostTask(
      FROM_HERE, {content::BrowserThread::IO},
      base::BindOnce(&safe_browsing::MojoSafeBrowsingImpl::MaybeCreate, rph_id,
                     resource_context, std::move(get_checker_delegate),
                     std::move(receiver)));
}

}  // namespace

SafeBrowsingService::SafeBrowsingService(const std::string& user_agent)
    : user_agent_(user_agent), safe_browsing_disabled_(false) {}

SafeBrowsingService::~SafeBrowsingService() = default;

void SafeBrowsingService::Initialize() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (network_context_) {
    // already initialized
    return;
  }

  safe_browsing_api_handler_.reset(
      new safe_browsing::SafeBrowsingApiHandlerBridge());
  safe_browsing::SafeBrowsingApiHandler::SetInstance(
      safe_browsing_api_handler_.get());

  base::FilePath user_data_dir;
  bool result =
      base::PathService::Get(base::DIR_ANDROID_APP_DATA, &user_data_dir);
  DCHECK(result);

  // safebrowsing network context needs to be created on the UI thread.
  network_context_ =
      std::make_unique<safe_browsing::SafeBrowsingNetworkContext>(
          user_data_dir,
          base::BindRepeating(CreateDefaultNetworkContextParams, user_agent_));

  CreateSafeBrowsingUIManager();
}

std::unique_ptr<blink::URLLoaderThrottle>
SafeBrowsingService::CreateURLLoaderThrottle(
    const base::RepeatingCallback<content::WebContents*()>& wc_getter,
    int frame_tree_node_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  return safe_browsing::BrowserURLLoaderThrottle::Create(
      base::BindOnce(
          [](SafeBrowsingService* sb_service) {
            return sb_service->GetSafeBrowsingUrlCheckerDelegate();
          },
          base::Unretained(this)),
      wc_getter, frame_tree_node_id,
      // rt_lookup_service are used to
      // perform real time url check, which is gated by UKM opted in. Since
      // WebLayer currently doesn't support UKM, this feature is not enabled.
      /*rt_lookup_service*/ nullptr);
}

std::unique_ptr<content::NavigationThrottle>
SafeBrowsingService::CreateSafeBrowsingNavigationThrottle(
    content::NavigationHandle* handle) {
  return std::make_unique<SafeBrowsingNavigationThrottle>(
      handle, GetSafeBrowsingUIManager());
}

scoped_refptr<safe_browsing::UrlCheckerDelegate>
SafeBrowsingService::GetSafeBrowsingUrlCheckerDelegate() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (!safe_browsing_url_checker_delegate_) {
    safe_browsing_url_checker_delegate_ = new UrlCheckerDelegateImpl(
        GetSafeBrowsingDBManager(), GetSafeBrowsingUIManager(),
        safe_browsing_disabled_);
  }

  return safe_browsing_url_checker_delegate_;
}

safe_browsing::RemoteSafeBrowsingDatabaseManager*
SafeBrowsingService::GetSafeBrowsingDBManager() {
  if (!safe_browsing_db_manager_) {
    CreateAndStartSafeBrowsingDBManager();
  }
  return safe_browsing_db_manager_.get();
}

SafeBrowsingUIManager* SafeBrowsingService::GetSafeBrowsingUIManager() {
  return ui_manager_.get();
}

void SafeBrowsingService::CreateSafeBrowsingUIManager() {
  DCHECK(!ui_manager_);
  ui_manager_ = new SafeBrowsingUIManager();
}

void SafeBrowsingService::CreateAndStartSafeBrowsingDBManager() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  DCHECK(!safe_browsing_db_manager_);

  safe_browsing_db_manager_ =
      new safe_browsing::RemoteSafeBrowsingDatabaseManager();

  // V4ProtocolConfig is not used. Just create one with empty values.
  safe_browsing::V4ProtocolConfig config("", false, "", "");
  safe_browsing_db_manager_->StartOnIOThread(GetURLLoaderFactoryOnIOThread(),
                                             config);
}

scoped_refptr<network::SharedURLLoaderFactory>
SafeBrowsingService::GetURLLoaderFactoryOnIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!shared_url_loader_factory_on_io_) {
    base::PostTask(
        FROM_HERE, {content::BrowserThread::UI},
        base::BindOnce(&SafeBrowsingService::CreateURLLoaderFactoryForIO,
                       base::Unretained(this),
                       url_loader_factory_on_io_.BindNewPipeAndPassReceiver()));
    shared_url_loader_factory_on_io_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            url_loader_factory_on_io_.get());
  }
  return shared_url_loader_factory_on_io_;
}

void SafeBrowsingService::CreateURLLoaderFactoryForIO(
    mojo::PendingReceiver<network::mojom::URLLoaderFactory> receiver) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto url_loader_factory_params =
      network::mojom::URLLoaderFactoryParams::New();
  url_loader_factory_params->process_id = network::mojom::kBrowserProcessId;
  url_loader_factory_params->is_corb_enabled = false;
  network_context_->GetNetworkContext()->CreateURLLoaderFactory(
      std::move(receiver), std::move(url_loader_factory_params));
}

void SafeBrowsingService::AddInterface(
    service_manager::BinderRegistry* registry,
    content::RenderProcessHost* render_process_host) {
  content::ResourceContext* resource_context =
      render_process_host->GetBrowserContext()->GetResourceContext();
  registry->AddInterface(
      base::BindRepeating(
          &MaybeCreateSafeBrowsing, render_process_host->GetID(),
          resource_context,
          base::BindRepeating(
              &SafeBrowsingService::GetSafeBrowsingUrlCheckerDelegate,
              base::Unretained(this))),
      base::CreateSingleThreadTaskRunner({content::BrowserThread::UI}));
}

void SafeBrowsingService::StopDBManager() {
  base::PostTask(FROM_HERE, {content::BrowserThread::IO},
                 base::BindOnce(&SafeBrowsingService::StopDBManagerOnIOThread,
                                base::Unretained(this)));
}

void SafeBrowsingService::StopDBManagerOnIOThread() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (safe_browsing_db_manager_) {
    safe_browsing_db_manager_->StopOnIOThread(true /*shutdown*/);
    safe_browsing_db_manager_.reset();
  }
}

void SafeBrowsingService::SetSafeBrowsingDisabled(bool disabled) {
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&SafeBrowsingService::SetSafeBrowsingDisabledOnIOThread,
                     base::Unretained(this), disabled));
}

void SafeBrowsingService::SetSafeBrowsingDisabledOnIOThread(bool disabled) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

  if (safe_browsing_disabled_ != disabled) {
    safe_browsing_disabled_ = disabled;
    // If there is no safe_browsing_url_checker_delegate_ yet the opt_out
    // setting will be set later during its creation.
    if (safe_browsing_url_checker_delegate_) {
      safe_browsing_url_checker_delegate_->SetSafeBrowsingDisabled(disabled);
    }
  }
}

}  // namespace weblayer
