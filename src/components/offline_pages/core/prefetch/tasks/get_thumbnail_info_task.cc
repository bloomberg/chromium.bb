// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/offline_pages/core/prefetch/tasks/get_thumbnail_info_task.h"

#include <utility>

#include "components/offline_pages/core/prefetch/store/prefetch_store.h"
#include "sql/database.h"
#include "sql/statement.h"

namespace offline_pages {
namespace {

GetThumbnailInfoTask::Result GetThumbnailInfoSync(const int64_t offline_id,
                                                  sql::Database* db) {
  static const char kSql[] = R"(SELECT thumbnail_url FROM prefetch_items
    WHERE offline_id=?;)";

  sql::Statement statement(db->GetCachedStatement(SQL_FROM_HERE, kSql));
  DCHECK(statement.is_valid());

  GetThumbnailInfoTask::Result result;
  statement.BindInt64(0, offline_id);
  if (statement.Step())
    result.thumbnail_url = GURL(statement.ColumnString(0));

  return result;
}

}  // namespace

GetThumbnailInfoTask::GetThumbnailInfoTask(PrefetchStore* store,
                                           const int64_t offline_id,
                                           ResultCallback callback)
    : prefetch_store_(store),
      offline_id_(offline_id),
      callback_(std::move(callback)) {}

GetThumbnailInfoTask::~GetThumbnailInfoTask() = default;

void GetThumbnailInfoTask::Run() {
  prefetch_store_->Execute(
      base::BindOnce(GetThumbnailInfoSync, offline_id_),
      base::BindOnce(&GetThumbnailInfoTask::CompleteTaskAndForwardResult,
                     weak_factory_.GetWeakPtr()),
      Result());
}

void GetThumbnailInfoTask::CompleteTaskAndForwardResult(Result result) {
  TaskComplete();
  std::move(callback_).Run(result);
}

}  // namespace offline_pages
