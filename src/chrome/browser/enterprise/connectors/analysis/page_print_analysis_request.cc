// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/page_print_analysis_request.h"

#include "base/memory/read_only_shared_memory_region.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"

namespace enterprise_connectors {

namespace {
constexpr size_t kMaxPageSize = 50 * 1024 * 1024;
}  // namespace

PagePrintAnalysisRequest::PagePrintAnalysisRequest(
    const AnalysisSettings& analysis_settings,
    base::ReadOnlySharedMemoryRegion page,
    safe_browsing::BinaryUploadService::ContentAnalysisCallback callback)
    : safe_browsing::BinaryUploadService::Request(
          std::move(callback),
          analysis_settings.analysis_url),
      page_(std::move(page)) {}

PagePrintAnalysisRequest::~PagePrintAnalysisRequest() = default;

void PagePrintAnalysisRequest::GetRequestData(DataCallback callback) {
  safe_browsing::BinaryUploadService::Request::Data data;
  data.size = page_.GetSize();

  if (data.size >= kMaxPageSize) {
    std::move(callback).Run(
        safe_browsing::BinaryUploadService::Result::FILE_TOO_LARGE,
        std::move(data));
    return;
  }

  data.page = std::move(page_);
  std::move(callback).Run(safe_browsing::BinaryUploadService::Result::SUCCESS,
                          std::move(data));
}

}  // namespace enterprise_connectors
