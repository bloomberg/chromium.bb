// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREVIEWS_PREVIEWS_TOP_HOST_PROVIDER_IMPL_H_
#define CHROME_BROWSER_PREVIEWS_PREVIEWS_TOP_HOST_PROVIDER_IMPL_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/sequence_checker.h"
#include "components/previews/content/previews_top_host_provider.h"

class PrefService;

namespace content {
class BrowserContext;
}

namespace previews {

// An implementation of the PreviewTopHostProvider for getting the top sites
// based on site engagement scores.
class PreviewsTopHostProviderImpl : public PreviewsTopHostProvider {
 public:
  explicit PreviewsTopHostProviderImpl(content::BrowserContext* BrowserContext);
  ~PreviewsTopHostProviderImpl() override;

  std::vector<std::string> GetTopHosts(size_t max_sites) const override;

 private:
  // |browser_context_| is used for interaction with the SiteEngagementService
  // and the embedder should guarantee that it is non-null during the lifetime
  // of |this|.
  content::BrowserContext* browser_context_;

  // |pref_service_| provides information about the current profile's
  // settings. It is not owned and guaranteed to outlive |this|.
  PrefService* pref_service_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(PreviewsTopHostProviderImpl);
};

}  // namespace previews

#endif  // CHROME_BROWSER_PREVIEWS_PREVIEWS_TOP_HOST_PROVIDER_IMPL_H_
