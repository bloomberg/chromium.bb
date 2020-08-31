// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_RENDERER_URL_LOADER_THROTTLE_PROVIDER_H_
#define CONTENT_PUBLIC_RENDERER_URL_LOADER_THROTTLE_PROVIDER_H_

#include <memory>
#include <vector>

#include "content/common/content_export.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"
#include "third_party/blink/public/platform/web_url_request.h"

namespace content {

enum class URLLoaderThrottleProviderType {
  // Used for requests from frames. Please note that the requests could be
  // frame or subresource requests.
  kFrame,
  // Used for requests from workers, including dedicated, shared and service
  // workers.
  kWorker
};

class CONTENT_EXPORT URLLoaderThrottleProvider {
 public:
  virtual ~URLLoaderThrottleProvider() {}

  // Used to copy a URLLoaderThrottleProvider between worker threads.
  virtual std::unique_ptr<URLLoaderThrottleProvider> Clone() = 0;

  // For frame requests this is called on the main thread. Dedicated, shared and
  // service workers call it on the worker thread. |render_frame_id| will be set
  // to the corresponding frame for frame and dedicated worker requests,
  // otherwise it will be MSG_ROUTING_NONE.
  virtual std::vector<std::unique_ptr<blink::URLLoaderThrottle>>
  CreateThrottles(int render_frame_id, const blink::WebURLRequest& request) = 0;

  // Set the network status online state as specified in |is_online|.
  virtual void SetOnline(bool is_online) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_RENDERER_URL_LOADER_THROTTLE_PROVIDER_H_
