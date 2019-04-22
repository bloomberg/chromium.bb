// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREVIEWS_CORE_TEST_PREVIEWS_DECIDER_H_
#define COMPONENTS_PREVIEWS_CORE_TEST_PREVIEWS_DECIDER_H_

#include "components/previews/core/previews_decider.h"

namespace previews {

// Simple test implementation of PreviewsDecider interface.
class TestPreviewsDecider : public previews::PreviewsDecider {
 public:
  TestPreviewsDecider(bool allow_previews);
  ~TestPreviewsDecider() override;

  // previews::PreviewsDecider:
  bool ShouldAllowPreviewAtNavigationStart(PreviewsUserData* previews_data,
                                           const GURL& url,
                                           bool is_reload,
                                           PreviewsType type) const override;
  bool ShouldCommitPreview(PreviewsUserData* previews_data,
                           const GURL& url,
                           PreviewsType type) const override;
  bool LoadPageHints(const GURL& url) override;
  bool GetResourceLoadingHints(
      const GURL& url,
      std::vector<std::string>* out_resource_patterns_to_block) const override;
  void LogHintCacheMatch(const GURL& url, bool is_committed) const override;

 private:
  bool allow_previews_;
};

}  // namespace previews

#endif  // COMPONENTS_PREVIEWS_CORE_TEST_PREVIEWS_DECIDER_H_
