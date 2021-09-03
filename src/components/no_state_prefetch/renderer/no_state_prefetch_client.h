// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NO_STATE_PREFETCH_RENDERER_NO_STATE_PREFETCH_CLIENT_H_
#define COMPONENTS_NO_STATE_PREFETCH_RENDERER_NO_STATE_PREFETCH_CLIENT_H_

#include "base/compiler_specific.h"
#include "third_party/blink/public/web/web_no_state_prefetch_client.h"
#include "third_party/blink/public/web/web_view_observer.h"

namespace prerender {

class NoStatePrefetchClient : public blink::WebViewObserver,
                              public blink::WebNoStatePrefetchClient {
 public:
  explicit NoStatePrefetchClient(blink::WebView* web_view);

 private:
  ~NoStatePrefetchClient() override;

  // blink::WebViewObserver implementation.
  void OnDestruct() override;

  // Implements blink::WebNoStatePrefetchClient
  bool IsPrefetchOnly() override;
};

}  // namespace prerender

#endif  // COMPONENTS_NO_STATE_PREFETCH_RENDERER_NO_STATE_PREFETCH_CLIENT_H_
