// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_NAVIGATION_PARAMS_H_
#define THIRD_PARTY_BLINK_PUBLIC_WEB_WEB_NAVIGATION_PARAMS_H_

#include <memory>

#include "third_party/blink/public/platform/modules/service_worker/web_service_worker_network_provider.h"
#include "third_party/blink/public/platform/web_source_location.h"
#include "third_party/blink/public/web/web_navigation_timings.h"

namespace blink {

// This structure holds all information provided by the embedder that is
// needed for blink to load a Document. This is hence different from
// WebDocumentLoader::ExtraData, which is an opaque structure stored in the
// DocumentLoader and used by the embedder.
struct WebNavigationParams {
  WebNavigationTimings navigation_timings;
  WebSourceLocation source_location;
  bool is_user_activated = false;
  std::unique_ptr<blink::WebServiceWorkerNetworkProvider>
      service_worker_network_provider;
};

}  // namespace blink

#endif
