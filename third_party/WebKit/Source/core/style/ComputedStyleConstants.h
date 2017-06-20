/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights
 * reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef ComputedStyleConstants_h
#define ComputedStyleConstants_h

#include "core/ComputedStyleBaseConstants.h"
#include <cstddef>

namespace blink {

template <typename Enum>
inline bool EnumHasFlags(Enum v, Enum mask) {
  return static_cast<unsigned>(v) & static_cast<unsigned>(mask);
}

// Some enums are automatically generated in ComputedStyleBaseConstants

// TODO(sashab): Change these enums to enum classes with an unsigned underlying
// type. Enum classes provide better type safety, and forcing an unsigned
// underlying type prevents msvc from interpreting enums as negative numbers.
// See: crbug.com/628043

// Sides used when drawing borders and outlines. The values should run clockwise
// from top.
enum BoxSide { kBSTop, kBSRight, kBSBottom, kBSLeft };

// See core/dom/stylerecalc.md for an explanation on what each state means
enum StyleRecalcChange {
  kNoChange,
  kNoInherit,
  kUpdatePseudoElements,
  kIndependentInherit,
  kInherit,
  kForce,
  kReattach
};

// Static pseudo styles. Dynamic ones are produced on the fly.
enum PseudoId {
  // The order must be NOP ID, public IDs, and then internal IDs.
  // If you add or remove a public ID, you must update _pseudoBits in
  // ComputedStyle.
  kPseudoIdNone,
  kPseudoIdFirstLine,
  kPseudoIdFirstLetter,
  kPseudoIdBefore,
  kPseudoIdAfter,
  kPseudoIdBackdrop,
  kPseudoIdSelection,
  kPseudoIdFirstLineInherited,
  kPseudoIdScrollbar,
  // Internal IDs follow:
  kPseudoIdScrollbarThumb,
  kPseudoIdScrollbarButton,
  kPseudoIdScrollbarTrack,
  kPseudoIdScrollbarTrackPiece,
  kPseudoIdScrollbarCorner,
  kPseudoIdResizer,
  kPseudoIdInputListButton,
  // Special values follow:
  kAfterLastInternalPseudoId,
  kFirstPublicPseudoId = kPseudoIdFirstLine,
  kFirstInternalPseudoId = kPseudoIdScrollbarThumb,
  kElementPseudoIdMask = (1 << (kPseudoIdBefore - kFirstPublicPseudoId)) |
                         (1 << (kPseudoIdAfter - kFirstPublicPseudoId)) |
                         (1 << (kPseudoIdBackdrop - kFirstPublicPseudoId))
};

enum OutlineIsAuto { kOutlineIsAutoOff = 0, kOutlineIsAutoOn };

// Random visual rendering model attributes. Not inherited.

enum class EVerticalAlign : unsigned {
  kBaseline,
  kMiddle,
  kSub,
  kSuper,
  kTextTop,
  kTextBottom,
  kTop,
  kBottom,
  kBaselineMiddle,
  kLength
};

enum EFillAttachment {
  kScrollBackgroundAttachment,
  kLocalBackgroundAttachment,
  kFixedBackgroundAttachment
};

enum EFillBox {
  kBorderFillBox,
  kPaddingFillBox,
  kContentFillBox,
  kTextFillBox
};

inline EFillBox EnclosingFillBox(EFillBox box_a, EFillBox box_b) {
  if (box_a == kBorderFillBox || box_b == kBorderFillBox)
    return kBorderFillBox;
  if (box_a == kPaddingFillBox || box_b == kPaddingFillBox)
    return kPaddingFillBox;
  if (box_a == kContentFillBox || box_b == kContentFillBox)
    return kContentFillBox;
  return kTextFillBox;
}

enum EFillRepeat { kRepeatFill, kNoRepeatFill, kRoundFill, kSpaceFill };

enum EFillLayerType { kBackgroundFillLayer, kMaskFillLayer };

// CSS3 Background Values
enum EFillSizeType { kContain, kCover, kSizeLength, kSizeNone };

// CSS3 Background Position
enum BackgroundEdgeOrigin { kTopEdge, kRightEdge, kBottomEdge, kLeftEdge };

// CSS Mask Source Types
enum EMaskSourceType { kMaskAlpha, kMaskLuminance };

// CSS3 Flexbox Properties

enum class EFlexWrap { kNowrap, kWrap, kWrapReverse };

// CSS3 Image Values
enum QuoteType { OPEN_QUOTE, CLOSE_QUOTE, NO_OPEN_QUOTE, NO_CLOSE_QUOTE };

enum EAnimPlayState { kAnimPlayStatePlaying, kAnimPlayStatePaused };

static const size_t kTextDecorationBits = 4;
enum class TextDecoration : unsigned {
  kNone = 0x0,
  kUnderline = 0x1,
  kOverline = 0x2,
  kLineThrough = 0x4,
  kBlink = 0x8
};
inline TextDecoration operator|(TextDecoration a, TextDecoration b) {
  return static_cast<TextDecoration>(static_cast<unsigned>(a) |
                                     static_cast<unsigned>(b));
}
inline TextDecoration& operator|=(TextDecoration& a, TextDecoration b) {
  return a = static_cast<TextDecoration>(static_cast<unsigned>(a) |
                                         static_cast<unsigned>(b));
}
inline TextDecoration& operator^=(TextDecoration& a, TextDecoration b) {
  return a = static_cast<TextDecoration>(static_cast<unsigned>(a) ^
                                         static_cast<unsigned>(b));
}

static const size_t kTextDecorationSkipBits = 3;
enum class TextDecorationSkip { kNone = 0x0, kObjects = 0x1, kInk = 0x2 };
inline TextDecorationSkip operator&(TextDecorationSkip a,
                                    TextDecorationSkip b) {
  return TextDecorationSkip(static_cast<unsigned>(a) &
                            static_cast<unsigned>(b));
}
inline TextDecorationSkip operator|(TextDecorationSkip a,
                                    TextDecorationSkip b) {
  return TextDecorationSkip(static_cast<unsigned>(a) |
                            static_cast<unsigned>(b));
}
inline TextDecorationSkip& operator|=(TextDecorationSkip& a,
                                      TextDecorationSkip b) {
  return a = a | b;
}

enum OffsetRotationType { kOffsetRotationAuto, kOffsetRotationFixed };

enum ELineClampType { kLineClampLineCount, kLineClampPercentage };

enum class TextEmphasisMark {
  kNone,
  kAuto,
  kDot,
  kCircle,
  kDoubleCircle,
  kTriangle,
  kSesame,
  kCustom
};

static const size_t kGridAutoFlowBits = 4;
enum InternalGridAutoFlowAlgorithm {
  kInternalAutoFlowAlgorithmSparse = 0x1,
  kInternalAutoFlowAlgorithmDense = 0x2
};

enum InternalGridAutoFlowDirection {
  kInternalAutoFlowDirectionRow = 0x4,
  kInternalAutoFlowDirectionColumn = 0x8
};

enum GridAutoFlow {
  kAutoFlowRow =
      kInternalAutoFlowAlgorithmSparse | kInternalAutoFlowDirectionRow,
  kAutoFlowColumn =
      kInternalAutoFlowAlgorithmSparse | kInternalAutoFlowDirectionColumn,
  kAutoFlowRowDense =
      kInternalAutoFlowAlgorithmDense | kInternalAutoFlowDirectionRow,
  kAutoFlowColumnDense =
      kInternalAutoFlowAlgorithmDense | kInternalAutoFlowDirectionColumn
};

static const size_t kContainmentBits = 4;
enum Containment {
  kContainsNone = 0x0,
  kContainsLayout = 0x1,
  kContainsStyle = 0x2,
  kContainsPaint = 0x4,
  kContainsSize = 0x8,
  kContainsStrict =
      kContainsLayout | kContainsStyle | kContainsPaint | kContainsSize,
  kContainsContent = kContainsLayout | kContainsStyle | kContainsPaint,
};
inline Containment operator|(Containment a, Containment b) {
  return Containment(int(a) | int(b));
}
inline Containment& operator|=(Containment& a, Containment b) {
  return a = a | b;
}

enum ItemPosition {
  kItemPositionAuto,
  kItemPositionNormal,
  kItemPositionStretch,
  kItemPositionBaseline,
  kItemPositionLastBaseline,
  kItemPositionCenter,
  kItemPositionStart,
  kItemPositionEnd,
  kItemPositionSelfStart,
  kItemPositionSelfEnd,
  kItemPositionFlexStart,
  kItemPositionFlexEnd,
  kItemPositionLeft,
  kItemPositionRight
};

enum OverflowAlignment {
  kOverflowAlignmentDefault,
  kOverflowAlignmentUnsafe,
  kOverflowAlignmentSafe
};

enum ItemPositionType { kNonLegacyPosition, kLegacyPosition };

enum ContentPosition {
  kContentPositionNormal,
  kContentPositionBaseline,
  kContentPositionLastBaseline,
  kContentPositionCenter,
  kContentPositionStart,
  kContentPositionEnd,
  kContentPositionFlexStart,
  kContentPositionFlexEnd,
  kContentPositionLeft,
  kContentPositionRight
};

enum ContentDistributionType {
  kContentDistributionDefault,
  kContentDistributionSpaceBetween,
  kContentDistributionSpaceAround,
  kContentDistributionSpaceEvenly,
  kContentDistributionStretch
};

// Reasonable maximum to prevent insane font sizes from causing crashes on some
// platforms (such as Windows).
static const float kMaximumAllowedFontSize = 10000.0f;

enum CSSBoxType {
  kBoxMissing = 0,
  kMarginBox,
  kBorderBox,
  kPaddingBox,
  kContentBox
};

enum SnapAxis {
  kSnapAxisBoth,
  kSnapAxisX,
  kSnapAxisY,
  kSnapAxisBlock,
  kSnapAxisInline,
};

enum SnapStrictness { kSnapStrictnessProximity, kSnapStrictnessMandatory };

enum SnapAlignment {
  kSnapAlignmentNone,
  kSnapAlignmentStart,
  kSnapAlignmentEnd,
  kSnapAlignmentCenter
};

}  // namespace blink

#endif  // ComputedStyleConstants_h
