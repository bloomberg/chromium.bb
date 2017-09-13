// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/loader/fetch/ClientHintsPreferences.h"

#include "platform/HTTPNames.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/network/HTTPParsers.h"
#include "platform/weborigin/KURL.h"

namespace blink {

namespace {

// Mapping from WebClientHintsType to the header value for enabling the
// corresponding client hint. The ordering should match the ordering of enums in
// WebClientHintsType.
static constexpr const char* kHeaderMapping[] = {"device-memory", "dpr",
                                                 "width", "viewport-width"};

static_assert(static_cast<int>(mojom::WebClientHintsType::kLast) + 1 ==
                  arraysize(kHeaderMapping),
              "unhandled client hint type");

void ParseAcceptChHeader(const String& header_value,
                         WebEnabledClientHints& enabled_hints) {
  CommaDelimitedHeaderSet accept_client_hints_header;
  ParseCommaDelimitedHeader(header_value, accept_client_hints_header);

  for (size_t i = 0; i < static_cast<int>(mojom::WebClientHintsType::kLast) + 1;
       ++i) {
    enabled_hints.SetIsEnabled(
        static_cast<mojom::WebClientHintsType>(i),
        accept_client_hints_header.Contains(kHeaderMapping[i]));
  }

  enabled_hints.SetIsEnabled(
      mojom::WebClientHintsType::kDeviceMemory,
      enabled_hints.IsEnabled(mojom::WebClientHintsType::kDeviceMemory) &&
          RuntimeEnabledFeatures::DeviceMemoryHeaderEnabled());
}

}  // namespace

ClientHintsPreferences::ClientHintsPreferences() {}

void ClientHintsPreferences::UpdateFrom(
    const ClientHintsPreferences& preferences) {
  for (size_t i = 0; i < static_cast<int>(mojom::WebClientHintsType::kLast) + 1;
       ++i) {
    mojom::WebClientHintsType type = static_cast<mojom::WebClientHintsType>(i);
    enabled_hints_.SetIsEnabled(type, preferences.ShouldSend(type));
  }
}

void ClientHintsPreferences::UpdateFromAcceptClientHintsHeader(
    const String& header_value,
    Context* context) {
  if (!RuntimeEnabledFeatures::ClientHintsEnabled() || header_value.IsEmpty())
    return;

  WebEnabledClientHints new_enabled_types;

  ParseAcceptChHeader(header_value, new_enabled_types);

  for (size_t i = 0; i < static_cast<int>(mojom::WebClientHintsType::kLast) + 1;
       ++i) {
    mojom::WebClientHintsType type = static_cast<mojom::WebClientHintsType>(i);
    enabled_hints_.SetIsEnabled(type, enabled_hints_.IsEnabled(type) ||
                                          new_enabled_types.IsEnabled(type));
  }

  if (context) {
    for (size_t i = 0;
         i < static_cast<int>(mojom::WebClientHintsType::kLast) + 1; ++i) {
      mojom::WebClientHintsType type =
          static_cast<mojom::WebClientHintsType>(i);
      if (enabled_hints_.IsEnabled(type))
        context->CountClientHints(type);
    }
  }
}

// static
void ClientHintsPreferences::UpdatePersistentHintsFromHeaders(
    const ResourceResponse& response,
    Context* context,
    WebEnabledClientHints& enabled_hints,
    TimeDelta* persist_duration) {
  *persist_duration = base::TimeDelta();

  if (response.WasCached())
    return;

  String accept_ch_header_value =
      response.HttpHeaderField(HTTPNames::Accept_CH);
  String accept_ch_lifetime_header_value =
      response.HttpHeaderField(HTTPNames::Accept_CH_Lifetime);

  if (!RuntimeEnabledFeatures::ClientHintsEnabled() ||
      !RuntimeEnabledFeatures::ClientHintsPersistentEnabled() ||
      accept_ch_header_value.IsEmpty() ||
      accept_ch_lifetime_header_value.IsEmpty()) {
    return;
  }

  const KURL url = response.Url();

  if (url.Protocol() != "https") {
    // Only HTTPS domains are allowed to persist client hints.
    return;
  }

  bool conversion_ok = false;
  int64_t persist_duration_seconds =
      accept_ch_lifetime_header_value.ToInt64Strict(&conversion_ok);
  if (!conversion_ok || persist_duration_seconds <= 0)
    return;

  *persist_duration = TimeDelta::FromSeconds(persist_duration_seconds);
  if (context)
    context->CountPersistentClientHintHeaders();

  ParseAcceptChHeader(accept_ch_header_value, enabled_hints);
}

}  // namespace blink
