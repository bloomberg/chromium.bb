// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_GET_THUMBNAIL_INFO_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_GET_THUMBNAIL_INFO_TASK_H_

#include <memory>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "components/offline_pages/task/task.h"
#include "url/gurl.h"

namespace offline_pages {
class PrefetchStore;

// Task that attempts to get thumbnail information about an offline item in the
// prefetch store.
class GetThumbnailInfoTask : public Task {
 public:
  struct Result {
    // The thumbnail URL of the offline item. This is empty if the offline item
    // was not found, or if the item had no thumbnail URL.
    GURL thumbnail_url;
  };
  using ResultCallback = base::OnceCallback<void(Result)>;

  GetThumbnailInfoTask(PrefetchStore* store,
                       const int64_t offline_id,
                       ResultCallback callback);
  ~GetThumbnailInfoTask() override;

  // Task implementation.
  void Run() override;

 private:
  void CompleteTaskAndForwardResult(Result result);
  PrefetchStore* prefetch_store_;
  int64_t offline_id_;
  ResultCallback callback_;

  base::WeakPtrFactory<GetThumbnailInfoTask> weak_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(GetThumbnailInfoTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_TASKS_GET_THUMBNAIL_INFO_TASK_H_
