// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/HostsUsingFeatures.h"

#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/Document.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/page/Page.h"
#include "platform/bindings/ScriptState.h"
#include "public/platform/Platform.h"

namespace blink {

HostsUsingFeatures::~HostsUsingFeatures() {
  UpdateMeasurementsAndClear();
}

HostsUsingFeatures::Value::Value() : count_bits_(0) {}

void HostsUsingFeatures::CountAnyWorld(Document& document, Feature feature) {
  document.HostsUsingFeaturesValue().Count(feature);
}

void HostsUsingFeatures::CountMainWorldOnly(const ScriptState* script_state,
                                            Document& document,
                                            Feature feature) {
  if (!script_state || !script_state->World().IsMainWorld())
    return;
  CountAnyWorld(document, feature);
}

static Document* DocumentFromEventTarget(EventTarget& target) {
  ExecutionContext* execution_context = target.GetExecutionContext();
  if (!execution_context)
    return nullptr;
  if (execution_context->IsDocument())
    return ToDocument(execution_context);
  if (LocalDOMWindow* executing_window = execution_context->ExecutingWindow())
    return executing_window->document();
  return nullptr;
}

void HostsUsingFeatures::CountHostOrIsolatedWorldHumanReadableName(
    const ScriptState* script_state,
    EventTarget& target,
    Feature feature) {
  if (!script_state)
    return;
  Document* document = DocumentFromEventTarget(target);
  if (!document)
    return;
  if (script_state->World().IsMainWorld()) {
    document->HostsUsingFeaturesValue().Count(feature);
    return;
  }
  if (Page* page = document->GetPage()) {
    page->GetHostsUsingFeatures().CountName(
        feature, script_state->World().NonMainWorldHumanReadableName());
  }
}

void HostsUsingFeatures::Value::Count(Feature feature) {
  DCHECK(feature < Feature::kNumberOfFeatures);
  count_bits_ |= 1 << static_cast<unsigned>(feature);
}

void HostsUsingFeatures::CountName(Feature feature, const String& name) {
  auto result = value_by_name_.insert(name, Value());
  result.stored_value->value.Count(feature);
}

void HostsUsingFeatures::Clear() {
  value_by_name_.clear();
  url_and_values_.clear();
}

void HostsUsingFeatures::DocumentDetached(Document& document) {
  HostsUsingFeatures::Value counter = document.HostsUsingFeaturesValue();
  if (counter.IsEmpty())
    return;

  const KURL& url = document.Url();
  if (!url.ProtocolIsInHTTPFamily())
    return;

  url_and_values_.push_back(std::make_pair(url, counter));
  document.HostsUsingFeaturesValue().Clear();
  DCHECK(document.HostsUsingFeaturesValue().IsEmpty());
}

void HostsUsingFeatures::UpdateMeasurementsAndClear() {
  if (!url_and_values_.IsEmpty()) {
    RecordHostToRappor();
    RecordETLDPlus1ToRappor();
    url_and_values_.clear();
  }
  if (!value_by_name_.IsEmpty())
    RecordNamesToRappor();
}

void HostsUsingFeatures::RecordHostToRappor() {
  DCHECK(!url_and_values_.IsEmpty());

  // Aggregate values by hosts.
  HashMap<String, HostsUsingFeatures::Value> aggregated_by_host;
  for (const auto& url_and_value : url_and_values_) {
    DCHECK(!url_and_value.first.IsEmpty());
    auto result = aggregated_by_host.insert(url_and_value.first.Host(),
                                            url_and_value.second);
    if (!result.is_new_entry)
      result.stored_value->value.Aggregate(url_and_value.second);
  }

  // Report to RAPPOR.
  for (auto& host_and_value : aggregated_by_host)
    host_and_value.value.RecordHostToRappor(host_and_value.key);
}

void HostsUsingFeatures::RecordETLDPlus1ToRappor() {
  DCHECK(!url_and_values_.IsEmpty());

  // Aggregate values by URL.
  HashMap<String, HostsUsingFeatures::Value> aggregated_by_url;
  for (const auto& url_and_value : url_and_values_) {
    DCHECK(!url_and_value.first.IsEmpty());
    auto result =
        aggregated_by_url.insert(url_and_value.first, url_and_value.second);
    if (!result.is_new_entry)
      result.stored_value->value.Aggregate(url_and_value.second);
  }

  // Report to RAPPOR.
  for (auto& url_and_value : aggregated_by_url)
    url_and_value.value.RecordETLDPlus1ToRappor(KURL(url_and_value.key));
}

void HostsUsingFeatures::RecordNamesToRappor() {
  DCHECK(!value_by_name_.IsEmpty());

  for (auto& name_and_value : value_by_name_)
    name_and_value.value.RecordNameToRappor(name_and_value.key);

  value_by_name_.clear();
}

void HostsUsingFeatures::Value::Aggregate(HostsUsingFeatures::Value other) {
  count_bits_ |= other.count_bits_;
}

void HostsUsingFeatures::Value::RecordHostToRappor(const String& host) {
  if (Get(Feature::kElementCreateShadowRoot))
    Platform::Current()->RecordRappor("WebComponents.ElementCreateShadowRoot",
                                      host);
  if (Get(Feature::kElementAttachShadow))
    Platform::Current()->RecordRappor("WebComponents.ElementAttachShadow",
                                      host);
  if (Get(Feature::kDocumentRegisterElement))
    Platform::Current()->RecordRappor("WebComponents.DocumentRegisterElement",
                                      host);
  if (Get(Feature::kEventPath))
    Platform::Current()->RecordRappor("WebComponents.EventPath", host);
  if (Get(Feature::kDeviceMotionInsecureHost))
    Platform::Current()->RecordRappor(
        "PowerfulFeatureUse.Host.DeviceMotion.Insecure", host);
  if (Get(Feature::kDeviceOrientationInsecureHost))
    Platform::Current()->RecordRappor(
        "PowerfulFeatureUse.Host.DeviceOrientation.Insecure", host);
  if (Get(Feature::kFullscreenInsecureHost))
    Platform::Current()->RecordRappor(
        "PowerfulFeatureUse.Host.Fullscreen.Insecure", host);
  if (Get(Feature::kGeolocationInsecureHost))
    Platform::Current()->RecordRappor(
        "PowerfulFeatureUse.Host.Geolocation.Insecure", host);
  if (Get(Feature::kApplicationCacheManifestSelectInsecureHost))
    Platform::Current()->RecordRappor(
        "PowerfulFeatureUse.Host.ApplicationCacheManifestSelect.Insecure",
        host);
  if (Get(Feature::kApplicationCacheAPIInsecureHost))
    Platform::Current()->RecordRappor(
        "PowerfulFeatureUse.Host.ApplicationCacheAPI.Insecure", host);
}

void HostsUsingFeatures::Value::RecordNameToRappor(const String& name) {
  if (Get(Feature::kEventPath))
    Platform::Current()->RecordRappor("WebComponents.EventPath.Extensions",
                                      name);
}

void HostsUsingFeatures::Value::RecordETLDPlus1ToRappor(const KURL& url) {
  if (Get(Feature::kGetUserMediaInsecureHost))
    Platform::Current()->RecordRapporURL(
        "PowerfulFeatureUse.ETLDPlus1.GetUserMedia.Insecure", WebURL(url));
  if (Get(Feature::kGetUserMediaSecureHost))
    Platform::Current()->RecordRapporURL(
        "PowerfulFeatureUse.ETLDPlus1.GetUserMedia.Secure", WebURL(url));
  if (Get(Feature::kRTCPeerConnectionAudio))
    Platform::Current()->RecordRapporURL("RTCPeerConnection.Audio",
                                         WebURL(url));
  if (Get(Feature::kRTCPeerConnectionVideo))
    Platform::Current()->RecordRapporURL("RTCPeerConnection.Video",
                                         WebURL(url));
  if (Get(Feature::kRTCPeerConnectionDataChannel))
    Platform::Current()->RecordRapporURL("RTCPeerConnection.DataChannel",
                                         WebURL(url));
  if (Get(Feature::kRTCPeerConnectionUsed) &&
      !Get(Feature::kRTCPeerConnectionAudio) &&
      !Get(Feature::kRTCPeerConnectionVideo) &&
      !Get(Feature::kRTCPeerConnectionDataChannel)) {
    Platform::Current()->RecordRapporURL("RTCPeerConnection.Unconnected",
                                         WebURL(url));
  }
}

}  // namespace blink
