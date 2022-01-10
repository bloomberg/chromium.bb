// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/entity_metadata.h"

#include <string>
#include <vector>

#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"

namespace optimization_guide {

EntityMetadata::EntityMetadata() = default;
EntityMetadata::EntityMetadata(
    const std::string& entity_id,
    const std::string& human_readable_name,
    const base::flat_map<std::string, float>& human_readable_categories)
    : entity_id(entity_id),
      human_readable_name(human_readable_name),
      human_readable_categories(human_readable_categories) {}
EntityMetadata::EntityMetadata(const EntityMetadata&) = default;
EntityMetadata::~EntityMetadata() = default;

std::string EntityMetadata::ToString() const {
  std::vector<std::string> categories;
  for (const auto& iter : human_readable_categories) {
    categories.push_back(
        base::StringPrintf("{%s,%f}", iter.first.c_str(), iter.second));
  }
  return base::StringPrintf("EntityMetadata{%s, %s, {%s}}", entity_id.c_str(),
                            human_readable_name.c_str(),
                            base::JoinString(categories, ",").c_str());
}

std::ostream& operator<<(std::ostream& out, const EntityMetadata& md) {
  out << md.ToString();
  return out;
}

bool operator==(const EntityMetadata& lhs, const EntityMetadata& rhs) {
  return lhs.entity_id == rhs.entity_id &&
         lhs.human_readable_name == rhs.human_readable_name &&
         lhs.human_readable_categories == rhs.human_readable_categories;
}

ScoredEntityMetadata::ScoredEntityMetadata() = default;
ScoredEntityMetadata::ScoredEntityMetadata(float score,
                                           const EntityMetadata& md)
    : metadata(md), score(score) {}
ScoredEntityMetadata::ScoredEntityMetadata(const ScoredEntityMetadata&) =
    default;
ScoredEntityMetadata::~ScoredEntityMetadata() = default;

std::string ScoredEntityMetadata::ToString() const {
  return base::StringPrintf("ScoredEntityMetadata{%f, %s}", score,
                            metadata.ToString().c_str());
}

std::ostream& operator<<(std::ostream& out, const ScoredEntityMetadata& md) {
  out << md.ToString();
  return out;
}

bool operator==(const ScoredEntityMetadata& lhs,
                const ScoredEntityMetadata& rhs) {
  constexpr const double kScoreTolerance = 1e-6;
  return lhs.metadata == rhs.metadata &&
         abs(lhs.score - rhs.score) <= kScoreTolerance;
}

}  // namespace optimization_guide
