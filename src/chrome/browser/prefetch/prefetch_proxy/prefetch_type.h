// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_PREFETCH_PROXY_PREFETCH_TYPE_H_
#define CHROME_BROWSER_PREFETCH_PREFETCH_PROXY_PREFETCH_TYPE_H_

// The type of prefetch. This determines various details about how a prefetch is
// handled.
class PrefetchType {
 public:
  PrefetchType(bool use_isolated_network_context,
               bool use_prefetch_proxy,
               bool can_prefetch_subresources);
  ~PrefetchType();

  PrefetchType(const PrefetchType& prefetch_type);
  PrefetchType& operator=(const PrefetchType& prefetch_type);

  // Whether prefetches of this type need to use an isolated network context, or
  // use the default network context.
  bool IsIsolatedNetworkContextRequired() const {
    return use_isolated_network_context_;
  }

  // Whether prefetches of this type need to use the Prefetch Proxy.
  bool IsProxyRequired() const { return use_prefetch_proxy_; }

  // Whether prefetches of this type can prefetch subresources.
  bool AllowedToPrefetchSubresources() const {
    return can_prefetch_subresources_;
  }

 private:
  friend bool operator==(const PrefetchType& prefetch_type_1,
                         const PrefetchType& prefetch_type_2);

  bool use_isolated_network_context_;
  bool use_prefetch_proxy_;
  bool can_prefetch_subresources_;
};

bool operator==(const PrefetchType& prefetch_type_1,
                const PrefetchType& prefetch_type_2);
bool operator!=(const PrefetchType& prefetch_type_1,
                const PrefetchType& prefetch_type_2);

#endif  // CHROME_BROWSER_PREFETCH_PREFETCH_PROXY_PREFETCH_TYPE_H_
