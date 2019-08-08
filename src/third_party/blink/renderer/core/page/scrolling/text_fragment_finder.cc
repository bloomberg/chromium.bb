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
#include "third_party/blink/renderer/core/editing/iterators/character_iterator.h"
#include "third_party/blink/renderer/core/editing/position.h"
#include "third_party/blink/renderer/core/page/scrolling/text_fragment_selector.h"

namespace blink {

namespace {

const char kNoContext[] = "";

EphemeralRangeInFlatTree FindMatchInRange(String search_text,
                                          PositionInFlatTree search_start,
                                          PositionInFlatTree search_end) {
  const FindOptions find_options = kCaseInsensitive;
  const EphemeralRangeInFlatTree search_range(search_start, search_end);
  return FindBuffer::FindMatchInRange(search_range, search_text, find_options);
}

PositionInFlatTree NextTextPosition(PositionInFlatTree position,
                                    PositionInFlatTree end_position) {
  const TextIteratorBehavior options =
      TextIteratorBehavior::Builder().SetEmitsSpaceForNbsp(true).Build();
  CharacterIteratorInFlatTree char_it(position, end_position, options);

  for (; char_it.length(); char_it.Advance(1)) {
    if (!IsSpaceOrNewline(char_it.CharacterAt(0)))
      return char_it.StartPosition();
  }

  return end_position;
}

// Find and return the range of |search_text| if the first text in the search
// range is |search_text|, skipping over whitespace and element boundaries.
EphemeralRangeInFlatTree FindImmediateMatch(String search_text,
                                            PositionInFlatTree search_start,
                                            PositionInFlatTree search_end) {
  if (search_text.IsEmpty())
    return EphemeralRangeInFlatTree();

  search_start = NextTextPosition(search_start, search_end);
  if (search_start == search_end)
    return EphemeralRangeInFlatTree();

  FindBuffer buffer(EphemeralRangeInFlatTree(search_start, search_end));
  const FindOptions find_options = kCaseInsensitive;

  std::unique_ptr<FindBuffer::Results> match_results =
      buffer.FindMatches(search_text, find_options);

  if (!match_results->IsEmpty() && match_results->front().start == 0u) {
    FindBuffer::BufferMatchResult match = match_results->front();
    return buffer.RangeFromBufferIndex(match.start, match.start + match.length);
  }

  return EphemeralRangeInFlatTree();
}

EphemeralRangeInFlatTree FindMatchInRangeWithContext(
    const String& search_text,
    const String& prefix,
    const String& suffix,
    PositionInFlatTree search_start,
    PositionInFlatTree search_end) {
  while (search_start != search_end) {
    EphemeralRangeInFlatTree potential_match;

    if (!prefix.IsEmpty()) {
      EphemeralRangeInFlatTree prefix_match =
          FindMatchInRange(prefix, search_start, search_end);

      // No prefix_match in remaining range
      if (prefix_match.IsNull())
        return EphemeralRangeInFlatTree();

      search_start = prefix_match.EndPosition();
      potential_match =
          FindImmediateMatch(search_text, search_start, search_end);

      // No search_text match after current prefix_match
      if (potential_match.IsNull())
        continue;
    } else {
      potential_match = FindMatchInRange(search_text, search_start, search_end);

      // No search_text match in remaining range
      if (potential_match.IsNull())
        return EphemeralRangeInFlatTree();
    }

    DCHECK(potential_match.IsNotNull());
    search_start = potential_match.EndPosition();
    if (!suffix.IsEmpty()) {
      EphemeralRangeInFlatTree suffix_match =
          FindImmediateMatch(suffix, search_start, search_end);

      // No suffix match after current potential_match
      if (suffix_match.IsNull())
        continue;
    }

    // If we reach here without a return or continue, we have a full match.
    return potential_match;
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
  PositionInFlatTree search_start =
      PositionInFlatTree::FirstPositionInNode(document);

  EphemeralRangeInFlatTree match =
      FindMatchFromPosition(document, search_start);

  if (match.IsNotNull()) {
    client_.DidFindMatch(match);

    // Continue searching to see if we have an ambiguous selector.
    // TODO(crbug.com/919204): This is temporary and only for measuring
    // ambiguous matching during prototyping.
    EphemeralRangeInFlatTree ambiguous_match =
        FindMatchFromPosition(document, match.EndPosition());
    if (ambiguous_match.IsNotNull())
      client_.DidFindAmbiguousMatch();
  }
}

EphemeralRangeInFlatTree TextFragmentFinder::FindMatchFromPosition(
    Document& document,
    PositionInFlatTree search_start) {
  PositionInFlatTree search_end;
  if (document.documentElement() && document.documentElement()->lastChild()) {
    search_end =
        PositionInFlatTree::AfterNode(*document.documentElement()->lastChild());
  } else {
    search_end = PositionInFlatTree::LastPositionInNode(document);
  }

  // TODO(crbug.com/930156): Make FindMatch work asynchronously.
  EphemeralRangeInFlatTree match;
  if (selector_.Type() == TextFragmentSelector::kExact) {
    match = FindMatchInRangeWithContext(selector_.Start(), selector_.Prefix(),
                                        selector_.Suffix(), search_start,
                                        search_end);
  } else {
    EphemeralRangeInFlatTree start_match =
        FindMatchInRangeWithContext(selector_.Start(), selector_.Prefix(),
                                    kNoContext, search_start, search_end);
    if (start_match.IsNull())
      return start_match;

    // TODO(crbug.com/924964): Determine what we should do if the start text and
    // end text are the same (and there are no context terms). This
    // implementation continues searching for the next instance of the text,
    // from the end of the first instance.
    search_start = start_match.EndPosition();
    EphemeralRangeInFlatTree end_match = FindMatchInRangeWithContext(
        selector_.End(), kNoContext, selector_.Suffix(), search_start,
        search_end);
    if (end_match.IsNotNull()) {
      match = EphemeralRangeInFlatTree(start_match.StartPosition(),
                                       end_match.EndPosition());
    }
  }

  return match;
}

}  // namespace blink
