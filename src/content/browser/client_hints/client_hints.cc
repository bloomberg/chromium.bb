// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/client_hints/client_hints.h"

#include <algorithm>
#include <string>

#include "base/command_line.h"
#include "base/containers/contains.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "content/browser/devtools/devtools_instrumentation.h"
#include "content/browser/renderer_host/frame_tree.h"
#include "content/browser/renderer_host/frame_tree_node.h"
#include "content/browser/renderer_host/navigator.h"
#include "content/browser/renderer_host/navigator_delegate.h"
#include "content/browser/renderer_host/render_view_host_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/client_hints_controller_delegate.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/host_zoom_map.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_switches.h"
#include "net/base/url_util.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/http/structured_headers.h"
#include "net/nqe/effective_connection_type.h"
#include "net/nqe/network_quality_estimator_params.h"
#include "services/network/public/cpp/client_hints.h"
#include "services/network/public/cpp/is_potentially_trustworthy.h"
#include "services/network/public/cpp/network_quality_tracker.h"
#include "services/network/public/mojom/web_client_hints_types.mojom-shared.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/common/client_hints/client_hints.h"
#include "third_party/blink/public/common/client_hints/enabled_client_hints.h"
#include "third_party/blink/public/common/device_memory/approximated_device_memory.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "third_party/blink/public/common/permissions_policy/permissions_policy.h"
#include "third_party/blink/public/common/user_agent/user_agent_metadata.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "url/origin.h"
#include "url/url_constants.h"

namespace content {

namespace {
using ::network::mojom::WebClientHintsType;

uint8_t randomization_salt = 0;

constexpr size_t kMaxRandomNumbers = 21;

// Returns the randomization salt (weak and insecure) that should be used when
// adding noise to the network quality metrics. This is known only to the
// device, and is generated only once. This makes it possible to add the same
// amount of noise for a given origin.
uint8_t RandomizationSalt() {
  if (randomization_salt == 0)
    randomization_salt = base::RandInt(1, kMaxRandomNumbers);
  DCHECK_LE(1, randomization_salt);
  DCHECK_GE(kMaxRandomNumbers, randomization_salt);
  return randomization_salt;
}

double GetRandomMultiplier(const std::string& host) {
  // The random number should be a function of the hostname to reduce
  // cross-origin fingerprinting. The random number should also be a function
  // of randomized salt which is known only to the device. This prevents
  // origin from removing noise from the estimates.
  unsigned hash = std::hash<std::string>{}(host) + RandomizationSalt();
  double random_multiplier =
      0.9 + static_cast<double>((hash % kMaxRandomNumbers)) * 0.01;
  DCHECK_LE(0.90, random_multiplier);
  DCHECK_GE(1.10, random_multiplier);
  return random_multiplier;
}

unsigned long RoundRtt(const std::string& host,
                       const absl::optional<base::TimeDelta>& rtt) {
  if (!rtt.has_value()) {
    // RTT is unavailable. So, return the fastest value.
    return 0;
  }

  // Limit the maximum reported value and the granularity to reduce
  // fingerprinting.
  constexpr base::TimeDelta kMaxRtt = base::Seconds(3);
  constexpr base::TimeDelta kGranularity = base::Milliseconds(50);

  const base::TimeDelta modified_rtt =
      std::min(rtt.value() * GetRandomMultiplier(host), kMaxRtt);
  DCHECK_GE(modified_rtt, base::TimeDelta());
  return modified_rtt.RoundToMultiple(kGranularity).InMilliseconds();
}

double RoundKbpsToMbps(const std::string& host,
                       const absl::optional<int32_t>& downlink_kbps) {
  // Limit the size of the buckets and the maximum reported value to reduce
  // fingerprinting.
  static const size_t kGranularityKbps = 50;
  static const double kMaxDownlinkKbps = 10.0 * 1000;

  // If downlink is unavailable, return the fastest value.
  double randomized_downlink_kbps = downlink_kbps.value_or(kMaxDownlinkKbps);
  randomized_downlink_kbps *= GetRandomMultiplier(host);

  randomized_downlink_kbps =
      std::min(randomized_downlink_kbps, kMaxDownlinkKbps);

  DCHECK_LE(0, randomized_downlink_kbps);
  DCHECK_GE(kMaxDownlinkKbps, randomized_downlink_kbps);
  // Round down to the nearest kGranularityKbps kbps value.
  double downlink_kbps_rounded =
      std::round(randomized_downlink_kbps / kGranularityKbps) *
      kGranularityKbps;

  // Convert from Kbps to Mbps.
  return downlink_kbps_rounded / 1000;
}

double GetDeviceScaleFactor() {
  double device_scale_factor = 1.0;
  if (display::Screen::GetScreen()) {
    device_scale_factor =
        display::Screen::GetScreen()->GetPrimaryDisplay().device_scale_factor();
  }
  DCHECK_LT(0.0, device_scale_factor);
  return device_scale_factor;
}

// Returns the zoom factor for a given |url|.
double GetZoomFactor(BrowserContext* context, const GURL& url) {
#if BUILDFLAG(IS_ANDROID)
  // On Android, use the default value when the AccessibilityPageZoom
  // feature is not enabled.
  if (!base::FeatureList::IsEnabled(features::kAccessibilityPageZoom))
    return 1.0;
#endif

  double zoom_level = HostZoomMap::GetDefaultForBrowserContext(context)
                          ->GetZoomLevelForHostAndScheme(
                              url.scheme(), net::GetHostOrSpecFromURL(url));

  if (zoom_level == 0.0) {
    // Get default zoom level.
    zoom_level = HostZoomMap::GetDefaultForBrowserContext(context)
                     ->GetDefaultZoomLevel();
  }

  return blink::PageZoomLevelToZoomFactor(zoom_level);
}

// Returns a string corresponding to |value|. The returned string satisfies
// ABNF: 1*DIGIT [ "." 1*DIGIT ]
std::string DoubleToSpecCompliantString(double value) {
  DCHECK_LE(0.0, value);
  std::string result = base::NumberToString(value);
  DCHECK(!result.empty());
  if (value >= 1.0)
    return result;

  DCHECK_LE(0.0, value);
  DCHECK_GT(1.0, value);

  // Check if there is at least one character before period.
  if (result.at(0) != '.')
    return result;

  // '.' is the first character in |result|. Prefix one digit before the
  // period to make it spec compliant.
  return "0" + result;
}

// Return the effective connection type value overridden for web APIs.
// If no override value has been set, a null value is returned.
absl::optional<net::EffectiveConnectionType>
GetWebHoldbackEffectiveConnectionType() {
  if (!base::FeatureList::IsEnabled(
          features::kNetworkQualityEstimatorWebHoldback)) {
    return absl::nullopt;
  }
  std::string effective_connection_type_param =
      base::GetFieldTrialParamValueByFeature(
          features::kNetworkQualityEstimatorWebHoldback,
          "web_effective_connection_type_override");

  absl::optional<net::EffectiveConnectionType> effective_connection_type =
      net::GetEffectiveConnectionTypeForName(effective_connection_type_param);
  DCHECK(effective_connection_type_param.empty() || effective_connection_type);

  if (!effective_connection_type)
    return absl::nullopt;
  DCHECK_NE(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN,
            effective_connection_type.value());
  return effective_connection_type;
}

void SetHeaderToDouble(net::HttpRequestHeaders* headers,
                       WebClientHintsType client_hint_type,
                       double value) {
  headers->SetHeader(network::GetClientHintToNameMap().at(client_hint_type),
                     DoubleToSpecCompliantString(value));
}

void SetHeaderToInt(net::HttpRequestHeaders* headers,
                    WebClientHintsType client_hint_type,
                    double value) {
  headers->SetHeader(network::GetClientHintToNameMap().at(client_hint_type),
                     base::NumberToString(std::round(value)));
}

void SetHeaderToString(net::HttpRequestHeaders* headers,
                       WebClientHintsType client_hint_type,
                       const std::string& value) {
  headers->SetHeader(network::GetClientHintToNameMap().at(client_hint_type),
                     value);
}

void RemoveClientHintHeader(WebClientHintsType client_hint_type,
                            net::HttpRequestHeaders* headers) {
  headers->RemoveHeader(network::GetClientHintToNameMap().at(client_hint_type));
}

void AddDeviceMemoryHeader(net::HttpRequestHeaders* headers,
                           bool use_deprecated_version = false) {
  DCHECK(headers);
  blink::ApproximatedDeviceMemory::Initialize();
  const float device_memory =
      blink::ApproximatedDeviceMemory::GetApproximatedDeviceMemory();
  DCHECK_LT(0.0, device_memory);
  SetHeaderToDouble(headers,
                    use_deprecated_version
                        ? WebClientHintsType::kDeviceMemory_DEPRECATED
                        : WebClientHintsType::kDeviceMemory,
                    device_memory);
}

void AddDPRHeader(net::HttpRequestHeaders* headers,
                  BrowserContext* context,
                  const GURL& url,
                  bool use_deprecated_version = false) {
  DCHECK(headers);
  DCHECK(context);
  double device_scale_factor = GetDeviceScaleFactor();
  double zoom_factor = GetZoomFactor(context, url);
  SetHeaderToDouble(headers,
                    use_deprecated_version ? WebClientHintsType::kDpr_DEPRECATED
                                           : WebClientHintsType::kDpr,
                    device_scale_factor * zoom_factor);
}

void AddSaveDataHeader(net::HttpRequestHeaders* headers,
                       BrowserContext* context) {
  DCHECK(headers);
  DCHECK(context);
  // Unlike other client hints, this one is only sent when it has a value.
  if (GetContentClient()->browser()->IsDataSaverEnabled(context))
    SetHeaderToString(headers, WebClientHintsType::kSaveData, "on");
}

void AddViewportWidthHeader(net::HttpRequestHeaders* headers,
                            BrowserContext* context,
                            const GURL& url,
                            bool use_deprecated_version = false) {
  DCHECK(headers);
  DCHECK(context);
  // The default value on Android. See
  // https://cs.chromium.org/chromium/src/third_party/WebKit/Source/core/css/viewportAndroid.css.
  double viewport_width = 980;

#if BUILDFLAG(IS_ANDROID)
  // On Android, use the default value when the AccessibilityPageZoom
  // feature is not enabled.
  if (!base::FeatureList::IsEnabled(features::kAccessibilityPageZoom)) {
    SetHeaderToInt(headers,
                   use_deprecated_version
                       ? WebClientHintsType::kViewportWidth_DEPRECATED
                       : WebClientHintsType::kViewportWidth,
                   viewport_width);
    return;
  }
#endif

  double device_scale_factor = GetDeviceScaleFactor();
  viewport_width = (display::Screen::GetScreen()
                        ->GetPrimaryDisplay()
                        .GetSizeInPixel()
                        .width()) /
                   GetZoomFactor(context, url) / device_scale_factor;
  DCHECK_LT(0, viewport_width);
  // TODO(yoav): Find out why this 0 check is needed...
  if (viewport_width > 0) {
    SetHeaderToInt(headers,
                   use_deprecated_version
                       ? WebClientHintsType::kViewportWidth_DEPRECATED
                       : WebClientHintsType::kViewportWidth,
                   viewport_width);
  }
}

void AddViewportHeightHeader(net::HttpRequestHeaders* headers,
                             BrowserContext* context,
                             const GURL& url) {
  DCHECK(headers);
  DCHECK(context);

  double overall_scale_factor =
      GetZoomFactor(context, url) * GetDeviceScaleFactor();
  double viewport_height = (display::Screen::GetScreen()
                                ->GetPrimaryDisplay()
                                .GetSizeInPixel()
                                .height()) /
                           overall_scale_factor;
#if BUILDFLAG(IS_ANDROID)
  // On Android, the viewport is scaled so the width is 980 and the height
  // maintains the same ratio.
  // TODO(1246208): Improve the usefulness of the viewport client hints for
  // navigation requests.
  double viewport_width = (display::Screen::GetScreen()
                               ->GetPrimaryDisplay()
                               .GetSizeInPixel()
                               .width()) /
                          overall_scale_factor;
  viewport_height *= 980.0 / viewport_width;
#endif  // BUILDFLAG(IS_ANDROID)

  DCHECK_LT(0, viewport_height);

  SetHeaderToInt(headers, network::mojom::WebClientHintsType::kViewportHeight,
                 viewport_height);
}

void AddRttHeader(net::HttpRequestHeaders* headers,
                  network::NetworkQualityTracker* network_quality_tracker,
                  const GURL& url) {
  DCHECK(headers);

  absl::optional<net::EffectiveConnectionType> web_holdback_ect =
      GetWebHoldbackEffectiveConnectionType();

  base::TimeDelta http_rtt;
  if (web_holdback_ect.has_value()) {
    http_rtt = net::NetworkQualityEstimatorParams::GetDefaultTypicalHttpRtt(
        web_holdback_ect.value());
  } else if (network_quality_tracker) {
    http_rtt = network_quality_tracker->GetHttpRTT();
  } else {
    http_rtt = net::NetworkQualityEstimatorParams::GetDefaultTypicalHttpRtt(
        net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
  }
  SetHeaderToInt(headers, WebClientHintsType::kRtt_DEPRECATED,
                 RoundRtt(url.host(), http_rtt));
}

void AddDownlinkHeader(net::HttpRequestHeaders* headers,
                       network::NetworkQualityTracker* network_quality_tracker,
                       const GURL& url) {
  DCHECK(headers);
  absl::optional<net::EffectiveConnectionType> web_holdback_ect =
      GetWebHoldbackEffectiveConnectionType();

  int32_t downlink_throughput_kbps;

  if (web_holdback_ect.has_value()) {
    downlink_throughput_kbps =
        net::NetworkQualityEstimatorParams::GetDefaultTypicalDownlinkKbps(
            web_holdback_ect.value());
  } else if (network_quality_tracker) {
    downlink_throughput_kbps =
        network_quality_tracker->GetDownstreamThroughputKbps();
  } else {
    downlink_throughput_kbps =
        net::NetworkQualityEstimatorParams::GetDefaultTypicalDownlinkKbps(
            net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
  }

  SetHeaderToDouble(headers, WebClientHintsType::kDownlink_DEPRECATED,
                    RoundKbpsToMbps(url.host(), downlink_throughput_kbps));
}

void AddEctHeader(net::HttpRequestHeaders* headers,
                  network::NetworkQualityTracker* network_quality_tracker,
                  const GURL& url) {
  DCHECK(headers);
  DCHECK_EQ(blink::kWebEffectiveConnectionTypeMappingCount,
            net::EFFECTIVE_CONNECTION_TYPE_4G + 1u);
  DCHECK_EQ(blink::kWebEffectiveConnectionTypeMappingCount,
            static_cast<size_t>(net::EFFECTIVE_CONNECTION_TYPE_LAST));

  absl::optional<net::EffectiveConnectionType> web_holdback_ect =
      GetWebHoldbackEffectiveConnectionType();

  int effective_connection_type;
  if (web_holdback_ect.has_value()) {
    effective_connection_type = web_holdback_ect.value();
  } else if (network_quality_tracker) {
    effective_connection_type =
        static_cast<int>(network_quality_tracker->GetEffectiveConnectionType());
  } else {
    effective_connection_type =
        static_cast<int>(net::EFFECTIVE_CONNECTION_TYPE_UNKNOWN);
  }

  SetHeaderToString(
      headers, WebClientHintsType::kEct_DEPRECATED,
      blink::kWebEffectiveConnectionTypeMapping[effective_connection_type]);
}

void AddPrefersColorSchemeHeader(net::HttpRequestHeaders* headers,
                                 FrameTreeNode* frame_tree_node) {
  if (!frame_tree_node)
    return;
  blink::mojom::PreferredColorScheme preferred_color_scheme =
      frame_tree_node->current_frame_host()->GetPreferredColorScheme();
  bool is_dark_mode =
      preferred_color_scheme == blink::mojom::PreferredColorScheme::kDark;
  SetHeaderToString(headers, WebClientHintsType::kPrefersColorScheme,
                    is_dark_mode ? "dark" : "light");
}

bool IsValidURLForClientHints(const url::Origin& origin) {
  return network::IsOriginPotentiallyTrustworthy(origin);
}

bool UserAgentClientHintEnabled() {
  return base::FeatureList::IsEnabled(blink::features::kUserAgentClientHint);
}

void AddUAHeader(net::HttpRequestHeaders* headers,
                 WebClientHintsType type,
                 const std::string& value) {
  SetHeaderToString(headers, type, value);
}

// Creates a serialized string header value out of the input type, using
// structured headers as described in
// https://www.rfc-editor.org/rfc/rfc8941.html.
template <typename T>
const std::string SerializeHeaderString(const T& value) {
  return net::structured_headers::SerializeItem(
             net::structured_headers::Item(value))
      .value_or(std::string());
}

// Returns true iff the `url` is embedded inside a frame that has the
// corresponding Sec-CH-UA-Reduced or Sec-CH-UA-Full client hint and thus, is
// enrolled in the UserAgentReduction or SendFullUserAgentAfterReduction
// Origin Trial.
//
// TODO(crbug.com/1258063): Remove when the UserAgentReduction and
// SendFullUserAgentAfterReduction Origin Trial is finished.
bool IsOriginTrialHintEnabledForFrame(
    const url::Origin& origin,
    const url::Origin& outermost_main_frame_origin,
    FrameTreeNode* frame_tree_node,
    ClientHintsControllerDelegate* delegate,
    WebClientHintsType hint_type) {
  // TODO(crbug.com/1300194): Refactor away from the use of FrameTreeNode
  RenderFrameHostImpl* current = frame_tree_node->current_frame_host();
  while (current) {
    const url::Origin& current_origin =
        (current == frame_tree_node->current_frame_host())
            ? origin
            : current->GetLastCommittedOrigin();

    // Don't use Sec-CH-UA-Reduced or Sec-CH-UA-Full from third-party origins if
    // third-party cookies are blocked, so that we don't reveal any more user
    // data than is allowed by the cookie settings.
    if (outermost_main_frame_origin.IsSameOriginWith(current_origin) ||
        !delegate->AreThirdPartyCookiesBlocked(current_origin.GetURL())) {
      blink::EnabledClientHints current_url_hints;
      delegate->GetAllowedClientHintsFromSource(current_origin,
                                                &current_url_hints);
      if (base::Contains(current_url_hints.GetEnabledHints(), hint_type))
        return true;
    }

    current = current->GetParent();
  }
  return false;
}

// TODO(crbug.com/1258063): Delete this function when the UserAgentReduction and
// SendFullUserAgentAfterReduction Origin Trial is finished.
void RemoveAllClientHintsExceptOriginTrialHints(
    const url::Origin& origin,
    FrameTreeNode* frame_tree_node,
    ClientHintsControllerDelegate* delegate,
    std::vector<WebClientHintsType>* accept_ch,
    url::Origin* outermost_main_frame_origin,
    absl::optional<url::Origin>* third_party_origin) {
  RenderFrameHostImpl* outermost_main_frame =
      frame_tree_node->frame_tree()->GetMainFrame()->GetOutermostMainFrame();

  for (auto it = accept_ch->begin(); it != accept_ch->end();) {
    if (*it == WebClientHintsType::kUAReduced ||
        *it == WebClientHintsType::kFullUserAgent) {
      ++it;
    } else {
      it = accept_ch->erase(it);
    }
  }

  if (!outermost_main_frame->GetLastCommittedOrigin().IsSameOriginWith(
          origin)) {
    // If third-party cookeis are blocked, we will not persist the
    // Sec-CH-UA-Reduced client hint in a third-party context.
    if (delegate->AreThirdPartyCookiesBlocked(origin.GetURL())) {
      accept_ch->clear();
      return;
    }
    // Third-party contexts need the correct main frame URL and third-party
    // URL in order to validate the Origin Trial token correctly, if present.
    *outermost_main_frame_origin =
        outermost_main_frame->GetLastCommittedOrigin();
    *third_party_origin = absl::make_optional(origin);
  }
}

// Captures the state used in applying client hints.
struct ClientHintsExtendedData {
  ClientHintsExtendedData(const url::Origin& origin,
                          FrameTreeNode* frame_tree_node,
                          ClientHintsControllerDelegate* delegate,
                          const absl::optional<GURL>& maybe_request_url)
      : resource_origin(origin) {
    // If the current frame is the outermost main frame, the URL wasn't
    // committed yet, so in order to get the main frame URL, we should use the
    // provided URL instead. Otherwise, the current frame is a subframe and the
    // outermost main frame URL was committed, so we can safely get it from it.
    // Similarly, an in-navigation outermost main frame doesn't yet have a
    // permissions policy.
    is_outermost_main_frame =
        !frame_tree_node || frame_tree_node->IsOutermostMainFrame();
    if (is_outermost_main_frame) {
      outermost_main_frame_origin = resource_origin;
      is_1p_origin = true;
    } else if (frame_tree_node->IsInFencedFrameTree()) {
      permissions_policy =
          blink::PermissionsPolicy::CreateForFencedFrame(resource_origin);
    } else {
      RenderFrameHostImpl* outermost_main_frame = frame_tree_node->frame_tree()
                                                      ->GetMainFrame()
                                                      ->GetOutermostMainFrame();
      outermost_main_frame_origin =
          outermost_main_frame->GetLastCommittedOrigin();
      permissions_policy = blink::PermissionsPolicy::CopyStateFrom(
          outermost_main_frame->permissions_policy());
      is_1p_origin = resource_origin.IsSameOriginWith(
          outermost_main_frame->GetLastCommittedOrigin());
    }

    const base::TimeTicks start_time = base::TimeTicks::Now();
    delegate->GetAllowedClientHintsFromSource(outermost_main_frame_origin,
                                              &hints);

    // If this is not a top-level frame, then check if any of the ancestors
    // in the path that led to this request have Sec-CH-UA-Reduced set.
    // TODO(crbug.com/1258063): Remove once the UserAgentReduction Origin Trial
    // is finished.
    if (frame_tree_node && !is_outermost_main_frame) {
      url::Origin trial_origin =
          maybe_request_url ? url::Origin::Create(maybe_request_url.value())
                            : origin;

      is_embedder_ua_reduced = IsOriginTrialHintEnabledForFrame(
          trial_origin, outermost_main_frame_origin, frame_tree_node, delegate,
          WebClientHintsType::kUAReduced);
      is_embedder_ua_full = IsOriginTrialHintEnabledForFrame(
          trial_origin, outermost_main_frame_origin, frame_tree_node, delegate,
          WebClientHintsType::kFullUserAgent);
    }

    // Record the time spent getting the client hints.
    base::TimeDelta duration = base::TimeTicks::Now() - start_time;
    base::UmaHistogramTimes("ClientHints.FetchLatency", duration);
  }

  blink::EnabledClientHints hints;
  // If true, one of the ancestor requests in the path to this request had
  // Sec-CH-UA-Reduced in their Accept-CH cache.  Only applies to embedded
  // requests (top-level requests will always set this to false).
  //
  // If an embedder of a request has Sec-CH-UA-Reduced, it means it will
  // receive the reduced User-Agent header, so we want to also send the reduced
  // User-Agent for the embedded request as well.
  bool is_embedder_ua_reduced = false;
  // If true, one of the ancestor requests in the path to this request had
  // Sec-CH-UA-Full in their Accept-CH cache.  Only applies to embedded
  // requests (top-level requests will always set this to false).
  //
  // If an embedder of a request has Sec-CH-UA-Full, it means it will
  // receive the full User-Agent header, so we want to also send the full
  // User-Agent for the embedded request as well.
  bool is_embedder_ua_full = false;
  url::Origin resource_origin;
  bool is_outermost_main_frame = false;
  url::Origin outermost_main_frame_origin;
  std::unique_ptr<blink::PermissionsPolicy> permissions_policy;
  bool is_1p_origin = false;
};

bool SkipPermissionPolicyCheck(WebClientHintsType type) {
  return type == WebClientHintsType::kUAReduced ||
         type == WebClientHintsType::kFullUserAgent;
}

bool IsClientHintEnabled(const ClientHintsExtendedData& data,
                         WebClientHintsType type) {
  return blink::IsClientHintSentByDefault(type) || data.hints.IsEnabled(type) ||
         (type == WebClientHintsType::kUAReduced &&
          data.is_embedder_ua_reduced) ||
         (type == WebClientHintsType::kFullUserAgent &&
          data.is_embedder_ua_full);
}

bool IsClientHintAllowed(const ClientHintsExtendedData& data,
                         WebClientHintsType type) {
  if (data.is_outermost_main_frame) {
    return data.is_1p_origin;
  }
  return SkipPermissionPolicyCheck(type) ||
         (data.permissions_policy->IsFeatureEnabledForOrigin(
             blink::GetClientHintToPolicyFeatureMap().at(type),
             data.resource_origin));
}

bool ShouldAddClientHint(const ClientHintsExtendedData& data,
                         WebClientHintsType type) {
  return IsClientHintEnabled(data, type) && IsClientHintAllowed(data, type);
}

bool IsJavascriptEnabled(FrameTreeNode* frame_tree_node) {
  return WebContents::FromRenderFrameHost(frame_tree_node->current_frame_host())
      ->GetOrCreateWebPreferences()
      .javascript_enabled;
}

// This modifies `data.permissions_policy` to reflect any changes to client hint
// permissions which may have occurred via the named accept-ch meta tag.
// The permissions policy the browser side has for the frame was set in stone
// before HTML parsing began, so any updates must be sent via
// `container_policy`.
// TODO(crbug.com/1278127): Replace w/ generic HTML policy modification.
void UpdateIFramePermissionsPolicyWithDelegationSupportForClientHints(
    ClientHintsExtendedData& data,
    const blink::ParsedPermissionsPolicy& container_policy) {
  if (container_policy.empty() ||
      !base::FeatureList::IsEnabled(
          blink::features::kClientHintThirdPartyDelegation)) {
    return;
  }

  // For client hints specifically, we need to allow the container policy
  // to overwrite the parent policy so that permissions policies set in HTML
  // via an accept-ch meta tag can be respected.
  blink::ParsedPermissionsPolicy client_hints_container_policy;
  for (const auto& container_policy_item : container_policy) {
    const auto& it = blink::GetPolicyFeatureToClientHintMap().find(
        container_policy_item.feature);
    if (it != blink::GetPolicyFeatureToClientHintMap().end()) {
      client_hints_container_policy.push_back(container_policy_item);

      // We need to ensure `blink::EnabledClientHints` is updated where the
      // main frame now has permission for the given client hints.
      std::set<url::Origin> origin_set(
          container_policy_item.allowed_origins.begin(),
          container_policy_item.allowed_origins.end());
      if (origin_set.find(data.outermost_main_frame_origin) !=
          origin_set.end()) {
        for (const auto& hint : it->second) {
          data.hints.SetIsEnabled(hint, /*should_send*/ true);
        }
      }
    }
  }
  data.permissions_policy->OverwriteHeaderPolicyForClientHints(
      client_hints_container_policy);
}

// Captures when UpdateNavigationRequestClientUaHeadersImpl() is being called.
enum class ClientUaHeaderCallType {
  // The call is happening during creation of the NavigationRequest.
  kDuringCreation,

  // The call is happening after creation of the NavigationRequest.
  kAfterCreated,
};

// Implementation of UpdateNavigationRequestClientUaHeaders().
void UpdateNavigationRequestClientUaHeadersImpl(
    const url::Origin& origin,
    ClientHintsControllerDelegate* delegate,
    bool override_ua,
    FrameTreeNode* frame_tree_node,
    ClientUaHeaderCallType call_type,
    net::HttpRequestHeaders* headers,
    const blink::ParsedPermissionsPolicy& container_policy,
    const absl::optional<GURL>& request_url) {
  absl::optional<blink::UserAgentMetadata> ua_metadata;
  bool disable_due_to_custom_ua = false;
  if (override_ua) {
    NavigatorDelegate* nav_delegate =
        frame_tree_node ? frame_tree_node->navigator().GetDelegate() : nullptr;
    ua_metadata =
        nav_delegate ? nav_delegate->GetUserAgentOverride().ua_metadata_override
                     : absl::nullopt;
    // If a custom UA override is set, but no value is provided for UA client
    // hints, disable them.
    disable_due_to_custom_ua = !ua_metadata.has_value();
  }

  if (frame_tree_node &&
      devtools_instrumentation::ApplyUserAgentMetadataOverrides(frame_tree_node,
                                                                &ua_metadata)) {
    // Likewise, if devtools says to override client hints but provides no
    // value, disable them. This overwrites previous decision from UI.
    disable_due_to_custom_ua = !ua_metadata.has_value();
  }

  if (!disable_due_to_custom_ua) {
    if (!ua_metadata.has_value())
      ua_metadata = delegate->GetUserAgentMetadata();

    ClientHintsExtendedData data(origin, frame_tree_node, delegate,
                                 request_url);
    UpdateIFramePermissionsPolicyWithDelegationSupportForClientHints(
        data, container_policy);

    // The `Sec-CH-UA` client hint is attached to all outgoing requests. This is
    // (intentionally) different than other client hints.
    // It's barred behind ShouldAddClientHints to make sure it's controlled by
    // Permissions Policy.
    //
    // https://wicg.github.io/client-hints-infrastructure/#abstract-opdef-append-client-hints-to-request
    if (ShouldAddClientHint(data, WebClientHintsType::kUA)) {
      AddUAHeader(headers, WebClientHintsType::kUA,
                  ua_metadata->SerializeBrandMajorVersionList());
    }
    // The `Sec-CH-UA-Mobile client hint was also deemed "low entropy" and can
    // safely be sent with every request. Similarly to UA, ShouldAddClientHints
    // makes sure it's controlled by Permissions Policy.
    if (ShouldAddClientHint(data, WebClientHintsType::kUAMobile)) {
      AddUAHeader(headers, WebClientHintsType::kUAMobile,
                  SerializeHeaderString(ua_metadata->mobile));
    }

    if (ShouldAddClientHint(data, WebClientHintsType::kUAFullVersion)) {
      AddUAHeader(headers, WebClientHintsType::kUAFullVersion,
                  SerializeHeaderString(ua_metadata->full_version));
    }

    if (ShouldAddClientHint(data, WebClientHintsType::kUAArch)) {
      AddUAHeader(headers, WebClientHintsType::kUAArch,
                  SerializeHeaderString(ua_metadata->architecture));
    }

    if (ShouldAddClientHint(data, WebClientHintsType::kUAPlatform)) {
      AddUAHeader(headers, WebClientHintsType::kUAPlatform,
                  SerializeHeaderString(ua_metadata->platform));
    }

    if (ShouldAddClientHint(data, WebClientHintsType::kUAPlatformVersion)) {
      AddUAHeader(headers, WebClientHintsType::kUAPlatformVersion,
                  SerializeHeaderString(ua_metadata->platform_version));
    }

    if (ShouldAddClientHint(data, WebClientHintsType::kUAModel)) {
      AddUAHeader(headers, WebClientHintsType::kUAModel,
                  SerializeHeaderString(ua_metadata->model));
    }
    if (ShouldAddClientHint(data, WebClientHintsType::kUABitness)) {
      AddUAHeader(headers, WebClientHintsType::kUABitness,
                  SerializeHeaderString(ua_metadata->bitness));
    }
    if (ShouldAddClientHint(data, WebClientHintsType::kUAWoW64)) {
      AddUAHeader(headers, WebClientHintsType::kUAWoW64,
                  SerializeHeaderString(ua_metadata->wow64));
    }
    if (ShouldAddClientHint(data, WebClientHintsType::kUAReduced)) {
      AddUAHeader(headers, WebClientHintsType::kUAReduced,
                  SerializeHeaderString(true));
    }
    if (ShouldAddClientHint(data, WebClientHintsType::kUAFullVersionList)) {
      AddUAHeader(headers, WebClientHintsType::kUAFullVersionList,
                  ua_metadata->SerializeBrandFullVersionList());
    }
    if (ShouldAddClientHint(data, WebClientHintsType::kFullUserAgent)) {
      AddUAHeader(headers, WebClientHintsType::kFullUserAgent,
                  SerializeHeaderString(true));
    }
  } else if (call_type == ClientUaHeaderCallType::kAfterCreated) {
    RemoveClientHintHeader(WebClientHintsType::kUA, headers);
    RemoveClientHintHeader(WebClientHintsType::kUAMobile, headers);
    RemoveClientHintHeader(WebClientHintsType::kUAFullVersion, headers);
    RemoveClientHintHeader(WebClientHintsType::kUAArch, headers);
    RemoveClientHintHeader(WebClientHintsType::kUAPlatform, headers);
    RemoveClientHintHeader(WebClientHintsType::kUAPlatformVersion, headers);
    RemoveClientHintHeader(WebClientHintsType::kUAModel, headers);
    RemoveClientHintHeader(WebClientHintsType::kUABitness, headers);
    RemoveClientHintHeader(WebClientHintsType::kUAReduced, headers);
    RemoveClientHintHeader(WebClientHintsType::kUAFullVersionList, headers);
    RemoveClientHintHeader(WebClientHintsType::kUAWoW64, headers);
    RemoveClientHintHeader(WebClientHintsType::kFullUserAgent, headers);
  }
}

}  // namespace

bool ShouldAddClientHints(const url::Origin& origin,
                          FrameTreeNode* frame_tree_node,
                          ClientHintsControllerDelegate* delegate,
                          const absl::optional<GURL> maybe_request_url) {
  url::Origin origin_to_check =
      maybe_request_url ? url::Origin::Create(maybe_request_url.value())
                        : origin;
  // Client hints should only be enabled when JavaScript is enabled. Platforms
  // which enable/disable JavaScript on a per-origin basis should implement
  // IsJavaScriptAllowed to check a given origin. Other platforms (Android
  // WebView) enable/disable JavaScript on a per-View basis, using the
  // WebPreferences setting.
  return IsValidURLForClientHints(origin_to_check) &&
         delegate->IsJavaScriptAllowed(
             origin_to_check.GetURL(),
             frame_tree_node ? frame_tree_node->GetParentOrOuterDocument()
                             : nullptr) &&
         (!frame_tree_node || IsJavascriptEnabled(frame_tree_node));
}

unsigned long RoundRttForTesting(const std::string& host,
                                 const absl::optional<base::TimeDelta>& rtt) {
  return RoundRtt(host, rtt);
}

double RoundKbpsToMbpsForTesting(const std::string& host,
                                 const absl::optional<int32_t>& downlink_kbps) {
  return RoundKbpsToMbps(host, downlink_kbps);
}

void UpdateNavigationRequestClientUaHeaders(
    const url::Origin& origin,
    ClientHintsControllerDelegate* delegate,
    bool override_ua,
    FrameTreeNode* frame_tree_node,
    net::HttpRequestHeaders* headers,
    const absl::optional<GURL>& request_url) {
  DCHECK(frame_tree_node);
  if (!UserAgentClientHintEnabled() ||
      !ShouldAddClientHints(origin, frame_tree_node, delegate, request_url)) {
    return;
  }

  UpdateNavigationRequestClientUaHeadersImpl(
      origin, delegate, override_ua, frame_tree_node,
      ClientUaHeaderCallType::kAfterCreated, headers, {}, request_url);
}

namespace {

void AddRequestClientHintsHeaders(
    const url::Origin& origin,
    net::HttpRequestHeaders* headers,
    BrowserContext* context,
    ClientHintsControllerDelegate* delegate,
    bool is_ua_override_on,
    FrameTreeNode* frame_tree_node,
    const blink::ParsedPermissionsPolicy& container_policy,
    const absl::optional<GURL>& request_url) {
  ClientHintsExtendedData data(origin, frame_tree_node, delegate, request_url);
  UpdateIFramePermissionsPolicyWithDelegationSupportForClientHints(
      data, container_policy);

  GURL url = origin.GetURL();

  // Add Headers
  if (ShouldAddClientHint(data, WebClientHintsType::kDeviceMemory_DEPRECATED)) {
    AddDeviceMemoryHeader(headers, /*use_deprecated_version*/ true);
  }
  if (ShouldAddClientHint(data, WebClientHintsType::kDeviceMemory)) {
    AddDeviceMemoryHeader(headers);
  }
  if (ShouldAddClientHint(data, WebClientHintsType::kDpr_DEPRECATED)) {
    AddDPRHeader(headers, context, url, /*use_deprecated_version*/ true);
  }
  if (ShouldAddClientHint(data, WebClientHintsType::kDpr)) {
    AddDPRHeader(headers, context, url);
  }
  if (ShouldAddClientHint(data,
                          WebClientHintsType::kViewportWidth_DEPRECATED)) {
    AddViewportWidthHeader(headers, context, url,
                           /*use_deprecated_version*/ true);
  }
  if (ShouldAddClientHint(data, WebClientHintsType::kViewportWidth)) {
    AddViewportWidthHeader(headers, context, url);
  }
  if (ShouldAddClientHint(
          data, network::mojom::WebClientHintsType::kViewportHeight)) {
    AddViewportHeightHeader(headers, context, url);
  }
  network::NetworkQualityTracker* network_quality_tracker =
      delegate->GetNetworkQualityTracker();
  if (ShouldAddClientHint(data, WebClientHintsType::kRtt_DEPRECATED)) {
    AddRttHeader(headers, network_quality_tracker, url);
  }
  if (ShouldAddClientHint(data, WebClientHintsType::kDownlink_DEPRECATED)) {
    AddDownlinkHeader(headers, network_quality_tracker, url);
  }
  if (ShouldAddClientHint(data, WebClientHintsType::kEct_DEPRECATED)) {
    AddEctHeader(headers, network_quality_tracker, url);
  }

  if (UserAgentClientHintEnabled()) {
    UpdateNavigationRequestClientUaHeadersImpl(
        origin, delegate, is_ua_override_on, frame_tree_node,
        ClientUaHeaderCallType::kDuringCreation, headers, container_policy,
        request_url);
  }

  if (ShouldAddClientHint(data, WebClientHintsType::kPrefersColorScheme)) {
    AddPrefersColorSchemeHeader(headers, frame_tree_node);
  }

  if (ShouldAddClientHint(data, WebClientHintsType::kSaveData))
    AddSaveDataHeader(headers, context);

  // Static assert that triggers if a new client hint header is added. If a
  // new client hint header is added, the following assertion should be updated.
  // If possible, logic should be added above so that the request headers for
  // the newly added client hint can be added to the request.
  static_assert(
      network::mojom::WebClientHintsType::kSaveData ==
          network::mojom::WebClientHintsType::kMaxValue,
      "Consider adding client hint request headers from the browser process");

  // TODO(crbug.com/735518): If the request is redirected, the client hint
  // headers stay attached to the redirected request. Consider removing/adding
  // the client hints headers if the request is redirected with a change in
  // scheme or a change in the origin.
}

}  // namespace

void AddPrefetchNavigationRequestClientHintsHeaders(
    const url::Origin& origin,
    net::HttpRequestHeaders* headers,
    BrowserContext* context,
    ClientHintsControllerDelegate* delegate,
    bool is_ua_override_on,
    bool is_javascript_enabled) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(blink::kWebEffectiveConnectionTypeMappingCount,
            net::EFFECTIVE_CONNECTION_TYPE_4G + 1u);
  DCHECK_EQ(blink::kWebEffectiveConnectionTypeMappingCount,
            static_cast<size_t>(net::EFFECTIVE_CONNECTION_TYPE_LAST));
  DCHECK(context);

  // Since prefetch navigation doesn't have a related frame tree node,
  // |is_javascript_enabled| is passed in to get whether a typical frame tree
  // node would support javascript.
  if (!is_javascript_enabled ||
      !ShouldAddClientHints(origin, nullptr, delegate)) {
    return;
  }

  AddRequestClientHintsHeaders(origin, headers, context, delegate,
                               is_ua_override_on, nullptr, {}, absl::nullopt);
}

void AddNavigationRequestClientHintsHeaders(
    const url::Origin& origin,
    net::HttpRequestHeaders* headers,
    BrowserContext* context,
    ClientHintsControllerDelegate* delegate,
    bool is_ua_override_on,
    FrameTreeNode* frame_tree_node,
    const blink::ParsedPermissionsPolicy& container_policy,
    const absl::optional<GURL>& request_url) {
  DCHECK(frame_tree_node);
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK_EQ(blink::kWebEffectiveConnectionTypeMappingCount,
            net::EFFECTIVE_CONNECTION_TYPE_4G + 1u);
  DCHECK_EQ(blink::kWebEffectiveConnectionTypeMappingCount,
            static_cast<size_t>(net::EFFECTIVE_CONNECTION_TYPE_LAST));
  DCHECK(context);
  if (!ShouldAddClientHints(origin, frame_tree_node, delegate, request_url)) {
    return;
  }

  AddRequestClientHintsHeaders(origin, headers, context, delegate,
                               is_ua_override_on, frame_tree_node,
                               container_policy, request_url);
}

absl::optional<std::vector<WebClientHintsType>>
ParseAndPersistAcceptCHForNavigation(
    const url::Origin& origin,
    const network::mojom::ParsedHeadersPtr& parsed_headers,
    const net::HttpResponseHeaders* response_headers,
    BrowserContext* context,
    ClientHintsControllerDelegate* delegate,
    FrameTreeNode* frame_tree_node) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(context);
  DCHECK(parsed_headers);

  if (!parsed_headers->accept_ch)
    return absl::nullopt;

  if (!IsValidURLForClientHints(origin))
    return absl::nullopt;

  // Client hints should only be enabled when JavaScript is enabled. Platforms
  // which enable/disable JavaScript on a per-origin basis should implement
  // IsJavaScriptAllowed to check a given origin. Other platforms (Android
  // WebView) enable/disable JavaScript on a per-View basis, using the
  // WebPreferences setting.
  if (!delegate->IsJavaScriptAllowed(
          origin.GetURL(), frame_tree_node->GetParentOrOuterDocument()) ||
      !IsJavascriptEnabled(frame_tree_node)) {
    return absl::nullopt;
  }

  std::vector<WebClientHintsType> accept_ch = parsed_headers->accept_ch.value();
  url::Origin main_frame_origin = origin;
  absl::optional<url::Origin> third_party_origin;
  // Only the main frame should parse accept-CH, except for the temporary
  // Sec-CH-UA-Reduced client hint (used for the User-Agent reduction origin
  // trial).
  //
  // Note that if Sec-CH-UA-Reduced is persisted for an embedded frame, it
  // means a subsequent top-level navigation will read Sec-CH-UA-Reduced from
  // the Accept-CH cache and send a reduced User-Agent string.
  //
  // TODO(crbug.com/1258063): Delete this call when the UserAgentReduction
  // Origin Trial is finished.
  if (!frame_tree_node->IsMainFrame()) {
    RemoveAllClientHintsExceptOriginTrialHints(
        origin, frame_tree_node, delegate, &accept_ch, &main_frame_origin,
        &third_party_origin);
    if (accept_ch.empty()) {
      // There are is no Sec-CH-UA-Reduced in Accept-CH for the embedded frame,
      // so nothing should be persisted.
      return absl::nullopt;
    }
  }

  absl::optional<GURL> third_party_url = absl::nullopt;
  if (third_party_origin)
    third_party_url = third_party_origin->GetURL();

  blink::EnabledClientHints enabled_hints;
  for (const WebClientHintsType type : accept_ch) {
    enabled_hints.SetIsEnabled(main_frame_origin.GetURL(), third_party_url,
                               response_headers, type, true);
  }

  const std::vector<WebClientHintsType> persisted_hints =
      enabled_hints.GetEnabledHints();
  PersistAcceptCH(origin, frame_tree_node->GetParentOrOuterDocument(), delegate,
                  persisted_hints);
  return persisted_hints;
}

void PersistAcceptCH(const url::Origin& origin,
                     content::RenderFrameHost* parent_rfh,
                     ClientHintsControllerDelegate* delegate,
                     const std::vector<WebClientHintsType>& hints) {
  DCHECK(delegate);
  delegate->PersistClientHints(origin, parent_rfh, hints);
}

std::vector<WebClientHintsType> LookupAcceptCHForCommit(
    const url::Origin& origin,
    ClientHintsControllerDelegate* delegate,
    FrameTreeNode* frame_tree_node,
    const absl::optional<GURL>& request_url) {
  std::vector<WebClientHintsType> result;
  if (!ShouldAddClientHints(origin, frame_tree_node, delegate, request_url)) {
    return result;
  }

  const ClientHintsExtendedData data(origin, frame_tree_node, delegate,
                                     request_url);
  std::vector<WebClientHintsType> hints = data.hints.GetEnabledHints();
  if (data.is_embedder_ua_reduced &&
      !base::Contains(hints, WebClientHintsType::kUAReduced)) {
    hints.push_back(WebClientHintsType::kUAReduced);
  }
  if (data.is_embedder_ua_full &&
      !base::Contains(hints, WebClientHintsType::kFullUserAgent)) {
    hints.push_back(WebClientHintsType::kFullUserAgent);
  }
  return hints;
}

bool AreCriticalHintsMissing(
    const url::Origin& origin,
    FrameTreeNode* frame_tree_node,
    ClientHintsControllerDelegate* delegate,
    const std::vector<WebClientHintsType>& critical_hints) {
  ClientHintsExtendedData data(origin, frame_tree_node, delegate,
                               absl::nullopt);

  // Note: these only check for per-hint origin/permissions policy settings, not
  // origin-level or "browser-level" policies like disabiling JS or other
  // features.
  for (auto hint : critical_hints) {
    if (IsClientHintAllowed(data, hint) && !IsClientHintEnabled(data, hint)) {
      return true;
    }
  }

  return false;
}

}  // namespace content
