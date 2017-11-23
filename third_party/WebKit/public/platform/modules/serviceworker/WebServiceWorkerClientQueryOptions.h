// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebServiceWorkerClientQueryOptions_h
#define WebServiceWorkerClientQueryOptions_h

#include "third_party/WebKit/common/service_worker/service_worker_client.mojom-shared.h"

namespace blink {

struct WebServiceWorkerClientQueryOptions {
  WebServiceWorkerClientQueryOptions()
      : client_type(mojom::ServiceWorkerClientType::kWindow),
        include_uncontrolled(false) {}

  mojom::ServiceWorkerClientType client_type;
  bool include_uncontrolled;
};

}  // namespace blink

#endif  // WebServiceWorkerClientQueryOptions_h
