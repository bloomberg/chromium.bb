// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/runners/cast/fake_queryable_data.h"

#include <string>
#include <utility>
#include <vector>

#include "base/json/json_writer.h"

FakeQueryableData::FakeQueryableData() = default;

FakeQueryableData::~FakeQueryableData() = default;

void FakeQueryableData::SendChanges(
    std::vector<std::pair<base::StringPiece, base::Value&&>> changes) {
  for (const auto& change : changes) {
    std::string value_json;
    CHECK(base::JSONWriter::Write(change.second, &value_json));
    changes_.push_back({change.first.as_string(), value_json});
  }

  if (get_changed_callback_)
    DeliverChanges();
}

void FakeQueryableData::GetChangedEntries(GetChangedEntriesCallback callback) {
  DCHECK(!get_changed_callback_);
  get_changed_callback_ = std::move(callback);

  if (!changes_.empty())
    DeliverChanges();
}

void FakeQueryableData::DeliverChanges() {
  (*get_changed_callback_)(std::move(changes_));
  get_changed_callback_ = base::nullopt;
}
