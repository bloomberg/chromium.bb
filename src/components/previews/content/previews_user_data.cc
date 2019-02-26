// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/previews/content/previews_user_data.h"

namespace previews {

const void* const kPreviewsUserDataKey = &kPreviewsUserDataKey;

PreviewsUserData::PreviewsUserData(uint64_t page_id)
    : page_id_(page_id), server_lite_page_info_(nullptr) {}

PreviewsUserData::~PreviewsUserData() {}

PreviewsUserData::PreviewsUserData(const PreviewsUserData& other)
    : page_id_(other.page_id_),
      navigation_ect_(other.navigation_ect_),
      data_savings_inflation_percent_(other.data_savings_inflation_percent_),
      cache_control_no_transform_directive_(
          other.cache_control_no_transform_directive_),
      offline_preview_used_(other.offline_preview_used_),
      black_listed_for_lite_page_(other.black_listed_for_lite_page_),
      committed_previews_type_(other.committed_previews_type_),
      allowed_previews_state_(other.allowed_previews_state_),
      committed_previews_state_(other.committed_previews_state_) {
  if (other.server_lite_page_info_) {
    server_lite_page_info_ =
        std::make_unique<ServerLitePageInfo>(*other.server_lite_page_info_);
  }
}

void PreviewsUserData::SetCommittedPreviewsType(
    previews::PreviewsType previews_type) {
  DCHECK(committed_previews_type_ == PreviewsType::NONE);
  committed_previews_type_ = previews_type;
}

void PreviewsUserData::SetCommittedPreviewsTypeForTesting(
    previews::PreviewsType previews_type) {
  committed_previews_type_ = previews_type;
}

}  // namespace previews
