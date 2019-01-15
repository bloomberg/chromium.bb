// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_CARET_NAVIGATOR_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_CARET_NAVIGATOR_H_

#include "base/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/editing/text_affinity.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"

#include <unicode/ubidi.h>

namespace blink {

struct NGInlineNodeData;

// Hosts the |text_content| of an inline formatting context and provides
// bidi-related utilities, including checking bidi levels, computing visual
// left/right characters and visual left/right caret movements.
// Design doc: http://bit.ly/2QVAwGq
class CORE_EXPORT NGCaretNavigator {
 public:
  NGCaretNavigator(NGCaretNavigator&&);
  explicit NGCaretNavigator(const NGInlineNodeData&);
  ~NGCaretNavigator();

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
  // - Enterable atomic inlines
  // - Editing-ignored contents
  // - Multiple lines

  enum class VisualMovementResultType {
    kWithinContext,
    kBeforeContext,
    kAfterContext
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

    VisualMovementResultType type;
    base::Optional<Position> position;
  };
  VisualCaretMovementResult LeftPositionOf(const Position&) const;
  VisualCaretMovementResult RightPositionOf(const Position&) const;

 private:
  enum class MoveDirection { kTowardsLeft, kTowardsRight };
  static MoveDirection OppositeDirectionOf(MoveDirection);
  static bool TowardsSameDirection(MoveDirection, TextDirection);
  static base::Optional<unsigned> MoveVisualIndex(unsigned index,
                                                  unsigned length,
                                                  MoveDirection);

  Position EdgeOfInternal(unsigned index, MoveDirection) const;
  VisualCharacterMovementResult MoveCharacterInternal(unsigned index,
                                                      MoveDirection) const;
  VisualCaretMovementResult MoveCaretInternal(const Position&,
                                              MoveDirection) const;

  String text_;

  // TODO(xiaochengh): Add line-aware index.
  // TODO(xiaochengh): Reduce memory consumption.
  Vector<UBiDiLevel, 32> bidi_levels_;
  Vector<int32_t, 32> indices_in_visual_order_;
  Vector<int32_t, 32> visual_indices_;

  bool is_bidi_enabled_;
  TextDirection base_direction_;

  DISALLOW_COPY_AND_ASSIGN(NGCaretNavigator);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_LAYOUT_NG_INLINE_NG_CARET_NAVIGATOR_H_
