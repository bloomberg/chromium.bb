// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebServiceWorkerClientsInfo_h
#define WebServiceWorkerClientsInfo_h

#include "public/platform/WebCallbacks.h"
#include "public/platform/WebURL.h"
#include "public/platform/WebURLRequest.h"
#include "public/platform/WebVector.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerClientType.h"
#include "third_party/WebKit/common/page/page_visibility_state.mojom-shared.h"

#include <memory>

namespace blink {

struct WebServiceWorkerError;

struct WebServiceWorkerClientInfo {
  WebServiceWorkerClientInfo()
      : page_visibility_state(mojom::PageVisibilityState::kLast),
        is_focused(false),
        frame_type(WebURLRequest::kFrameTypeNone),
        client_type(kWebServiceWorkerClientTypeWindow) {}

  WebString uuid;

  mojom::PageVisibilityState page_visibility_state;
  bool is_focused;
  WebURL url;
  WebURLRequest::FrameType frame_type;
  WebServiceWorkerClientType client_type;
};

struct WebServiceWorkerClientsInfo {
  WebVector<WebServiceWorkerClientInfo> clients;
};

// Two WebCallbacks, one for one client, one for a WebVector of clients.
using WebServiceWorkerClientCallbacks =
    WebCallbacks<std::unique_ptr<WebServiceWorkerClientInfo>,
                 const WebServiceWorkerError&>;
using WebServiceWorkerClientsCallbacks =
    WebCallbacks<const WebServiceWorkerClientsInfo&,
                 const WebServiceWorkerError&>;

}  // namespace blink

#endif  // WebServiceWorkerClientsInfo_h
