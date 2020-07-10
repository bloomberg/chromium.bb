// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DISCOVERY_MDNS_TESTING_MDNS_TEST_UTIL_H_
#define DISCOVERY_MDNS_TESTING_MDNS_TEST_UTIL_H_

#include <initializer_list>

#include "absl/strings/string_view.h"
#include "discovery/mdns/mdns_records.h"

namespace openscreen {
namespace discovery {

TxtRecordRdata MakeTxtRecord(std::initializer_list<absl::string_view> strings);

}  // namespace discovery
}  // namespace openscreen

#endif  // DISCOVERY_MDNS_TESTING_MDNS_TEST_UTIL_H_
