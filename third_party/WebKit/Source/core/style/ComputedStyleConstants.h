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

// Some enums are automatically generated in ComputedStyleBaseConstants

// TODO(sashab): Change these enums to enum classes with an unsigned underlying
// type. Enum classes provide better type safety, and forcing an unsigned
// underlying type prevents msvc from interpreting enums as negative numbers.
// See: crbug.com/628043

// Sides used when drawing borders and outlines. The values should run clockwise
// from top.
enum BoxSide { BSTop, BSRight, BSBottom, BSLeft };

// See core/dom/stylerecalc.md for an explanation on what each state means
enum StyleRecalcChange {
  NoChange,
  NoInherit,
  UpdatePseudoElements,
  IndependentInherit,
  Inherit,
  Force,
  Reattach,
  ReattachNoLayoutObject
};

// Static pseudo styles. Dynamic ones are produced on the fly.
enum PseudoId {
  // The order must be NOP ID, public IDs, and then internal IDs.
  // If you add or remove a public ID, you must update _pseudoBits in
  // ComputedStyle.
  PseudoIdNone,
  PseudoIdFirstLine,
  PseudoIdFirstLetter,
  PseudoIdBefore,
  PseudoIdAfter,
  PseudoIdBackdrop,
  PseudoIdSelection,
  PseudoIdFirstLineInherited,
  PseudoIdScrollbar,
  // Internal IDs follow:
  PseudoIdScrollbarThumb,
  PseudoIdScrollbarButton,
  PseudoIdScrollbarTrack,
  PseudoIdScrollbarTrackPiece,
  PseudoIdScrollbarCorner,
  PseudoIdResizer,
  PseudoIdInputListButton,
  // Special values follow:
  AfterLastInternalPseudoId,
  FirstPublicPseudoId = PseudoIdFirstLine,
  FirstInternalPseudoId = PseudoIdScrollbarThumb,
  PublicPseudoIdMask =
      ((1 << FirstInternalPseudoId) - 1) & ~((1 << FirstPublicPseudoId) - 1),
  ElementPseudoIdMask = (1 << (PseudoIdBefore - 1)) |
                        (1 << (PseudoIdAfter - 1)) |
                        (1 << (PseudoIdBackdrop - 1))
};

enum ColumnFill { ColumnFillBalance, ColumnFillAuto };

enum ColumnSpan { ColumnSpanNone = 0, ColumnSpanAll };

// These have been defined in the order of their precedence for
// border-collapsing. Do not change this order! This order also must match the
// order in CSSValueKeywords.in.
enum EBorderStyle {
  BorderStyleNone,
  BorderStyleHidden,
  BorderStyleInset,
  BorderStyleGroove,
  BorderStyleOutset,
  BorderStyleRidge,
  BorderStyleDotted,
  BorderStyleDashed,
  BorderStyleSolid,
  BorderStyleDouble
};

enum EBorderPrecedence {
  BorderPrecedenceOff,
  BorderPrecedenceTable,
  BorderPrecedenceColumnGroup,
  BorderPrecedenceColumn,
  BorderPrecedenceRowGroup,
  BorderPrecedenceRow,
  BorderPrecedenceCell
};

enum OutlineIsAuto { OutlineIsAutoOff = 0, OutlineIsAutoOn };

enum EMarginCollapse {
  MarginCollapseCollapse,
  MarginCollapseSeparate,
  MarginCollapseDiscard
};

// Box decoration attributes. Not inherited.

enum EBoxDecorationBreak { BoxDecorationBreakSlice, BoxDecorationBreakClone };

// Box attributes. Not inherited.

enum class EBoxSizing : unsigned { kContentBox, kBorderBox };

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

enum TextCombine { TextCombineNone, TextCombineAll };

enum EFillAttachment {
  ScrollBackgroundAttachment,
  LocalBackgroundAttachment,
  FixedBackgroundAttachment
};

enum EFillBox { BorderFillBox, PaddingFillBox, ContentFillBox, TextFillBox };

inline EFillBox enclosingFillBox(EFillBox boxA, EFillBox boxB) {
  if (boxA == BorderFillBox || boxB == BorderFillBox)
    return BorderFillBox;
  if (boxA == PaddingFillBox || boxB == PaddingFillBox)
    return PaddingFillBox;
  if (boxA == ContentFillBox || boxB == ContentFillBox)
    return ContentFillBox;
  return TextFillBox;
}

enum EFillRepeat { RepeatFill, NoRepeatFill, RoundFill, SpaceFill };

enum EFillLayerType { BackgroundFillLayer, MaskFillLayer };

// CSS3 Background Values
enum EFillSizeType { Contain, Cover, SizeLength, SizeNone };

// CSS3 Background Position
enum BackgroundEdgeOrigin { TopEdge, RightEdge, BottomEdge, LeftEdge };

// CSS Mask Source Types
enum EMaskSourceType { MaskAlpha, MaskLuminance };

// Deprecated Flexible Box Properties

enum EBoxPack { BoxPackStart, BoxPackCenter, BoxPackEnd, BoxPackJustify };
enum EBoxAlignment { BSTRETCH, BSTART, BCENTER, BEND, BBASELINE };
enum EBoxOrient { HORIZONTAL, VERTICAL };
enum EBoxLines { SINGLE, MULTIPLE };

// CSS3 Flexbox Properties

enum EFlexDirection { FlowRow, FlowRowReverse, FlowColumn, FlowColumnReverse };
enum EFlexWrap { FlexNoWrap, FlexWrap, FlexWrapReverse };

enum ETextSecurity { TSNONE, TSDISC, TSCIRCLE, TSSQUARE };

// CSS3 User Modify Properties

enum EUserModify { READ_ONLY, READ_WRITE, READ_WRITE_PLAINTEXT_ONLY };

// CSS3 User Drag Values

enum EUserDrag { DRAG_AUTO, DRAG_NONE, DRAG_ELEMENT };

// CSS3 User Select Values

enum EUserSelect { SELECT_NONE, SELECT_TEXT, SELECT_ALL };

// CSS3 Image Values
enum ObjectFit {
  ObjectFitFill,
  ObjectFitContain,
  ObjectFitCover,
  ObjectFitNone,
  ObjectFitScaleDown
};

// Word Break Values. Matches WinIE and CSS3

enum EWordBreak {
  NormalWordBreak,
  BreakAllWordBreak,
  KeepAllWordBreak,
  BreakWordBreak
};

enum EOverflowWrap { NormalOverflowWrap, BreakOverflowWrap };

enum LineBreak {
  LineBreakAuto,
  LineBreakLoose,
  LineBreakNormal,
  LineBreakStrict,
  LineBreakAfterWhiteSpace
};

enum EResize { RESIZE_NONE, RESIZE_BOTH, RESIZE_HORIZONTAL, RESIZE_VERTICAL };

enum QuoteType { OPEN_QUOTE, CLOSE_QUOTE, NO_OPEN_QUOTE, NO_CLOSE_QUOTE };

enum EAnimPlayState { AnimPlayStatePlaying, AnimPlayStatePaused };

static const size_t TextDecorationBits = 4;
enum TextDecoration {
  TextDecorationNone = 0x0,
  TextDecorationUnderline = 0x1,
  TextDecorationOverline = 0x2,
  TextDecorationLineThrough = 0x4,
  TextDecorationBlink = 0x8
};
inline TextDecoration operator|(TextDecoration a, TextDecoration b) {
  return TextDecoration(int(a) | int(b));
}
inline TextDecoration& operator|=(TextDecoration& a, TextDecoration b) {
  return a = a | b;
}

enum TextDecorationStyle {
  TextDecorationStyleSolid,
  TextDecorationStyleDouble,
  TextDecorationStyleDotted,
  TextDecorationStyleDashed,
  TextDecorationStyleWavy
};

static const size_t TextDecorationSkipBits = 3;
enum TextDecorationSkip {
  TextDecorationSkipNone = 0x0,
  TextDecorationSkipObjects = 0x1,
  TextDecorationSkipInk = 0x2
};
inline TextDecorationSkip operator|(TextDecorationSkip a,
                                    TextDecorationSkip b) {
  return TextDecorationSkip(static_cast<unsigned>(a) |
                            static_cast<unsigned>(b));
}
inline TextDecorationSkip& operator|=(TextDecorationSkip& a,
                                      TextDecorationSkip b) {
  return a = a | b;
}

enum TextAlignLast {
  TextAlignLastAuto,
  TextAlignLastStart,
  TextAlignLastEnd,
  TextAlignLastLeft,
  TextAlignLastRight,
  TextAlignLastCenter,
  TextAlignLastJustify
};

enum TextUnderlinePosition {
  // FIXME: Implement support for 'under left' and 'under right' values.
  TextUnderlinePositionAuto,
  TextUnderlinePositionUnder
};

enum class ECursor : unsigned {
  kAuto,
  kCrosshair,
  kDefault,
  kPointer,
  kMove,
  kVerticalText,
  kCell,
  kContextMenu,
  kAlias,
  kProgress,
  kNoDrop,
  kNotAllowed,
  kZoomIn,
  kZoomOut,
  kEResize,
  kNeResize,
  kNwResize,
  kNResize,
  kSeResize,
  kSwResize,
  kSResize,
  kWResize,
  kEwResize,
  kNsResize,
  kNeswResize,
  kNwseResize,
  kColResize,
  kRowResize,
  kText,
  kWait,
  kHelp,
  kAllScroll,
  kWebkitGrab,
  kWebkitGrabbing,
  kCopy,
  kNone
};

enum class EDisplay : unsigned {
  Inline,
  Block,
  ListItem,
  InlineBlock,
  Table,
  InlineTable,
  TableRowGroup,
  TableHeaderGroup,
  TableFooterGroup,
  TableRow,
  TableColumnGroup,
  TableColumn,
  TableCell,
  TableCaption,
  WebkitBox,
  WebkitInlineBox,
  Flex,
  InlineFlex,
  Grid,
  InlineGrid,
  Contents,
  FlowRoot,
  None
};

enum class EInsideLink : unsigned {
  kNotInsideLink,
  kInsideUnvisitedLink,
  kInsideVisitedLink
};

enum ETransformStyle3D { TransformStyle3DFlat, TransformStyle3DPreserve3D };

enum OffsetRotationType { OffsetRotationAuto, OffsetRotationFixed };

enum EBackfaceVisibility {
  BackfaceVisibilityVisible,
  BackfaceVisibilityHidden
};

enum ELineClampType { LineClampLineCount, LineClampPercentage };

enum Hyphens { HyphensNone, HyphensManual, HyphensAuto };

enum ESpeak {
  SpeakNone,
  SpeakNormal,
  SpeakSpellOut,
  SpeakDigits,
  SpeakLiteralPunctuation,
  SpeakNoPunctuation
};

enum TextEmphasisFill { TextEmphasisFillFilled, TextEmphasisFillOpen };

enum TextEmphasisMark {
  TextEmphasisMarkNone,
  TextEmphasisMarkAuto,
  TextEmphasisMarkDot,
  TextEmphasisMarkCircle,
  TextEmphasisMarkDoubleCircle,
  TextEmphasisMarkTriangle,
  TextEmphasisMarkSesame,
  TextEmphasisMarkCustom
};

enum TextEmphasisPosition {
  TextEmphasisPositionOver,
  TextEmphasisPositionUnder
};

enum TextOrientation {
  TextOrientationMixed,
  TextOrientationUpright,
  TextOrientationSideways
};

enum TextOverflow { TextOverflowClip = 0, TextOverflowEllipsis };

enum EImageRendering {
  ImageRenderingAuto,
  ImageRenderingOptimizeSpeed,
  ImageRenderingOptimizeQuality,
  ImageRenderingOptimizeContrast,
  ImageRenderingPixelated
};

enum RubyPosition { RubyPositionBefore, RubyPositionAfter };

static const size_t GridAutoFlowBits = 4;
enum InternalGridAutoFlowAlgorithm {
  InternalAutoFlowAlgorithmSparse = 0x1,
  InternalAutoFlowAlgorithmDense = 0x2
};

enum InternalGridAutoFlowDirection {
  InternalAutoFlowDirectionRow = 0x4,
  InternalAutoFlowDirectionColumn = 0x8
};

enum GridAutoFlow {
  AutoFlowRow = InternalAutoFlowAlgorithmSparse | InternalAutoFlowDirectionRow,
  AutoFlowColumn =
      InternalAutoFlowAlgorithmSparse | InternalAutoFlowDirectionColumn,
  AutoFlowRowDense =
      InternalAutoFlowAlgorithmDense | InternalAutoFlowDirectionRow,
  AutoFlowColumnDense =
      InternalAutoFlowAlgorithmDense | InternalAutoFlowDirectionColumn
};

enum DraggableRegionMode {
  DraggableRegionNone,
  DraggableRegionDrag,
  DraggableRegionNoDrag
};

static const size_t TouchActionBits = 6;
enum TouchAction {
  TouchActionNone = 0x0,
  TouchActionPanLeft = 0x1,
  TouchActionPanRight = 0x2,
  TouchActionPanX = TouchActionPanLeft | TouchActionPanRight,
  TouchActionPanUp = 0x4,
  TouchActionPanDown = 0x8,
  TouchActionPanY = TouchActionPanUp | TouchActionPanDown,
  TouchActionPan = TouchActionPanX | TouchActionPanY,
  TouchActionPinchZoom = 0x10,
  TouchActionManipulation = TouchActionPan | TouchActionPinchZoom,
  TouchActionDoubleTapZoom = 0x20,
  TouchActionAuto = TouchActionManipulation | TouchActionDoubleTapZoom
};
inline TouchAction operator|(TouchAction a, TouchAction b) {
  return static_cast<TouchAction>(int(a) | int(b));
}
inline TouchAction& operator|=(TouchAction& a, TouchAction b) {
  return a = a | b;
}
inline TouchAction operator&(TouchAction a, TouchAction b) {
  return static_cast<TouchAction>(int(a) & int(b));
}
inline TouchAction& operator&=(TouchAction& a, TouchAction b) {
  return a = a & b;
}

enum EIsolation { IsolationAuto, IsolationIsolate };

static const size_t ContainmentBits = 4;
enum Containment {
  ContainsNone = 0x0,
  ContainsLayout = 0x1,
  ContainsStyle = 0x2,
  ContainsPaint = 0x4,
  ContainsSize = 0x8,
  ContainsStrict =
      ContainsLayout | ContainsStyle | ContainsPaint | ContainsSize,
  ContainsContent = ContainsLayout | ContainsStyle | ContainsPaint,
};
inline Containment operator|(Containment a, Containment b) {
  return Containment(int(a) | int(b));
}
inline Containment& operator|=(Containment& a, Containment b) {
  return a = a | b;
}

enum ItemPosition {
  ItemPositionAuto,  // It will mean 'normal' after running the StyleAdjuster to
                     // avoid resolving the initial values.
  ItemPositionNormal,
  ItemPositionStretch,
  ItemPositionBaseline,
  ItemPositionLastBaseline,
  ItemPositionCenter,
  ItemPositionStart,
  ItemPositionEnd,
  ItemPositionSelfStart,
  ItemPositionSelfEnd,
  ItemPositionFlexStart,
  ItemPositionFlexEnd,
  ItemPositionLeft,
  ItemPositionRight
};

enum OverflowAlignment {
  OverflowAlignmentDefault,
  OverflowAlignmentUnsafe,
  OverflowAlignmentSafe
};

enum ItemPositionType { NonLegacyPosition, LegacyPosition };

enum ContentPosition {
  ContentPositionNormal,
  ContentPositionBaseline,
  ContentPositionLastBaseline,
  ContentPositionCenter,
  ContentPositionStart,
  ContentPositionEnd,
  ContentPositionFlexStart,
  ContentPositionFlexEnd,
  ContentPositionLeft,
  ContentPositionRight
};

enum ContentDistributionType {
  ContentDistributionDefault,
  ContentDistributionSpaceBetween,
  ContentDistributionSpaceAround,
  ContentDistributionSpaceEvenly,
  ContentDistributionStretch
};

// Reasonable maximum to prevent insane font sizes from causing crashes on some
// platforms (such as Windows).
static const float maximumAllowedFontSize = 10000.0f;

enum TextIndentLine { TextIndentFirstLine, TextIndentEachLine };
enum TextIndentType { TextIndentNormal, TextIndentHanging };

enum CSSBoxType {
  BoxMissing = 0,
  MarginBox,
  BorderBox,
  PaddingBox,
  ContentBox
};

enum ScrollSnapType {
  ScrollSnapTypeNone,
  ScrollSnapTypeMandatory,
  ScrollSnapTypeProximity
};

enum AutoRepeatType { NoAutoRepeat, AutoFill, AutoFit };

}  // namespace blink

#endif  // ComputedStyleConstants_h
