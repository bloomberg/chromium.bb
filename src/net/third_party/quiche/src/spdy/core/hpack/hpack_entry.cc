// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "spdy/core/hpack/hpack_entry.h"

#include "absl/strings/str_cat.h"
#include "spdy/platform/api/spdy_estimate_memory_usage.h"
#include "spdy/platform/api/spdy_logging.h"

namespace spdy {

HpackEntry::HpackEntry(std::string name, std::string value)
    : name_(std::move(name)), value_(std::move(value)) {}

// static
size_t HpackEntry::Size(absl::string_view name, absl::string_view value) {
  return name.size() + value.size() + kHpackEntrySizeOverhead;
}
size_t HpackEntry::Size() const {
  return Size(name(), value());
}

std::string HpackEntry::GetDebugString() const {
  return absl::StrCat("{ name: \"", name_, "\", value: \"", value_, "\" }");
}

size_t HpackEntry::EstimateMemoryUsage() const {
  return SpdyEstimateMemoryUsage(name_) + SpdyEstimateMemoryUsage(value_);
}

}  // namespace spdy
