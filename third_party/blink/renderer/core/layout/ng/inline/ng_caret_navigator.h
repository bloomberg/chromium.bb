// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_CARET_NAVIGATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_CARET_NAVIGATOR_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/document_lifecycle.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#include <unicode/ubidi.h>

namespace blink {

class LayoutBlockFlow;
struct NGInlineNodeData;

// Hosts the |text_content| of an inline formatting context and provides
// bidi-related utilities, including checking bidi levels, computing visual
// left/right characters and visual left/right caret movements.
// Design doc: http://bit.ly/2QVAwGq
class CORE_EXPORT NGCaretNavigator {
  STACK_ALLOCATED();

 public:
  explicit NGCaretNavigator(const LayoutBlockFlow&);
  ~NGCaretNavigator();

  const String& GetText() const;
  bool IsBidiEnabled() const;

  // Abstraction of a caret position in |text_|.
  enum class PositionAnchorType { kBefore, kAfter };
  struct Position {
    // |index| is character index the |text_| string.
    unsigned index;
    PositionAnchorType type;

    bool IsBeforeCharacter() const {
      return type == PositionAnchorType::kBefore;
    }

    bool IsAfterCharacter() const { return type == PositionAnchorType::kAfter; }

    bool operator==(const Position& other) const {
      return index == other.index && type == other.type;
    }
  };

  // Returns the bidi level or resolved direction of the character at the given
  // logical |index|.
  UBiDiLevel BidiLevelAt(unsigned index) const;
  TextDirection TextDirectionAt(unsigned index) const;

  // Returns true if the characters at indexes |offset - 1| and |offset| both
  // exist and are at different bidi levels.
  bool OffsetIsBidiBoundary(unsigned offset) const;

  // Converts an (offset, affinity) pair into a |Position| type of this class.
  // Intiontionally long name to indicate the hackiness for handling legacy
  // callers.
  Position CaretPositionFromTextContentOffsetAndAffinity(
      unsigned offset,
      TextAffinity affinity) const;

  // Returns the visual left/right edge caret position of the character at the
  // given logical |index|.
  Position LeftEdgeOf(unsigned index) const;
  Position RightEdgeOf(unsigned index) const;

  // Left/right visual movements
  // TODO(xiaochengh): Handle the following
  // - Grapheme clusters

  enum class VisualMovementResultType {
    kWithinContext,
    kBeforeContext,
    kAfterContext,
    kEnteredChildContext
  };

  // Given the character at the logical |index|, returns the logical index of
  // the character at its left/right side.
  struct VisualCharacterMovementResult {
    bool IsWithinContext() const {
      return type == VisualMovementResultType::kWithinContext;
    }
    bool IsBeforeContext() const {
      return type == VisualMovementResultType::kBeforeContext;
    }
    bool IsAfterContext() const {
      return type == VisualMovementResultType::kAfterContext;
    }

    VisualMovementResultType type;
    base::Optional<unsigned> index;
  };
  VisualCharacterMovementResult LeftCharacterOf(unsigned index) const;
  VisualCharacterMovementResult RightCharacterOf(unsigned index) const;

  // Given a caret position, moves it left/right by one grapheme cluster and
  // returns the result.
  // Note: If we end up entering an inline block, the result |Position| is
  // either before or after the inline block, depending on from which side the
  // inline block is entered. For example:
  // RightPositionOf(abc|<inline-block>def</inline-block>ghi)
  // -> {inline-block, PositionAnchorType::kBefore}
  // LeftPositionOf(abc<inline-block>def</inline-block>|ghi)
  // -> {inline-block, PositionAnchorType::kAfter}
  struct VisualCaretMovementResult {
    bool IsWithinContext() const {
      return type == VisualMovementResultType::kWithinContext;
    }
    bool IsBeforeContext() const {
      return type == VisualMovementResultType::kBeforeContext;
    }
    bool IsAfterContext() const {
      return type == VisualMovementResultType::kAfterContext;
    }
    bool HasEnteredChildContext() const {
      return type == VisualMovementResultType::kEnteredChildContext;
    }

    VisualMovementResultType type;
    base::Optional<Position> position;
  };
  VisualCaretMovementResult LeftPositionOf(const Position&) const;
  VisualCaretMovementResult RightPositionOf(const Position&) const;

  // TODO(xiaochengh): Specify and implement the behavior in edge cases, e.g.,
  // when the leftmost character of the first line is CSS-generated.
  Position LeftmostPositionInFirstLine() const;
  Position RightmostPositionInFirstLine() const;
  Position LeftmostPositionInLastLine() const;
  Position RightmostPositionInLastLine() const;

 private:
  // A caret position is invalid if it is:
  // - kAfter to a line break character.
  // - Anchored to a collapsible space that's removed by line wrap.
  // - Anchored to a character that's ignored in caret movement.
  bool IsValidCaretPosition(const Position&) const;
  bool IsLineBreak(unsigned index) const;
  bool IsCollapsibleWhitespace(unsigned index) const;
  bool IsCollapsedSpaceByLineWrap(unsigned index) const;
  bool IsIgnoredInCaretMovement(unsigned index) const;

  // Returns true if the character at |index| represents a child block
  // formatting context that can be entered by caret navigation. Such contexts
  // must be atomic inlines (inline block, inline table, ...) and must not host
  // user agent shadow tree (which excludes, e.g., <input> and image alt text).
  bool IsEnterableChildContext(unsigned index) const;

  enum class MoveDirection { kTowardsLeft, kTowardsRight };
  static MoveDirection OppositeDirectionOf(MoveDirection);
  static bool TowardsSameDirection(MoveDirection, TextDirection);

  // ------ Line-related functions ------

  // A line contains a consecutive substring of |GetText()|. The lines should
  // not overlap, and should together cover the entire |GetText()|.
  struct Line {
    unsigned start_offset;
    unsigned end_offset;
    TextDirection base_direction;
  };
  Line ContainingLineOf(unsigned index) const;
  Vector<int32_t, 32> CharacterIndicesInVisualOrder(const Line&) const;
  unsigned VisualMostForwardCharacterOf(const Line&,
                                        MoveDirection direction) const;
  unsigned VisualLastCharacterOf(const Line&) const;
  unsigned VisualFirstCharacterOf(const Line&) const;

  // ------ Implementation of public visual movement functions ------

  Position EdgeOfInternal(unsigned index, MoveDirection) const;
  VisualCharacterMovementResult MoveCharacterInternal(unsigned index,
                                                      MoveDirection) const;
  VisualCaretMovementResult MoveCaretInternal(const Position&,
                                              MoveDirection) const;

  // Performs a "minimal" caret movement to the left/right without validating
  // the result. The result might be invalid due to, e.g., anchored to an
  // unallowed character, being visually the same as the input, etc. It's a
  // subroutine of |MoveCaretInternal|, who keeps calling it until both of the
  // folliwng are satisfied:
  // - We've reached a valid caret position.
  // - During the process, the caret has moved passing a character on which
  // |IsIgnoredInCaretMovement| is false (indicated by |has_passed_character|).
  struct UnvalidatedVisualCaretMovementResult {
    VisualMovementResultType type;
    base::Optional<Position> position;
    bool has_passed_character = false;
  };
  UnvalidatedVisualCaretMovementResult MoveCaretWithoutValidation(
      const Position&,
      MoveDirection) const;

  const NGInlineNodeData& GetData() const;

  const LayoutBlockFlow& context_;
  DocumentLifecycle::DisallowTransitionScope disallow_transition_;
};

CORE_EXPORT std::ostream& operator<<(std::ostream&,
                                     const NGCaretNavigator::Position&);

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_CARET_NAVIGATOR_H_
