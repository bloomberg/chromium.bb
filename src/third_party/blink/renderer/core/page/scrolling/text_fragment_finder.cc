// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/text_fragment_finder.h"

#include <memory>

#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/finder/find_buffer.h"
#include "third_party/blink/renderer/core/editing/position.h"

namespace blink {

TextFragmentFinder::TextFragmentFinder(Client& client) : client_(client) {}

void TextFragmentFinder::FindMatch(const String& search_text,
                                   Document& document) {
  search_text_ = search_text;
  PositionInFlatTree search_start =
      PositionInFlatTree::FirstPositionInNode(document);

  PositionInFlatTree search_end;
  if (document.documentElement() && document.documentElement()->lastChild()) {
    search_end =
        PositionInFlatTree::AfterNode(*document.documentElement()->lastChild());
  } else {
    search_end = PositionInFlatTree::LastPositionInNode(document);
  }

  // TODO(bokan): Make FindMatch work asynchronously. https://crbug.com/930156.
  while (search_start != search_end) {
    // Find in the whole block.
    FindBuffer buffer(EphemeralRangeInFlatTree(search_start, search_end));
    const FindOptions find_options = kCaseInsensitive;
    std::unique_ptr<FindBuffer::Results> match_results =
        buffer.FindMatches(search_text_, find_options);

    if (!match_results->IsEmpty()) {
      FindBuffer::BufferMatchResult match = match_results->front();
      const EphemeralRangeInFlatTree ephemeral_match_range =
          buffer.RangeFromBufferIndex(match.start, match.start + match.length);
      client_.DidFindMatch(ephemeral_match_range);
      break;
    }

    // At this point, all text in the block collected above has been
    // processed. Now we move to the next block if there's any,
    // otherwise we should stop.
    search_start = buffer.PositionAfterBlock();
    if (search_start.IsNull())
      break;
  }
}

}  // namespace blink
