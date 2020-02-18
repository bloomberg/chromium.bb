// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "discovery/mdns/testing/mdns_test_util.h"

#include <utility>
#include <vector>

namespace openscreen {
namespace discovery {

TxtRecordRdata MakeTxtRecord(std::initializer_list<absl::string_view> strings) {
  std::vector<TxtRecordRdata::Entry> texts;
  for (const auto& string : strings) {
    texts.push_back(TxtRecordRdata::Entry(string.begin(), string.end()));
  }
  return TxtRecordRdata(std::move(texts));
}

}  // namespace discovery
}  // namespace openscreen
