// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/browser/media_router_metrics.h"

#include <algorithm>

#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/default_clock.h"
#include "components/media_router/common/media_route_provider_helper.h"
#include "components/media_router/common/media_sink.h"
#include "components/media_router/common/media_source.h"
#include "components/media_router/common/mojom/media_router.mojom.h"
#include "url/gurl.h"
#include "url/url_constants.h"

namespace media_router {

namespace {

constexpr char kHistogramProviderCreateRouteResult[] =
    "MediaRouter.Provider.CreateRoute.Result";
constexpr char kHistogramProviderJoinRouteResult[] =
    "MediaRouter.Provider.JoinRoute.Result";
constexpr char kHistogramProviderTerminateRouteResult[] =
    "MediaRouter.Provider.TerminateRoute.Result";

std::string GetHistogramNameForProvider(
    const std::string& base_name,
    absl::optional<mojom::MediaRouteProviderId> provider_id) {
  if (!provider_id) {
    return base_name;
  }
  switch (*provider_id) {
    case mojom::MediaRouteProviderId::CAST:
      return base_name + ".Cast";
    case mojom::MediaRouteProviderId::DIAL:
      return base_name + ".DIAL";
    case mojom::MediaRouteProviderId::WIRED_DISPLAY:
      return base_name + ".WiredDisplay";
    case mojom::MediaRouteProviderId::ANDROID_CAF:
      return base_name + ".AndroidCaf";
    // The rest use the base histogram name.
    case mojom::MediaRouteProviderId::TEST:
      return base_name;
  }
}

std::string GetUiName(UiType ui) {
  switch (ui) {
    case UiType::kCastDialog:
      return "CastHarmony";
    case UiType::kGlobalMediaControls:
      return "GlobalMediaControls";
  }
}

std::string GetDeviceCountHistogramName(const std::string& ui,
                                        MediaRouterDialogOpenOrigin origin,
                                        mojom::MediaRouteProviderId provider,
                                        bool is_available) {
  std::string trigger;
  switch (origin) {
    case MediaRouterDialogOpenOrigin::PAGE:
      trigger = "PresentationApi";
      break;
    default:
      trigger = "BrowserUi";
  }
  std::string mrp = ProviderIdToString(provider);
  std::string state = is_available ? "Available" : "Unavailable";
  return base::StrCat({MediaRouterMetrics::kHistogramUiDeviceCount, ".", ui,
                       ".", trigger, ".", mrp, ".", state});
}

PresentationUrlType GetPresentationUrlType(const GURL& url) {
  if (url.SchemeIs(kDialPresentationUrlScheme))
    return PresentationUrlType::kDial;
  if (url.SchemeIs(kCastPresentationUrlScheme))
    return PresentationUrlType::kCast;
  if (url.SchemeIs(kCastDialPresentationUrlScheme))
    return PresentationUrlType::kCastDial;
  if (url.SchemeIs(kRemotePlaybackPresentationUrlScheme))
    return PresentationUrlType::kRemotePlayback;
  if (base::StartsWith(url.spec(), kLegacyCastPresentationUrlPrefix,
                       base::CompareCase::INSENSITIVE_ASCII))
    return PresentationUrlType::kCastLegacy;
  if (url.SchemeIs(url::kHttpsScheme))
    return PresentationUrlType::kHttps;
  if (url.SchemeIs(url::kHttpScheme))
    return PresentationUrlType::kHttp;
  return PresentationUrlType::kOther;
}

}  // namespace

MediaRouterMetrics::MediaRouterMetrics() = default;
MediaRouterMetrics::~MediaRouterMetrics() = default;

// static
const char MediaRouterMetrics::kHistogramCloseLatency[] =
    "MediaRouter.Ui.Action.CloseLatency";
const char MediaRouterMetrics::kHistogramCloudPrefAtDialogOpen[] =
    "MediaRouter.Cloud.PrefAtDialogOpen";
const char MediaRouterMetrics::kHistogramCloudPrefAtInit[] =
    "MediaRouter.Cloud.PrefAtInit";
const char MediaRouterMetrics::kHistogramIconClickLocation[] =
    "MediaRouter.Icon.Click.Location";
const char MediaRouterMetrics::kHistogramMediaRouterFileFormat[] =
    "MediaRouter.Source.LocalFileFormat";
const char MediaRouterMetrics::kHistogramMediaRouterFileSize[] =
    "MediaRouter.Source.LocalFileSize";
const char MediaRouterMetrics::kHistogramMediaSinkType[] =
    "MediaRouter.Sink.SelectedType";
const char MediaRouterMetrics::kHistogramPresentationUrlType[] =
    "MediaRouter.PresentationRequest.AvailabilityUrlType";
const char MediaRouterMetrics::kHistogramRouteCreationOutcome[] =
    "MediaRouter.Route.CreationOutcome";
const char MediaRouterMetrics::kHistogramStartLocalLatency[] =
    "MediaRouter.Ui.Action.StartLocal.Latency";
const char MediaRouterMetrics::kHistogramStartLocalPosition[] =
    "MediaRouter.Ui.Action.StartLocalPosition";
const char MediaRouterMetrics::kHistogramStartLocalSessionSuccessful[] =
    "MediaRouter.Ui.Action.StartLocalSessionSuccessful";
const char MediaRouterMetrics::kHistogramStopRoute[] =
    "MediaRouter.Ui.Action.StopRoute";
const char MediaRouterMetrics::kHistogramUiDeviceCount[] =
    "MediaRouter.Ui.Device.Count";
const char MediaRouterMetrics::kHistogramUiDialogIconStateAtOpen[] =
    "MediaRouter.Ui.Dialog.IconStateAtOpen";
const char MediaRouterMetrics::kHistogramUiDialogLoadedWithData[] =
    "MediaRouter.Ui.Dialog.LoadedWithData";
const char MediaRouterMetrics::kHistogramUiDialogPaint[] =
    "MediaRouter.Ui.Dialog.Paint";
const char MediaRouterMetrics::kHistogramUiFirstAction[] =
    "MediaRouter.Ui.FirstAction";
const char MediaRouterMetrics::kHistogramUiIconStateAtInit[] =
    "MediaRouter.Ui.IconStateAtInit";

// static
const base::TimeDelta MediaRouterMetrics::kDeviceCountMetricDelay =
    base::Seconds(3);

// static
void MediaRouterMetrics::RecordMediaRouterDialogOrigin(
    MediaRouterDialogOpenOrigin origin) {
  DCHECK_LT(static_cast<int>(origin),
            static_cast<int>(MediaRouterDialogOpenOrigin::TOTAL_COUNT));
  UMA_HISTOGRAM_ENUMERATION(
      kHistogramIconClickLocation, static_cast<int>(origin),
      static_cast<int>(MediaRouterDialogOpenOrigin::TOTAL_COUNT));
}

// static
void MediaRouterMetrics::RecordMediaRouterDialogPaint(
    const base::TimeDelta& delta) {
  UMA_HISTOGRAM_TIMES(kHistogramUiDialogPaint, delta);
}

// static
void MediaRouterMetrics::RecordMediaRouterDialogLoaded(
    const base::TimeDelta& delta) {
  UMA_HISTOGRAM_TIMES(kHistogramUiDialogLoadedWithData, delta);
}

// static
void MediaRouterMetrics::RecordCloseDialogLatency(
    const base::TimeDelta& delta) {
  UMA_HISTOGRAM_TIMES(kHistogramCloseLatency, delta);
}

// static
void MediaRouterMetrics::RecordMediaRouterInitialUserAction(
    MediaRouterUserAction action) {
  DCHECK_LT(static_cast<int>(action),
            static_cast<int>(MediaRouterUserAction::TOTAL_COUNT));
  UMA_HISTOGRAM_ENUMERATION(
      kHistogramUiFirstAction, static_cast<int>(action),
      static_cast<int>(MediaRouterUserAction::TOTAL_COUNT));
}

// static
void MediaRouterMetrics::RecordRouteCreationOutcome(
    MediaRouterRouteCreationOutcome outcome) {
  DCHECK_LT(static_cast<int>(outcome),
            static_cast<int>(MediaRouterRouteCreationOutcome::TOTAL_COUNT));
  UMA_HISTOGRAM_ENUMERATION(
      kHistogramRouteCreationOutcome, static_cast<int>(outcome),
      static_cast<int>(MediaRouterRouteCreationOutcome::TOTAL_COUNT));
}

// static
void MediaRouterMetrics::RecordMediaRouterFileFormat(
    const media::container_names::MediaContainerName format) {
  UMA_HISTOGRAM_ENUMERATION(kHistogramMediaRouterFileFormat, format,
                            media::container_names::CONTAINER_MAX + 1);
}

// static
void MediaRouterMetrics::RecordMediaRouterFileSize(int64_t size) {
  UMA_HISTOGRAM_MEMORY_LARGE_MB(kHistogramMediaRouterFileSize, size);
}

// static
void MediaRouterMetrics::RecordPresentationUrlType(const GURL& url) {
  PresentationUrlType type = GetPresentationUrlType(url);
  UMA_HISTOGRAM_ENUMERATION(kHistogramPresentationUrlType, type,
                            PresentationUrlType::kPresentationUrlTypeCount);
}

// static
void MediaRouterMetrics::RecordMediaSinkType(SinkIconType sink_icon_type) {
  UMA_HISTOGRAM_ENUMERATION(kHistogramMediaSinkType, sink_icon_type,
                            SinkIconType::TOTAL_COUNT);
}

// static
void MediaRouterMetrics::RecordMediaSinkTypeForGlobalMediaControls(
    SinkIconType sink_icon_type) {
  UMA_HISTOGRAM_ENUMERATION(
      base::StrCat({kHistogramMediaSinkType, ".GlobalMediaControls"}),
      sink_icon_type, SinkIconType::TOTAL_COUNT);
}

// static
void MediaRouterMetrics::RecordMediaSinkTypeForCastDialog(
    SinkIconType sink_icon_type) {
  UMA_HISTOGRAM_ENUMERATION(
      base::StrCat({kHistogramMediaSinkType, ".CastHarmony"}), sink_icon_type,
      SinkIconType::TOTAL_COUNT);
}

// static
void MediaRouterMetrics::RecordMediaSinkTypeWhenCastAndDialPresent(
    SinkIconType sink_icon_type,
    UiType ui) {
  UMA_HISTOGRAM_ENUMERATION(
      base::StrCat(
          {kHistogramMediaSinkType, ".CastAndDialPresent.", GetUiName(ui)}),
      sink_icon_type, SinkIconType::TOTAL_COUNT);
}

// static
void MediaRouterMetrics::RecordDeviceCount(int device_count) {
  UMA_HISTOGRAM_COUNTS_100(kHistogramUiDeviceCount, device_count);
}

// static
void MediaRouterMetrics::RecordGmcDeviceCount(
    MediaRouterDialogOpenOrigin origin,
    mojom::MediaRouteProviderId provider,
    bool is_available,
    int count) {
  base::UmaHistogramCounts100(
      GetDeviceCountHistogramName("GlobalMediaControls", origin, provider,
                                  is_available),
      count);
}

// static
void MediaRouterMetrics::RecordCastDialogDeviceCount(
    MediaRouterDialogOpenOrigin origin,
    mojom::MediaRouteProviderId provider,
    bool is_available,
    int count) {
  base::UmaHistogramCounts100(
      GetDeviceCountHistogramName("CastHarmony", origin, provider,
                                  is_available),
      count);
}

// static
void MediaRouterMetrics::RecordStartRouteDeviceIndex(int index) {
  base::UmaHistogramSparse(kHistogramStartLocalPosition, std::min(index, 100));
}

// static
void MediaRouterMetrics::RecordStartLocalSessionLatency(
    const base::TimeDelta& delta) {
  UMA_HISTOGRAM_TIMES(kHistogramStartLocalLatency, delta);
}

// static
void MediaRouterMetrics::RecordStartLocalSessionSuccessful(bool success) {
  UMA_HISTOGRAM_BOOLEAN(kHistogramStartLocalSessionSuccessful, success);
}

// static
void MediaRouterMetrics::RecordStopLocalRoute() {
  // Local routes have the enum value 0.
  UMA_HISTOGRAM_BOOLEAN(kHistogramStopRoute, 0);
}

// static
void MediaRouterMetrics::RecordStopRemoteRoute() {
  // Remote routes have the enum value 1.
  UMA_HISTOGRAM_BOOLEAN(kHistogramStopRoute, 1);
}

// static
void MediaRouterMetrics::RecordIconStateAtDialogOpen(bool is_pinned) {
  UMA_HISTOGRAM_BOOLEAN(kHistogramUiDialogIconStateAtOpen, is_pinned);
}

// static
void MediaRouterMetrics::RecordIconStateAtInit(bool is_pinned) {
  // Since this gets called only rarely, use base::UmaHistogramBoolean() to
  // avoid instantiating the caching code.
  base::UmaHistogramBoolean(kHistogramUiIconStateAtInit, is_pinned);
}

// static
void MediaRouterMetrics::RecordCloudPrefAtDialogOpen(bool enabled) {
  base::UmaHistogramBoolean(kHistogramCloudPrefAtDialogOpen, enabled);
}

// static
void MediaRouterMetrics::RecordCloudPrefAtInit(bool enabled) {
  base::UmaHistogramBoolean(kHistogramCloudPrefAtInit, enabled);
}

// static
void MediaRouterMetrics::RecordCreateRouteResultCode(
    RouteRequestResult::ResultCode result_code,
    absl::optional<mojom::MediaRouteProviderId> provider_id) {
  DCHECK_LT(result_code, RouteRequestResult::TOTAL_COUNT);
  base::UmaHistogramEnumeration(
      GetHistogramNameForProvider(kHistogramProviderCreateRouteResult,
                                  provider_id),
      result_code, RouteRequestResult::TOTAL_COUNT);
}

// static
void MediaRouterMetrics::RecordJoinRouteResultCode(
    RouteRequestResult::ResultCode result_code,
    absl::optional<mojom::MediaRouteProviderId> provider_id) {
  DCHECK_LT(result_code, RouteRequestResult::ResultCode::TOTAL_COUNT);
  base::UmaHistogramEnumeration(
      GetHistogramNameForProvider(kHistogramProviderJoinRouteResult,
                                  provider_id),
      result_code, RouteRequestResult::TOTAL_COUNT);
}

// static
void MediaRouterMetrics::RecordMediaRouteProviderTerminateRoute(
    RouteRequestResult::ResultCode result_code,
    absl::optional<mojom::MediaRouteProviderId> provider_id) {
  DCHECK_LT(result_code, RouteRequestResult::ResultCode::TOTAL_COUNT);
  base::UmaHistogramEnumeration(
      GetHistogramNameForProvider(kHistogramProviderTerminateRouteResult,
                                  provider_id),
      result_code, RouteRequestResult::TOTAL_COUNT);
}

}  // namespace media_router
