// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/page/scrolling/text_fragment_finder.h"

#include <memory>

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/range.h"
#include "third_party/blink/renderer/core/editing/ephemeral_range.h"
#include "third_party/blink/renderer/core/editing/finder/find_buffer.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_selector.h"

namespace blink {

TextFragmentFinder::TextFragmentFinder(Client& client,
                                       const TextFragmentSelector& selector)
    : client_(client), selector_(selector) {}

void TextFragmentFinder::FindMatch(Document& document) {
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
    // TODO(bokan): We need to add the capability to match a snippet based on
    // it's start and end. https://crbug.com/924964.
    std::unique_ptr<FindBuffer::Results> match_results =
        buffer.FindMatches(selector_.Start(), find_options);

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

PositionInFlatTree TextFragmentFinder::FindStart() {
  return PositionInFlatTree();
}

PositionInFlatTree TextFragmentFinder::FindEnd() {
  return PositionInFlatTree();
}

}  // namespace blink
