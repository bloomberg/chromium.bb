// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/reporting/client/empty_dm_token_retriever.h"

#include <utility>

#include "base/callback.h"

namespace reporting {

void EmptyDMTokenRetriever::RetrieveDMToken(
    DMTokenRetriever::CompletionCallback completion_cb) {
  std::move(completion_cb).Run("");
}

}  // namespace reporting
