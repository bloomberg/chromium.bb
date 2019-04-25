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

namespace {

EphemeralRangeInFlatTree FindMatchFromPosition(
    String search_text,
    Document& document,
    PositionInFlatTree search_start) {
  if (search_text.IsEmpty())
    return EphemeralRangeInFlatTree();

  PositionInFlatTree search_end;
  if (document.documentElement() && document.documentElement()->lastChild()) {
    search_end =
        PositionInFlatTree::AfterNode(*document.documentElement()->lastChild());
  } else {
    search_end = PositionInFlatTree::LastPositionInNode(document);
  }

  while (search_start != search_end) {
    // Find in the whole block.
    FindBuffer buffer(EphemeralRangeInFlatTree(search_start, search_end));
    const FindOptions find_options = kCaseInsensitive;

    std::unique_ptr<FindBuffer::Results> match_results =
        buffer.FindMatches(search_text, find_options);

    if (!match_results->IsEmpty()) {
      FindBuffer::BufferMatchResult match = match_results->front();
      return buffer.RangeFromBufferIndex(match.start,
                                         match.start + match.length);
    }

    // At this point, all text in the block collected above has been
    // processed. Now we move to the next block if there's any,
    // otherwise we should stop.
    search_start = buffer.PositionAfterBlock();
    if (search_start.IsNull())
      break;
  }

  return EphemeralRangeInFlatTree();
}

}  // namespace

TextFragmentFinder::TextFragmentFinder(Client& client,
                                       const TextFragmentSelector& selector)
    : client_(client), selector_(selector) {
  DCHECK(!selector_.Start().IsEmpty());
}

void TextFragmentFinder::FindMatch(Document& document) {
  // TODO(crbug.com/930156): Make FindMatch work asynchronously.
  EphemeralRangeInFlatTree start_match =
      FindMatchFromPosition(selector_.Start(), document,
                            PositionInFlatTree::FirstPositionInNode(document));
  if (start_match.IsNull())
    return;

  if (selector_.End().IsEmpty()) {
    client_.DidFindMatch(start_match);
  } else {
    // TODO(crbug.com/924964): Determine what we should do if the start text and
    // end text are the same (and there are no context terms). This
    // implementation continues searching for the next instance of the text,
    // from the end of the first instance.
    EphemeralRangeInFlatTree end_match = FindMatchFromPosition(
        selector_.End(), document, start_match.EndPosition());
    if (!end_match.IsNull()) {
      client_.DidFindMatch(EphemeralRangeInFlatTree(start_match.StartPosition(),
                                                    end_match.EndPosition()));
    }
  }
}

}  // namespace blink
