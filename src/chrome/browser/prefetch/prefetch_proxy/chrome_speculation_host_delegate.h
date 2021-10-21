// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_PREFETCH_PROXY_CHROME_SPECULATION_HOST_DELEGATE_H_
#define CHROME_BROWSER_PREFETCH_PREFETCH_PROXY_CHROME_SPECULATION_HOST_DELEGATE_H_

#include <vector>

#include "content/public/browser/speculation_host_delegate.h"
#include "third_party/blink/public/mojom/speculation_rules/speculation_rules.mojom.h"
#include "url/gurl.h"

namespace content {
class RenderFrameHost;
}  // namespace content

namespace prerender {
class NoStatePrefetchHandle;
}

class ChromeSpeculationHostDelegate : public content::SpeculationHostDelegate {
 public:
  explicit ChromeSpeculationHostDelegate(
      content::RenderFrameHost& render_frame_host);
  ~ChromeSpeculationHostDelegate() override;

  // Disallows copy and move operations.
  ChromeSpeculationHostDelegate(const ChromeSpeculationHostDelegate&) = delete;
  ChromeSpeculationHostDelegate& operator=(
      const ChromeSpeculationHostDelegate&) = delete;

  // content::SpeculationRulesDelegate implementation.
  void ProcessCandidates(
      std::vector<blink::mojom::SpeculationCandidatePtr>& candidates) override;

 private:
  // content::SpeculationHostImpl, which inherits content::DocumentService,
  // owns `this`, so `this` can access `render_frame_host_` safely.
  content::RenderFrameHost& render_frame_host_;

  // All on-going NoStatePrefetches
  std::vector<std::unique_ptr<prerender::NoStatePrefetchHandle>>
      same_origin_no_state_prefetches_;
};

#endif  // CHROME_BROWSER_PREFETCH_PREFETCH_PROXY_CHROME_SPECULATION_HOST_DELEGATE_H_
