// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/heap_snapshot_taker.h"

#include <stddef.h>
#include <utility>

#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/devtools_client.h"
#include "chrome/test/chromedriver/chrome/status.h"

HeapSnapshotTaker::HeapSnapshotTaker(DevToolsClient* client)
    : client_(client) {
  client_->AddListener(this);
}

HeapSnapshotTaker::~HeapSnapshotTaker() {}

Status HeapSnapshotTaker::TakeSnapshot(std::unique_ptr<base::Value>* snapshot) {
  Status status1 = TakeSnapshotInternal();
  base::DictionaryValue params;
  Status status2 = client_->SendCommand("Debugger.disable", params);

  Status status3(kOk);
  if (status1.IsOk() && status2.IsOk()) {
    std::unique_ptr<base::Value> value = base::JSONReader::Read(snapshot_);
    if (!value) {
      status3 = Status(kUnknownError, "heap snapshot not in JSON format");
    } else {
      *snapshot = std::move(value);
    }
  }
  snapshot_.clear();
  if (status1.IsError()) {
    return status1;
  } else if (status2.IsError()) {
    return status2;
  } else {
    return status3;
  }
}

Status HeapSnapshotTaker::TakeSnapshotInternal() {
  base::DictionaryValue params;
  const char* const kMethods[] = {
      "Debugger.enable",
      "HeapProfiler.collectGarbage",
      "HeapProfiler.takeHeapSnapshot"
  };
  for (size_t i = 0; i < arraysize(kMethods); ++i) {
    Status status = client_->SendCommand(kMethods[i], params);
    if (status.IsError())
      return status;
  }

  return Status(kOk);
}

Status HeapSnapshotTaker::OnEvent(DevToolsClient* client,
                                  const std::string& method,
                                  const base::DictionaryValue& params) {
  if (method == "HeapProfiler.addHeapSnapshotChunk") {
    std::string chunk;
    if (!params.GetString("chunk", &chunk)) {
      return Status(kUnknownError,
                    "HeapProfiler.addHeapSnapshotChunk has no 'chunk'");
    }
    snapshot_.append(chunk);
  }
  return Status(kOk);
}
