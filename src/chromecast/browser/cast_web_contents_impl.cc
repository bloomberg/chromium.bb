// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/cast_web_contents_impl.h"

#include <utility>

#include "base/bind.h"
#include "base/no_destructor.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chromecast/base/chromecast_switches.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/devtools/remote_debugging_server.h"
#include "chromecast/common/mojom/media_playback_options.mojom.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/resource_load_info.mojom.h"
#include "net/base/net_errors.h"
#include "services/service_manager/public/cpp/interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "url/gurl.h"

namespace chromecast {

namespace {

// IDs start at 1, since 0 is reserved for the root content window.
size_t next_tab_id = 1;

// Remove the given CastWebContents pointer from the global instance vector.
void RemoveCastWebContents(CastWebContents* instance) {
  auto& all_cast_web_contents = CastWebContents::GetAll();
  auto it = std::find(all_cast_web_contents.begin(),
                      all_cast_web_contents.end(), instance);
  if (it != all_cast_web_contents.end()) {
    all_cast_web_contents.erase(it);
  }
}

}  // namespace

// static
std::vector<CastWebContents*>& CastWebContents::GetAll() {
  static base::NoDestructor<std::vector<CastWebContents*>> instance;
  return *instance;
}

CastWebContentsImpl::CastWebContentsImpl(content::WebContents* web_contents,
                                         const InitParams& init_params)
    : web_contents_(web_contents),
      delegate_(init_params.delegate),
      page_state_(PageState::IDLE),
      last_state_(PageState::IDLE),
      enabled_for_dev_(init_params.enabled_for_dev),
      use_cma_renderer_(init_params.use_cma_renderer),
      remote_debugging_server_(
          shell::CastBrowserProcess::GetInstance()->remote_debugging_server()),
      tab_id_(init_params.is_root_window ? 0 : next_tab_id++),
      main_frame_loaded_(false),
      closing_(false),
      stopped_(false),
      stop_notified_(false),
      notifying_(false),
      last_error_(net::OK),
      task_runner_(base::SequencedTaskRunnerHandle::Get()),
      weak_factory_(this) {
  DCHECK(web_contents_);
  DCHECK(web_contents_->GetController().IsInitialNavigation());
  DCHECK(!web_contents_->IsLoading());
  CastWebContents::GetAll().push_back(this);
  content::WebContentsObserver::Observe(web_contents_);
  if (enabled_for_dev_) {
    LOG(INFO) << "Enabling dev console for CastWebContentsImpl";
    remote_debugging_server_->EnableWebContentsForDebugging(web_contents_);
  }

  // TODO(yucliu): Change the flag name to kDisableCmaRenderer in a latter diff.
  if (GetSwitchValueBoolean(switches::kDisableMojoRenderer, false)) {
    use_cma_renderer_ = false;
  }
}

CastWebContentsImpl::~CastWebContentsImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!notifying_) << "Do not destroy CastWebContents during observer "
                         "notification!";

  DisableDebugging();
  for (auto& observer : observer_list_) {
    observer.ResetCastWebContents();
  }
  RemoveCastWebContents(this);
}

int CastWebContentsImpl::tab_id() const {
  return tab_id_;
}

content::WebContents* CastWebContentsImpl::web_contents() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return web_contents_;
}

CastWebContents::PageState CastWebContentsImpl::page_state() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return page_state_;
}

void CastWebContentsImpl::AddRendererFeatures(
    std::vector<RendererFeature> features) {
  for (auto& feature : features) {
    renderer_features_.push_back({feature.name, feature.value.Clone()});
  }
}

void CastWebContentsImpl::LoadUrl(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!web_contents_) {
    LOG(ERROR) << "Cannot load URL for deleted WebContents";
    return;
  }
  if (closing_) {
    LOG(ERROR) << "Cannot load URL for WebContents while closing";
    return;
  }
  OnPageLoading();
  LOG(INFO) << "Load url: " << url.possibly_invalid_spec();
  web_contents_->GetController().LoadURL(url, content::Referrer(),
                                         ui::PAGE_TRANSITION_TYPED, "");
  UpdatePageState();
  DCHECK_EQ(PageState::LOADING, page_state_);
  NotifyObservers();
}

void CastWebContentsImpl::ClosePage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!web_contents_ || closing_)
    return;
  closing_ = true;
  web_contents_->DispatchBeforeUnload(false /* auto_cancel */);
  web_contents_->ClosePage();
  // If the WebContents doesn't close within the specified timeout, then signal
  // the page closure anyway so that the Delegate can delete the WebContents and
  // stop the page itself.
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&CastWebContentsImpl::OnClosePageTimeout,
                     weak_factory_.GetWeakPtr()),
      base::TimeDelta::FromMilliseconds(1000));
}

void CastWebContentsImpl::Stop(int error_code) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (stopped_) {
    UpdatePageState();
    NotifyObservers();
    return;
  }
  last_error_ = error_code;
  closing_ = false;
  stopped_ = true;
  UpdatePageState();
  DCHECK_NE(PageState::IDLE, page_state_);
  DCHECK_NE(PageState::LOADING, page_state_);
  DCHECK_NE(PageState::LOADED, page_state_);
  NotifyObservers();
}

void CastWebContentsImpl::SetDelegate(CastWebContents::Delegate* delegate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_ = delegate;
}

void CastWebContentsImpl::AllowWebAndMojoWebUiBindings() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  content::RenderViewHost* rvh = web_contents_->GetRenderViewHost();
  DCHECK(rvh);
  rvh->GetMainFrame()->AllowBindings(content::BINDINGS_POLICY_WEB_UI |
                                     content::BINDINGS_POLICY_MOJO_WEB_UI);
}

// Set background to transparent before making the view visible. This is in
// case Chrome dev tools was opened and caused background color to be reset.
// Note: we also have to set color to black first, because
// RenderWidgetHostViewBase::SetBackgroundColor ignores setting color to
// current color, and it isn't aware that dev tools has changed the color.
void CastWebContentsImpl::ClearRenderWidgetHostView() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  content::RenderWidgetHostView* view =
      web_contents_->GetRenderWidgetHostView();
  if (view) {
    view->SetBackgroundColor(SK_ColorBLACK);
    view->SetBackgroundColor(SK_ColorTRANSPARENT);
  }
}

void CastWebContentsImpl::AddObserver(CastWebContents::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  observer_list_.AddObserver(observer);
}

void CastWebContentsImpl::RemoveObserver(CastWebContents::Observer* observer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(observer);
  observer_list_.RemoveObserver(observer);
}

service_manager::BinderRegistry* CastWebContentsImpl::binder_registry() {
  return &binder_registry_;
}

void CastWebContentsImpl::RegisterInterfaceProvider(
    const InterfaceSet& interface_set,
    service_manager::InterfaceProvider* interface_provider) {
  DCHECK(interface_provider);
  interface_providers_map_.emplace(interface_set, interface_provider);
}

void CastWebContentsImpl::OnClosePageTimeout() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!closing_ || stopped_) {
    return;
  }
  closing_ = false;
  Stop(net::OK);
}

void CastWebContentsImpl::RenderFrameCreated(
    content::RenderFrameHost* render_frame_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(render_frame_host);

  // New render frame has been created, we need to add it to the app
  // whitelisting session so URL requests are handled correctly. This must be
  // done before URL requests are executed within render frame.
  auto* process = render_frame_host->GetProcess();
  const int render_process_id = process->GetID();
  const int render_frame_id = render_frame_host->GetRoutingID();

  for (Observer& observer : observer_list_) {
    observer.RenderFrameCreated(render_process_id, render_frame_id);
  }

  chromecast::shell::mojom::FeatureManagerPtr feature_manager_ptr;
  render_frame_host->GetRemoteInterfaces()->GetInterface(&feature_manager_ptr);
  feature_manager_ptr->ConfigureFeatures(GetRendererFeatures());

  chromecast::shell::mojom::MediaPlaybackOptionsAssociatedPtr
      media_playback_options;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &media_playback_options);
  media_playback_options->SetUseCmaRenderer(use_cma_renderer_);
}

std::vector<chromecast::shell::mojom::FeaturePtr>
CastWebContentsImpl::GetRendererFeatures() {
  std::vector<chromecast::shell::mojom::FeaturePtr> features;
  for (const auto& feature : renderer_features_) {
    features.push_back(chromecast::shell::mojom::Feature::New(
        feature.name, feature.value.Clone()));
  }
  return features;
}

void CastWebContentsImpl::OnInterfaceRequestFromFrame(
    content::RenderFrameHost* /* render_frame_host */,
    const std::string& interface_name,
    mojo::ScopedMessagePipeHandle* interface_pipe) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (binder_registry_.TryBindInterface(interface_name, interface_pipe)) {
    return;
  }
  for (auto& entry : interface_providers_map_) {
    auto const& interface_set = entry.first;
    // Interface is provided by this InterfaceProvider.
    if (interface_set.find(interface_name) != interface_set.end()) {
      auto* interface_provider = entry.second;
      interface_provider->GetInterfaceByName(interface_name,
                                             std::move(*interface_pipe));
      break;
    }
  }
}

void CastWebContentsImpl::RenderProcessGone(base::TerminationStatus status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  LOG(INFO) << "Render process for main frame exited unexpectedly.";
  Stop(net::ERR_UNEXPECTED);
}

void CastWebContentsImpl::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(navigation_handle);
  if (!web_contents_ || closing_ || stopped_)
    return;
  if (!navigation_handle->IsInMainFrame())
    return;
  // Main frame has begun navigating/loading.
  OnPageLoading();
  start_loading_ticks_ = base::TimeTicks::Now();
  GURL loading_url;
  content::NavigationEntry* nav_entry =
      web_contents()->GetController().GetVisibleEntry();
  if (nav_entry) {
    loading_url = nav_entry->GetVirtualURL();
  }
  TracePageLoadBegin(loading_url);
  UpdatePageState();
  DCHECK_EQ(page_state_, PageState::LOADING);
  NotifyObservers();
}

void CastWebContentsImpl::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // If the navigation was not committed, it means either the page was a
  // download or error 204/205, or the navigation never left the previous
  // URL. Ignore these navigations.
  if (!navigation_handle->HasCommitted()) {
    LOG(WARNING) << "Navigation did not commit: url="
                 << navigation_handle->GetURL();
    return;
  }

  // Return early if we didn't navigate to an error page. Note that even if we
  // haven't navigated to an error page, there could still be errors in loading
  // the desired content: e.g. if the server returned HTTP 404, or if there is
  // an error with the content itself.
  if (!navigation_handle->IsErrorPage())
    return;

  net::Error error_code = navigation_handle->GetNetErrorCode();

  // If we abort errors in an iframe, it can create a really confusing
  // and fragile user experience.  Rather than create a list of errors
  // that are most likely to occur, we ignore all of them for now.
  if (!navigation_handle->IsInMainFrame()) {
    LOG(ERROR) << "Got error on sub-iframe: url=" << navigation_handle->GetURL()
               << ", error=" << error_code
               << ", description=" << net::ErrorToShortString(error_code);
    return;
  }

  LOG(ERROR) << "Got error on navigation: url=" << navigation_handle->GetURL()
             << ", error_code=" << error_code
             << ", description=" << net::ErrorToShortString(error_code);

  Stop(error_code);
  DCHECK_EQ(page_state_, PageState::ERROR);
}

void CastWebContentsImpl::DidFinishLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (page_state_ != PageState::LOADING || !web_contents_ ||
      render_frame_host != web_contents_->GetMainFrame()) {
    return;
  }
  // The main frame finished loading. Before proceeding, we need to verify that
  // the loaded page is the one that was requested.
  TracePageLoadEnd(validated_url);
  int http_status_code = 0;
  content::NavigationEntry* nav_entry =
      web_contents()->GetController().GetVisibleEntry();
  if (nav_entry) {
    http_status_code = nav_entry->GetHttpStatusCode();
  }

  if (http_status_code != 0 && http_status_code / 100 != 2) {
    // An error HTML page was loaded instead of the content we requested.
    LOG(ERROR) << "Failed loading page for: " << validated_url
               << "; http status code: " << http_status_code;
    Stop(net::ERR_FAILED);
    DCHECK_EQ(page_state_, PageState::ERROR);
    return;
  }
  // Main frame finished loading properly.
  base::TimeDelta load_time = base::TimeTicks::Now() - start_loading_ticks_;
  LOG(INFO) << "Finished loading page after " << load_time.InMilliseconds()
            << " ms, url=" << validated_url;
  OnPageLoaded();
}

void CastWebContentsImpl::DidFailLoad(
    content::RenderFrameHost* render_frame_host,
    const GURL& validated_url,
    int error_code,
    const base::string16& error_description) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Only report an error if we are the main frame.  See b/8433611.
  if (render_frame_host->GetParent()) {
    LOG(ERROR) << "Got error on sub-iframe: url=" << validated_url.spec()
               << ", error=" << error_code;
    return;
  }
  if (error_code == net::ERR_ABORTED) {
    // ERR_ABORTED means download was aborted by the app, typically this happens
    // when flinging URL for direct playback, the initial URLRequest gets
    // cancelled/aborted and then the same URL is requested via the buffered
    // data source for media::Pipeline playback.
    LOG(INFO) << "Load canceled: url=" << validated_url.spec();

    // We consider the page to be fully loaded in this case, since the app has
    // intentionally entered this state. If the app wanted to stop, it would
    // have called window.close() instead.
    OnPageLoaded();
    return;
  }

  LOG(ERROR) << "Got error on load: url=" << validated_url.spec()
             << ", error_code=" << error_code;

  TracePageLoadEnd(validated_url);
  Stop(error_code);
  DCHECK_EQ(PageState::ERROR, page_state_);
}

void CastWebContentsImpl::OnPageLoading() {
  closing_ = false;
  stopped_ = false;
  stop_notified_ = false;
  main_frame_loaded_ = false;
  last_error_ = net::OK;
}

void CastWebContentsImpl::OnPageLoaded() {
  main_frame_loaded_ = true;
  UpdatePageState();
  DCHECK(page_state_ == PageState::LOADED);
  NotifyObservers();
}

void CastWebContentsImpl::UpdatePageState() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  last_state_ = page_state_;
  if (!web_contents_) {
    DCHECK(stopped_);
    page_state_ = PageState::DESTROYED;
  } else if (!stopped_) {
    if (main_frame_loaded_) {
      page_state_ = PageState::LOADED;
    } else {
      page_state_ = PageState::LOADING;
    }
  } else if (stopped_) {
    if (last_error_ != net::OK) {
      page_state_ = PageState::ERROR;
    } else {
      page_state_ = PageState::CLOSED;
    }
  }
}

void CastWebContentsImpl::NotifyObservers() {
  if (!delegate_)
    return;
  // Don't notify if the page state didn't change.
  if (last_state_ == page_state_)
    return;
  // Don't recursively notify the delegate.
  if (notifying_)
    return;
  notifying_ = true;
  if (stopped_ && !stop_notified_) {
    stop_notified_ = true;
    delegate_->OnPageStopped(this, last_error_);
  } else {
    delegate_->OnPageStateChanged(this);
  }
  notifying_ = false;
}

void CastWebContentsImpl::ResourceLoadComplete(
    content::RenderFrameHost* render_frame_host,
    const content::GlobalRequestID& request_id,
    const content::mojom::ResourceLoadInfo& resource_load_info) {
  if (!web_contents_ || render_frame_host != web_contents_->GetMainFrame())
    return;
  int net_error = resource_load_info.net_error;
  if (net_error == net::OK)
    return;
  LOG(ERROR) << "Resource \"" << resource_load_info.url << "\" failed to load "
             << " with net_error=" << net_error
             << ", description=" << net::ErrorToShortString(net_error);
  for (auto& observer : observer_list_) {
    observer.ResourceLoadFailed(this);
  }
}

void CastWebContentsImpl::InnerWebContentsCreated(
    content::WebContents* inner_web_contents) {
  auto result = inner_contents_.insert(std::make_unique<CastWebContentsImpl>(
      inner_web_contents, InitParams{nullptr, enabled_for_dev_}));
  if (delegate_)
    delegate_->InnerContentsCreated(result.first->get(), this);
}

void CastWebContentsImpl::WebContentsDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  closing_ = false;
  DisableDebugging();
  content::WebContentsObserver::Observe(nullptr);
  web_contents_ = nullptr;
  Stop(net::OK);
  RemoveCastWebContents(this);
  DCHECK_EQ(PageState::DESTROYED, page_state_);
}

void CastWebContentsImpl::TracePageLoadBegin(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_ASYNC_BEGIN1("browser,navigation", "CastWebContentsImpl Launch",
                           this, "URL", url.possibly_invalid_spec());
}

void CastWebContentsImpl::TracePageLoadEnd(const GURL& url) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  TRACE_EVENT_ASYNC_END1("browser,navigation", "CastWebContentsImpl Launch",
                         this, "URL", url.possibly_invalid_spec());
}

void CastWebContentsImpl::DisableDebugging() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!enabled_for_dev_ || !web_contents_)
    return;
  LOG(INFO) << "Disabling dev console for " << web_contents_->GetVisibleURL();
  remote_debugging_server_->DisableWebContentsForDebugging(web_contents_);
}

std::ostream& operator<<(std::ostream& os,
                         CastWebContentsImpl::PageState state) {
#define CASE(state)                           \
  case CastWebContentsImpl::PageState::state: \
    os << #state;                             \
    return os;

  switch (state) {
    CASE(IDLE);
    CASE(LOADING);
    CASE(LOADED);
    CASE(CLOSED);
    CASE(DESTROYED);
    CASE(ERROR);
  }
#undef CASE
}

}  // namespace chromecast
