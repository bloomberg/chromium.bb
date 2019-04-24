// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FUCHSIA_RUNNERS_CAST_FAKE_QUERYABLE_DATA_H_
#define FUCHSIA_RUNNERS_CAST_FAKE_QUERYABLE_DATA_H_

#include <map>
#include <string>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/values.h"
#include "fuchsia/fidl/chromium/cast/cpp/fidl.h"

// A simple STL map-backed implementation of QueryableData, for testing
// purposes.
class FakeQueryableData : public chromium::cast::QueryableData {
 public:
  FakeQueryableData();
  ~FakeQueryableData() override;

  void Add(base::StringPiece key, const base::Value& value);

  // chromium::cast::QueryableData implementation.
  void GetChangedEntries(GetChangedEntriesCallback callback) override;

 private:
  std::map<std::string, chromium::cast::QueryableDataEntry> entries_;

  DISALLOW_COPY_AND_ASSIGN(FakeQueryableData);
};

#endif  // FUCHSIA_RUNNERS_CAST_FAKE_QUERYABLE_DATA_H_
