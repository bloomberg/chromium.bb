// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PRERENDER_URL_LOADER_THROTTLE_H_
#define CHROME_COMMON_PRERENDER_URL_LOADER_THROTTLE_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "chrome/common/prerender_canceler.mojom.h"
#include "chrome/common/prerender_types.h"
#include "net/base/request_priority.h"
#include "third_party/blink/public/common/loader/url_loader_throttle.h"
#include "third_party/blink/public/mojom/loader/resource_load_info.mojom-shared.h"

namespace prerender {

class PrerenderURLLoaderThrottle
    : public blink::URLLoaderThrottle,
      public base::SupportsWeakPtr<PrerenderURLLoaderThrottle> {
 public:
  PrerenderURLLoaderThrottle(
      PrerenderMode mode,
      const std::string& histogram_prefix,
      mojo::PendingRemote<chrome::mojom::PrerenderCanceler> canceler);
  ~PrerenderURLLoaderThrottle() override;

  // Called when the prerender is used. This will unpaused requests and set the
  // priorities to the original value.
  void PrerenderUsed();

  void set_destruction_closure(base::OnceClosure closure) {
    destruction_closure_ = std::move(closure);
  }

 private:
  // blink::URLLoaderThrottle implementation.
  void DetachFromCurrentSequence() override;
  void WillStartRequest(network::ResourceRequest* request,
                        bool* defer) override;
  void WillRedirectRequest(
      net::RedirectInfo* redirect_info,
      const network::mojom::URLResponseHead& response_head,
      bool* defer,
      std::vector<std::string>* to_be_removed_headers,
      net::HttpRequestHeaders* modified_headers,
      net::HttpRequestHeaders* modified_cors_exempt_headers) override;
  void WillProcessResponse(const GURL& response_url,
                           network::mojom::URLResponseHead* response_head,
                           bool* defer) override;

  void OnTimedOut();

  PrerenderMode mode_;
  std::string histogram_prefix_;

  bool deferred_ = false;
  int redirect_count_ = 0;
  blink::mojom::ResourceType resource_type_;

  mojo::PendingRemote<chrome::mojom::PrerenderCanceler> canceler_;

  // The throttle changes most request priorities to IDLE during prerendering.
  // The priority is reset back to the original priority when prerendering is
  // finished.
  base::Optional<net::RequestPriority> original_request_priority_;

  base::OnceClosure destruction_closure_;

  base::OneShotTimer detached_timer_;
};

}  // namespace prerender

#endif  // CHROME_COMMON_PRERENDER_URL_LOADER_THROTTLE_H_
