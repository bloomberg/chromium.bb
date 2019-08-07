// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_FAKE_QUERYABLE_DATA_H_
#define FUCHSIA_RUNNERS_CAST_FAKE_QUERYABLE_DATA_H_

#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"

// A simple STL map-backed implementation of QueryableData, for testing
// purposes.
class FakeQueryableData : public chromium::cast::QueryableData {
 public:
  FakeQueryableData();
  ~FakeQueryableData() override;

  // Sends, or prepares to send, a set of changed values to the page.
  void SendChanges(
      std::vector<std::pair<base::StringPiece, base::Value&&>> changes);

 private:
  void DeliverChanges();

  // chromium::cast::QueryableData implementation.
  void GetChangedEntries(GetChangedEntriesCallback callback) override;

  base::Optional<GetChangedEntriesCallback> get_changed_callback_;
  std::vector<chromium::cast::QueryableDataEntry> changes_;

  DISALLOW_COPY_AND_ASSIGN(FakeQueryableData);
};

#endif  // FUCHSIA_RUNNERS_CAST_FAKE_QUERYABLE_DATA_H_
