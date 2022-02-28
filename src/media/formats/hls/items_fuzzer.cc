// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>
#include <cstdint>

#include "base/check.h"
#include "base/strings/string_piece.h"
#include "media/formats/hls/items.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace {

bool IsSubstring(base::StringPiece sub, base::StringPiece base) {
  return base.data() <= sub.data() &&
         base.data() + base.size() >= sub.data() + sub.size();
}

media::hls::SourceString GetItemContent(media::hls::TagItem tag) {
  // Ensure the tag kind returned was valid
  CHECK(tag.kind >= media::hls::TagKind::kUnknown &&
        tag.kind <= media::hls::TagKind::kMaxValue);

  return tag.content;
}

media::hls::SourceString GetItemContent(media::hls::UriItem uri) {
  return uri.content;
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Create a StringPiece from the given input
  const base::StringPiece manifest(reinterpret_cast<const char*>(data), size);
  media::hls::SourceLineIterator iterator{manifest};

  while (true) {
    const auto prev_iterator = iterator;
    auto result = media::hls::GetNextLineItem(&iterator);

    if (result.has_error()) {
      // Ensure that this was an error this function is expected to return
      CHECK(result.code() == media::hls::ParseStatusCode::kReachedEOF ||
            result.code() == media::hls::ParseStatusCode::kInvalidEOL);

      // Ensure that `manifest` is still a substring of the previous manifest
      CHECK(IsSubstring(iterator.SourceForTesting(),
                        prev_iterator.SourceForTesting()));
      CHECK(iterator.CurrentLineForTesting() >=
            prev_iterator.CurrentLineForTesting());
      break;
    }

    auto value = std::move(result).value();
    auto content = absl::visit([](auto x) { return GetItemContent(x); }, value);

    // Ensure that the line number associated with this item is between the
    // original line number and the updated line number
    CHECK(content.Line() >= prev_iterator.CurrentLineForTesting() &&
          content.Line() < iterator.CurrentLineForTesting());

    // Ensure that the content associated with this item is a substring of the
    // previous iterator
    CHECK(IsSubstring(content.Str(), prev_iterator.SourceForTesting()));

    // Ensure that the updated iterator contains a substring of the previous
    // iterator
    CHECK(IsSubstring(iterator.SourceForTesting(),
                      prev_iterator.SourceForTesting()));

    // Ensure that the content associated with this item is NOT a substring of
    // the updated iterator
    CHECK(!IsSubstring(content.Str(), iterator.SourceForTesting()));
  }

  return 0;
}
