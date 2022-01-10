// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CEF_LIBCEF_RENDERER_ALLOY_URL_LOADER_THROTTLE_PROVIDER_IMPL_H_
#define CEF_LIBCEF_RENDERER_ALLOY_URL_LOADER_THROTTLE_PROVIDER_IMPL_H_

#include <memory>
#include <vector>

#include "base/threading/thread_checker.h"
#include "third_party/blink/public/platform/url_loader_throttle_provider.h"

// Instances must be constructed on the render thread, and then used and
// destructed on a single thread, which can be different from the render thread.
class CefURLLoaderThrottleProviderImpl
    : public blink::URLLoaderThrottleProvider {
 public:
  explicit CefURLLoaderThrottleProviderImpl(
      blink::URLLoaderThrottleProviderType type);

  CefURLLoaderThrottleProviderImpl& operator=(
      const CefURLLoaderThrottleProviderImpl&) = delete;

  ~CefURLLoaderThrottleProviderImpl() override;

  // blink::URLLoaderThrottleProvider implementation.
  std::unique_ptr<blink::URLLoaderThrottleProvider> Clone() override;
  blink::WebVector<std::unique_ptr<blink::URLLoaderThrottle>> CreateThrottles(
      int render_frame_id,
      const blink::WebURLRequest& request) override;
  void SetOnline(bool is_online) override;

 private:
  // This copy constructor works in conjunction with Clone(), not intended for
  // general use.
  CefURLLoaderThrottleProviderImpl(
      const CefURLLoaderThrottleProviderImpl& other);

  blink::URLLoaderThrottleProviderType type_;

  THREAD_CHECKER(thread_checker_);
};

#endif  // CEF_LIBCEF_RENDERER_ALLOY_URL_LOADER_THROTTLE_PROVIDER_IMPL_H_
