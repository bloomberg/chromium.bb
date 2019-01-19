// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_FINDER_FIND_BUFFER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_FINDER_FIND_BUFFER_H_

#include "third_party/blink/public/mojom/frame/find_in_page.mojom-blink.h"
#include "third_party/blink/renderer/core/editing/finder/find_buffer.h"
#include "third_party/blink/renderer/core/editing/iterators/text_searcher_icu.h"
#include "third_party/blink/renderer/core/layout/ng/inline/ng_inline_node.h"

namespace blink {

class LayoutBlockFlow;
class Node;
class WebString;

// Buffer for find-in-page, collects text until it meets a block/other
// delimiters. Uses TextSearcherICU to find match in buffer.
// See doc at https://goo.gl/rnXjBu
class CORE_EXPORT FindBuffer {
  STACK_ALLOCATED();

 public:
  FindBuffer(const PositionInFlatTree& start_position);

  // A match result, containing the starting position of the match and
  // the length of the match.
  struct BufferMatchResult {
    const unsigned start;
    const unsigned length;
  };

  // All match results for this buffer. We can iterate through the
  // BufferMatchResults one by one using the Iterator.
  class CORE_EXPORT Results {
   public:
    Results();

    Results(const Vector<UChar>& buffer,
            String search_text,
            const mojom::blink::FindOptions& options);

    class CORE_EXPORT Iterator
        : public std::iterator<std::forward_iterator_tag, BufferMatchResult> {
     public:
      Iterator() = default;
      Iterator(TextSearcherICU* text_searcher);

      bool operator==(const Iterator& other) {
        return has_match_ == other.has_match_;
      }

      bool operator!=(const Iterator& other) {
        return has_match_ != other.has_match_;
      }

      const BufferMatchResult operator*() const;

      void operator++();

     private:
      TextSearcherICU* text_searcher_;
      MatchResultICU match_;
      bool has_match_ = false;
    };

    Iterator begin();

    Iterator end() const;

    unsigned CountForTesting();

   private:
    bool empty_result_ = false;
    String search_text_;
    TextSearcherICU text_searcher_;
  };

  // Finds all the match for |search_text| in |buffer_|.
  std::unique_ptr<Results> FindMatches(
      const WebString& search_text,
      const mojom::blink::FindOptions& options) const;

  // Gets a flat tree range corresponding to text in the [start_index,
  // end_index) of |buffer|.
  EphemeralRangeInFlatTree RangeFromBufferIndex(unsigned start_index,
                                                unsigned end_index) const;

  PositionInFlatTree PositionAfterBlock() const {
    if (!node_after_block_)
      return PositionInFlatTree();
    return PositionInFlatTree::FirstPositionInNode(*node_after_block_);
  }

 private:
  // Collects text for one LayoutBlockFlow located at or after |start_node|
  // to |buffer_|, might be stopped without finishing one full LayoutBlockFlow
  // if we encountered another LayoutBLockFlow. Saves the next starting node
  // after the block (first node in another LayoutBlockFlow) to
  // |node_after_block_|.
  void CollectTextUntilBlockBoundary(Node& start_node);

  class CORE_EXPORT InvisibleLayoutScope {
    STACK_ALLOCATED();

   public:
    InvisibleLayoutScope() {}
    ~InvisibleLayoutScope();

    void EnsureRecalc(Node& block_root);
    bool DidRecalc() { return did_recalc_; }

   private:
    bool did_recalc_ = false;
    Member<Element> invisible_root_;
  };

  // Mapping for position in buffer -> actual node where the text came from,
  // along with the offset in the NGOffsetMapping of this find_buffer.
  // This is needed because when we find a match in the buffer, we want to know
  // where it's located in the NGOffsetMapping for this FindBuffer.
  // Example: (assume there are no whitespace)
  // <div>
  //  aaa
  //  <span style="float:right;">bbb<span>ccc</span></span>
  //  ddd
  // </div>
  // We can finish FIP with three FindBuffer runs:
  // Run #1, 1 BufferNodeMapping with mapping text = "aaa\uFFFCddd",
  // The "\uFFFC" is the object replacement character created by the float.
  // For text node "aaa", oib = 0, oim = 0.
  // Content of |buffer_| = "aaa".
  // Run #2, 2 BufferNodeMappings, with mapping text = "bbbccc",
  //  1. For text node "bbb", oib = 0, oim = 0.
  //  2. For text node "ccc", oib = 3, oim = 3.
  // Content of |buffer_| = "bbbccc".
  // Run #3, 1 BufferNodeMapping with mapping text = "aaa\uFFFCddd",
  // For text node "ddd", oib = 0, oim = 4.
  // Content of |buffer_| = "ddd".
  // Since the LayoutBlockFlow for "aaa" and "ddd" is the same, they have the
  // same NGOffsetMapping, the |offset_in_mapping_| for the BufferNodeMapping in
  // run #3 is 4 (the index of first "d" character in the mapping text).
  struct BufferNodeMapping {
    const unsigned offset_in_buffer;
    const unsigned offset_in_mapping;
  };

  BufferNodeMapping MappingForIndex(unsigned index) const;

  PositionInFlatTree PositionAtStartOfCharacterAtIndex(unsigned index) const;

  PositionInFlatTree PositionAtEndOfCharacterAtIndex(unsigned index) const;

  void AddTextToBuffer(const Text& text_node, LayoutBlockFlow& block_flow);

  InvisibleLayoutScope invisible_layout_scope_;
  Member<Node> node_after_block_;
  Vector<UChar> buffer_;
  Vector<BufferNodeMapping> buffer_node_mappings_;

  // For legacy layout, we need to save a unique_ptr of the NGOffsetMapping
  // because nobody owns it. In LayoutNG, the NGOffsetMapping is owned by
  // the corresponding LayoutBlockFlow, so we don't need to save it.
  std::unique_ptr<NGOffsetMapping> offset_mapping_storage_;
  const NGOffsetMapping* offset_mapping_ = nullptr;

  bool mapping_needs_recalc_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_EDITING_FINDER_FIND_BUFFER_H_
