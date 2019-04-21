// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/core/test_previews_decider.h"

namespace previews {

TestPreviewsDecider::TestPreviewsDecider(bool allow_previews)
    : allow_previews_(allow_previews) {}

TestPreviewsDecider::~TestPreviewsDecider() {}

bool TestPreviewsDecider::ShouldAllowPreviewAtNavigationStart(
    PreviewsUserData* previews_data,
    const GURL& url,
    bool is_reload,
    PreviewsType type) const {
  return allow_previews_;
}

bool TestPreviewsDecider::ShouldCommitPreview(PreviewsUserData* previews_data,
                                              const GURL& url,
                                              PreviewsType type) const {
  return allow_previews_;
}

bool TestPreviewsDecider::LoadPageHints(const GURL& url) {
  return true;
}

bool TestPreviewsDecider::GetResourceLoadingHints(
    const GURL& url,
    std::vector<std::string>* out_resource_patterns_to_block) const {
  return false;
}

void TestPreviewsDecider::LogHintCacheMatch(const GURL& url,
                                            bool is_committed) const {}

}  // namespace previews
