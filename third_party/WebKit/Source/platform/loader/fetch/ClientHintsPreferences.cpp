// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/loader/fetch/ClientHintsPreferences.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/network/HTTPParsers.h"

namespace blink {

ClientHintsPreferences::ClientHintsPreferences()
    : should_send_dpr_(false),
      should_send_resource_width_(false),
      should_send_viewport_width_(false) {}

void ClientHintsPreferences::UpdateFrom(
    const ClientHintsPreferences& preferences) {
  should_send_dpr_ = preferences.should_send_dpr_;
  should_send_resource_width_ = preferences.should_send_resource_width_;
  should_send_viewport_width_ = preferences.should_send_viewport_width_;
}

void ClientHintsPreferences::UpdateFromAcceptClientHintsHeader(
    const String& header_value,
    Context* context) {
  if (!RuntimeEnabledFeatures::clientHintsEnabled() || header_value.IsEmpty())
    return;

  CommaDelimitedHeaderSet accept_client_hints_header;
  ParseCommaDelimitedHeader(header_value, accept_client_hints_header);
  if (accept_client_hints_header.Contains("dpr")) {
    if (context)
      context->CountClientHintsDPR();
    should_send_dpr_ = true;
  }

  if (accept_client_hints_header.Contains("width")) {
    if (context)
      context->CountClientHintsResourceWidth();
    should_send_resource_width_ = true;
  }

  if (accept_client_hints_header.Contains("viewport-width")) {
    if (context)
      context->CountClientHintsViewportWidth();
    should_send_viewport_width_ = true;
  }
}

}  // namespace blink
