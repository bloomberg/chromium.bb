// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/upload_list/combining_upload_list.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "base/time/time.h"

CombiningUploadList::CombiningUploadList(
    std::vector<scoped_refptr<UploadList>> sublists)
    : sublists_(std::move(sublists)) {
  DCHECK(!sublists_.empty());
}

CombiningUploadList::~CombiningUploadList() = default;

std::vector<UploadList::UploadInfo> CombiningUploadList::LoadUploadList() {
  std::vector<UploadInfo> uploads;
  for (const auto& sublist : sublists_) {
    std::vector<UploadInfo> sublist_uploads = sublist->LoadUploadList();
    uploads.reserve(uploads.size() + sublist_uploads.size());
    std::move(sublist_uploads.begin(), sublist_uploads.end(),
              std::back_inserter(uploads));
  }

  // UploadList expects the list to be sorted, newest first. We sort by
  // capture_time if we have it because that's the most stable (won't change
  // if a crash is uploaded), but we'll use upload_time if that's all we have.
  std::sort(uploads.begin(), uploads.end(),
            [](const UploadInfo& a, const UploadInfo& b) {
              base::Time time_a =
                  a.capture_time.is_null() ? a.upload_time : a.capture_time;
              base::Time time_b =
                  b.capture_time.is_null() ? b.upload_time : b.capture_time;
              return time_a > time_b;
            });
  return uploads;
}

void CombiningUploadList::ClearUploadList(const base::Time& begin,
                                          const base::Time& end) {
  for (const scoped_refptr<UploadList>& sublist : sublists_) {
    sublist->ClearUploadList(begin, end);
  }
}

void CombiningUploadList::RequestSingleUpload(const std::string& local_id) {
  for (const scoped_refptr<UploadList>& sublist : sublists_) {
    sublist->RequestSingleUpload(local_id);
  }
}
