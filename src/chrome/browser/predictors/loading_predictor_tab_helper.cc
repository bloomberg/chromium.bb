// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_predictor_tab_helper.h"

#include <set>
#include <string>

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/predictors/loading_predictor.h"
#include "chrome/browser/predictors/loading_predictor_factory.h"
#include "chrome/browser/predictors/predictors_enums.h"
#include "chrome/browser/predictors/predictors_features.h"
#include "chrome/browser/profiles/profile.h"
#include "components/optimization_guide/optimization_guide_decider.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/render_frame_host.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "services/network/public/mojom/fetch_api.mojom.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom.h"

using content::BrowserThread;

namespace predictors {

namespace {

constexpr char kLoadingPredictorOptimizationHintsReceiveStatusHistogram[] =
    "LoadingPredictor.OptimizationHintsReceiveStatus";

// Called only for subresources.
net::RequestPriority GetRequestPriority(
    network::mojom::RequestDestination request_destination) {
  switch (request_destination) {
    case network::mojom::RequestDestination::kStyle:
    case network::mojom::RequestDestination::kFont:
      return net::HIGHEST;

    case network::mojom::RequestDestination::kScript:
      return net::MEDIUM;

    case network::mojom::RequestDestination::kEmpty:
    case network::mojom::RequestDestination::kAudio:
    case network::mojom::RequestDestination::kAudioWorklet:
    case network::mojom::RequestDestination::kDocument:
    case network::mojom::RequestDestination::kEmbed:
    case network::mojom::RequestDestination::kFrame:
    case network::mojom::RequestDestination::kIframe:
    case network::mojom::RequestDestination::kImage:
    case network::mojom::RequestDestination::kManifest:
    case network::mojom::RequestDestination::kObject:
    case network::mojom::RequestDestination::kPaintWorklet:
    case network::mojom::RequestDestination::kReport:
    case network::mojom::RequestDestination::kServiceWorker:
    case network::mojom::RequestDestination::kSharedWorker:
    case network::mojom::RequestDestination::kTrack:
    case network::mojom::RequestDestination::kVideo:
    case network::mojom::RequestDestination::kWorker:
    case network::mojom::RequestDestination::kXslt:
      return net::LOWEST;
  }
}

bool IsHandledNavigation(content::NavigationHandle* navigation_handle) {
  return navigation_handle->IsInMainFrame() &&
         !navigation_handle->IsSameDocument() &&
         navigation_handle->GetURL().SchemeIsHTTPOrHTTPS();
}

// Util class for recording the status for when we received optimization hints
// for navigations that we requested them for.
class ScopedOptimizationHintsReceiveStatusRecorder {
 public:
  ScopedOptimizationHintsReceiveStatusRecorder()
      : status_(OptimizationHintsReceiveStatus::kUnknown) {}
  ~ScopedOptimizationHintsReceiveStatusRecorder() {
    DCHECK_NE(status_, OptimizationHintsReceiveStatus::kUnknown);
    UMA_HISTOGRAM_ENUMERATION(
        kLoadingPredictorOptimizationHintsReceiveStatusHistogram, status_);
  }

  void set_status(OptimizationHintsReceiveStatus status) { status_ = status; }

 private:
  OptimizationHintsReceiveStatus status_;
};

}  // namespace

LoadingPredictorTabHelper::LoadingPredictorTabHelper(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  auto* predictor = LoadingPredictorFactory::GetForProfile(profile);
  if (predictor)
    predictor_ = predictor->GetWeakPtr();
  if (base::FeatureList::IsEnabled(
          features::kLoadingPredictorUseOptimizationGuide)) {
    optimization_guide_decider_ =
        OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
    if (optimization_guide_decider_) {
      optimization_guide_decider_->RegisterOptimizationTypesAndTargets(
          {optimization_guide::proto::LOADING_PREDICTOR},
          /*optimization_targets=*/{});
    }
  }
}

LoadingPredictorTabHelper::~LoadingPredictorTabHelper() = default;

void LoadingPredictorTabHelper::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  if (!predictor_)
    return;

  if (!IsHandledNavigation(navigation_handle))
    return;

  auto navigation_id = NavigationID(
      web_contents(),
      ukm::ConvertToSourceId(navigation_handle->GetNavigationId(),
                             ukm::SourceIdType::NAVIGATION_ID),
      navigation_handle->GetURL(), navigation_handle->NavigationStart());
  if (!navigation_id.is_valid())
    return;
  current_navigation_id_ = navigation_id;

  if (predictor_->OnNavigationStarted(navigation_id))
    return;

  if (!optimization_guide_decider_)
    return;

  last_optimization_guide_prediction_ = OptimizationGuidePrediction();
  last_optimization_guide_prediction_->decision =
      optimization_guide::OptimizationGuideDecision::kUnknown;

  // We do not have any predictions on device, so consult the optimization
  // guide.
  optimization_guide_decider_->CanApplyOptimizationAsync(
      navigation_handle, optimization_guide::proto::LOADING_PREDICTOR,
      base::BindOnce(&LoadingPredictorTabHelper::OnOptimizationGuideDecision,
                     weak_ptr_factory_.GetWeakPtr(), navigation_id));
}

void LoadingPredictorTabHelper::DidRedirectNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!predictor_)
    return;

  if (!IsHandledNavigation(navigation_handle))
    return;

  auto navigation_id = NavigationID(
      web_contents(),
      ukm::ConvertToSourceId(navigation_handle->GetNavigationId(),
                             ukm::SourceIdType::NAVIGATION_ID),
      navigation_handle->GetURL(), navigation_handle->NavigationStart());
  if (!navigation_id.is_valid())
    return;
  current_navigation_id_ = navigation_id;

  // If we are not trying to get an optimization guide prediction for this page
  // load, just return.
  if (!optimization_guide_decider_ || !last_optimization_guide_prediction_)
    return;

  // Get an updated prediction for the navigation.
  optimization_guide_decider_->CanApplyOptimizationAsync(
      navigation_handle, optimization_guide::proto::LOADING_PREDICTOR,
      base::BindOnce(&LoadingPredictorTabHelper::OnOptimizationGuideDecision,
                     weak_ptr_factory_.GetWeakPtr(), navigation_id));
}

void LoadingPredictorTabHelper::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!predictor_)
    return;

  if (!IsHandledNavigation(navigation_handle))
    return;

  // Clear out the current navigation since there is not one in flight anymore.
  current_navigation_id_ = NavigationID();

  auto old_navigation_id =
      NavigationID(web_contents(),
                   ukm::ConvertToSourceId(navigation_handle->GetNavigationId(),
                                          ukm::SourceIdType::NAVIGATION_ID),
                   navigation_handle->GetRedirectChain().front(),
                   navigation_handle->NavigationStart());
  auto new_navigation_id = NavigationID(
      web_contents(),
      ukm::ConvertToSourceId(navigation_handle->GetNavigationId(),
                             ukm::SourceIdType::NAVIGATION_ID),
      navigation_handle->GetURL(), navigation_handle->NavigationStart());
  if (!old_navigation_id.is_valid() || !new_navigation_id.is_valid())
    return;

  predictor_->OnNavigationFinished(old_navigation_id, new_navigation_id,
                                   navigation_handle->IsErrorPage());
}

void LoadingPredictorTabHelper::ResourceLoadComplete(
    content::RenderFrameHost* render_frame_host,
    const content::GlobalRequestID& request_id,
    const blink::mojom::ResourceLoadInfo& resource_load_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!predictor_)
    return;

  bool is_main_frame = render_frame_host->GetParent() == nullptr;
  if (!is_main_frame)
    return;

  auto navigation_id = NavigationID(web_contents());
  if (!navigation_id.is_valid())
    return;

  predictor_->loading_data_collector()->RecordResourceLoadComplete(
      navigation_id, resource_load_info);
}

void LoadingPredictorTabHelper::DidLoadResourceFromMemoryCache(
    const GURL& url,
    const std::string& mime_type,
    network::mojom::RequestDestination request_destination) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!predictor_)
    return;

  auto navigation_id = NavigationID(web_contents());
  if (!navigation_id.is_valid())
    return;

  blink::mojom::ResourceLoadInfo resource_load_info;
  resource_load_info.original_url = url;
  resource_load_info.final_url = url;
  resource_load_info.mime_type = mime_type;
  resource_load_info.request_destination = request_destination;
  resource_load_info.method = "GET";
  resource_load_info.request_priority =
      GetRequestPriority(resource_load_info.request_destination);
  resource_load_info.network_info =
      blink::mojom::CommonNetworkInfo::New(false, false, base::nullopt);
  predictor_->loading_data_collector()->RecordResourceLoadComplete(
      navigation_id, resource_load_info);
}

void LoadingPredictorTabHelper::DocumentOnLoadCompletedInMainFrame() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!predictor_)
    return;

  auto navigation_id = NavigationID(web_contents());
  if (!navigation_id.is_valid())
    return;

  predictor_->loading_data_collector()->RecordMainFrameLoadComplete(
      navigation_id, last_optimization_guide_prediction_);

  // Clear out Optimization Guide Prediction, as it is no longer needed.
  last_optimization_guide_prediction_ = base::nullopt;
}

void LoadingPredictorTabHelper::OnOptimizationGuideDecision(
    const NavigationID& navigation_id,
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(navigation_id.is_valid());

  if (!predictor_)
    return;

  ScopedOptimizationHintsReceiveStatusRecorder recorder;

  if (current_navigation_id_.is_valid() &&
      current_navigation_id_ != navigation_id) {
    // The current navigation has either redirected or a new one has started, so
    // return.
    recorder.set_status(
        OptimizationHintsReceiveStatus::kAfterRedirectOrNextNavigationStart);
    return;
  }
  if (!current_navigation_id_.is_valid()) {
    // There is not a pending navigation.
    recorder.set_status(OptimizationHintsReceiveStatus::kAfterNavigationFinish);

    auto last_committed_navigation_id = NavigationID(web_contents());
    if (!last_committed_navigation_id.is_valid() ||
        last_committed_navigation_id != navigation_id) {
      // This hint is no longer relevant, so return.
      return;
    }

    // If we get here, we have not navigated away from the navigation we
    // received hints for. Proceed to get the preconnect prediction so we can
    // see how the predicted resources match what was actually fetched for
    // counterfactual logging purposes.
  } else {
    recorder.set_status(
        OptimizationHintsReceiveStatus::kBeforeNavigationFinish);
  }

  if (!last_optimization_guide_prediction_) {
    // Data for the navigation has already been recorded, do not proceed any
    // further, even for counterfactual logging.
    return;
  }

  last_optimization_guide_prediction_->decision = decision;

  if (decision != optimization_guide::OptimizationGuideDecision::kTrue)
    return;

  if (!metadata.loading_predictor_metadata()) {
    // Metadata is not applicable, so just log an unknown decision.
    last_optimization_guide_prediction_->decision =
        optimization_guide::OptimizationGuideDecision::kUnknown;
    return;
  }

  PreconnectPrediction prediction;
  url::Origin main_frame_origin =
      url::Origin::Create(navigation_id.main_frame_url);
  net::NetworkIsolationKey network_isolation_key(main_frame_origin,
                                                 main_frame_origin);
  std::set<url::Origin> predicted_origins;
  std::vector<GURL> predicted_subresources;
  const auto lp_metadata = metadata.loading_predictor_metadata();
  for (const auto& subresource : lp_metadata->subresources()) {
    GURL subresource_url(subresource.url());
    if (!subresource_url.is_valid())
      continue;
    predicted_subresources.push_back(subresource_url);
    url::Origin subresource_origin = url::Origin::Create(subresource_url);
    if (predicted_origins.find(subresource_origin) != predicted_origins.end())
      continue;
    predicted_origins.insert(subresource_origin);
    prediction.requests.emplace_back(subresource_origin, 1,
                                     network_isolation_key);
  }

  last_optimization_guide_prediction_->preconnect_prediction = prediction;
  last_optimization_guide_prediction_->predicted_subresources =
      predicted_subresources;

  // Only preconnect if the navigation is still pending and we want to use the
  // predictions to preconnect to subresource origins.
  if (current_navigation_id_.is_valid() &&
      features::ShouldUseOptimizationGuidePredictionsToPreconnect()) {
    predictor_->PrepareForPageLoad(navigation_id.main_frame_url,
                                   HintOrigin::OPTIMIZATION_GUIDE,
                                   /*preconnectable=*/false, prediction);
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(LoadingPredictorTabHelper)

}  // namespace predictors
