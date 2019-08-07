// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/download_manager.h"

#include "components/download/public/common/download_task_runner.h"

namespace content {

// static
scoped_refptr<base::SequencedTaskRunner> DownloadManager::GetTaskRunner() {
  return download::GetDownloadTaskRunner();
}

}  // namespace content
