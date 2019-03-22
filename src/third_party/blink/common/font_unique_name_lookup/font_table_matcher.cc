// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/common/font_unique_name_lookup/font_table_matcher.h"
#include "base/strings/utf_string_conversions.h"
#include "third_party/blink/public/common/font_unique_name_lookup/icu_fold_case_util.h"

#include <algorithm>

namespace blink {

FontTableMatcher::FontTableMatcher(
    const base::ReadOnlySharedMemoryMapping& mapping) {
  font_table_.ParseFromArray(mapping.memory(), mapping.size());
}

// static
base::ReadOnlySharedMemoryMapping
FontTableMatcher::MemoryMappingFromFontUniqueNameTable(
    const FontUniqueNameTable& font_unique_name_table) {
  size_t serialization_size = font_unique_name_table.ByteSizeLong();
  CHECK(serialization_size);
  base::MappedReadOnlyRegion mapped_region =
      base::ReadOnlySharedMemoryRegion::Create(serialization_size);
  font_unique_name_table.SerializeToArray(mapped_region.mapping.memory(),
                                          mapped_region.mapping.mapped_size());
  return mapped_region.region.Map();
}

base::Optional<FontTableMatcher::MatchResult> FontTableMatcher::MatchName(
    const std::string& name_request) const {
  std::string folded_name_request = IcuFoldCase(name_request);
  const auto& font_entries = font_table_.font_entries();
  auto find_result = std::find_if(
      font_entries.begin(), font_entries.end(),
      [&folded_name_request](
          const FontUniqueNameTable_FontUniqueNameEntry& entry) {
        return !entry.postscript_name().compare(folded_name_request) ||
               !entry.full_name().compare(folded_name_request);
      });
  if (find_result != font_entries.end()) {
    return base::Optional<MatchResult>(
        {find_result->file_path(), find_result->ttc_index()});
  }
  return {};
}

size_t FontTableMatcher::AvailableFonts() const {
  return font_table_.font_entries_size();
}

bool FontTableMatcher::FontListIsDisjointFrom(
    const FontTableMatcher& other) const {
  std::vector<std::string> paths_self, paths_other, intersection_result;
  for (const auto& indexed_font : font_table_.font_entries()) {
    paths_self.push_back(indexed_font.file_path());
  }
  for (const auto& indexed_font_other : other.font_table_.font_entries()) {
    paths_other.push_back(indexed_font_other.file_path());
  }
  std::sort(paths_self.begin(), paths_self.end());
  std::sort(paths_other.begin(), paths_other.end());
  std::set_intersection(paths_self.begin(), paths_self.end(),
                        paths_other.begin(), paths_other.end(),
                        std::back_inserter(intersection_result));
  return intersection_result.empty();
}

}  // namespace blink
