// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/parser/CSSParserFastPaths.h"

#include "core/StylePropertyShorthand.h"
#include "core/css/CSSTransformValue.h"
#include "core/css/CSSValuePool.h"
#include "core/css/parser/CSSParserIdioms.h"
#include "core/css/parser/CSSParserValues.h"
#include "core/css/parser/CSSPropertyParser.h"

namespace blink {

static inline bool isSimpleLengthPropertyID(CSSPropertyID propertyId, bool& acceptsNegativeNumbers)
{
    switch (propertyId) {
    case CSSPropertyFontSize:
    case CSSPropertyHeight:
    case CSSPropertyWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyMinWidth:
    case CSSPropertyPaddingBottom:
    case CSSPropertyPaddingLeft:
    case CSSPropertyPaddingRight:
    case CSSPropertyPaddingTop:
    case CSSPropertyWebkitLogicalWidth:
    case CSSPropertyWebkitLogicalHeight:
    case CSSPropertyWebkitMinLogicalWidth:
    case CSSPropertyWebkitMinLogicalHeight:
    case CSSPropertyWebkitPaddingAfter:
    case CSSPropertyWebkitPaddingBefore:
    case CSSPropertyWebkitPaddingEnd:
    case CSSPropertyWebkitPaddingStart:
    case CSSPropertyShapeMargin:
        acceptsNegativeNumbers = false;
        return true;
    case CSSPropertyBottom:
    case CSSPropertyLeft:
    case CSSPropertyMarginBottom:
    case CSSPropertyMarginLeft:
    case CSSPropertyMarginRight:
    case CSSPropertyMarginTop:
    case CSSPropertyRight:
    case CSSPropertyTop:
    case CSSPropertyWebkitMarginAfter:
    case CSSPropertyWebkitMarginBefore:
    case CSSPropertyWebkitMarginEnd:
    case CSSPropertyWebkitMarginStart:
        acceptsNegativeNumbers = true;
        return true;
    default:
        return false;
    }
}

template <typename CharacterType>
static inline bool parseSimpleLength(const CharacterType* characters, unsigned length, CSSPrimitiveValue::UnitType& unit, double& number)
{
    if (length > 2 && (characters[length - 2] | 0x20) == 'p' && (characters[length - 1] | 0x20) == 'x') {
        length -= 2;
        unit = CSSPrimitiveValue::CSS_PX;
    } else if (length > 1 && characters[length - 1] == '%') {
        length -= 1;
        unit = CSSPrimitiveValue::CSS_PERCENTAGE;
    }

    // We rely on charactersToDouble for validation as well. The function
    // will set "ok" to "false" if the entire passed-in character range does
    // not represent a double.
    bool ok;
    number = charactersToDouble(characters, length, &ok);
    return ok;
}

static PassRefPtrWillBeRawPtr<CSSValue> parseSimpleLengthValue(CSSPropertyID propertyId, const String& string, CSSParserMode cssParserMode)
{
    ASSERT(!string.isEmpty());
    bool acceptsNegativeNumbers = false;

    // In @viewport, width and height are shorthands, not simple length values.
    if (isCSSViewportParsingEnabledForMode(cssParserMode) || !isSimpleLengthPropertyID(propertyId, acceptsNegativeNumbers))
        return nullptr;

    unsigned length = string.length();
    double number;
    CSSPrimitiveValue::UnitType unit = CSSPrimitiveValue::CSS_NUMBER;

    if (string.is8Bit()) {
        if (!parseSimpleLength(string.characters8(), length, unit, number))
            return nullptr;
    } else {
        if (!parseSimpleLength(string.characters16(), length, unit, number))
            return nullptr;
    }

    if (unit == CSSPrimitiveValue::CSS_NUMBER) {
        bool quirksMode = isQuirksModeBehavior(cssParserMode);
        if (number && !quirksMode)
            return nullptr;
        unit = CSSPrimitiveValue::CSS_PX;
    }
    if (number < 0 && !acceptsNegativeNumbers)
        return nullptr;

    return cssValuePool().createValue(number, unit);
}

static inline bool isColorPropertyID(CSSPropertyID propertyId)
{
    switch (propertyId) {
    case CSSPropertyColor:
    case CSSPropertyBackgroundColor:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderTopColor:
    case CSSPropertyOutlineColor:
    case CSSPropertyWebkitBorderAfterColor:
    case CSSPropertyWebkitBorderBeforeColor:
    case CSSPropertyWebkitBorderEndColor:
    case CSSPropertyWebkitBorderStartColor:
    case CSSPropertyWebkitColumnRuleColor:
    case CSSPropertyWebkitTextEmphasisColor:
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTextStrokeColor:
    case CSSPropertyTextDecorationColor:
        return true;
    default:
        return false;
    }
}

static PassRefPtrWillBeRawPtr<CSSValue> parseColorValue(CSSPropertyID propertyId, const String& string, CSSParserMode cssParserMode)
{
    ASSERT(!string.isEmpty());
    bool quirksMode = isQuirksModeBehavior(cssParserMode);
    if (!isColorPropertyID(propertyId))
        return nullptr;
    CSSParserString cssString;
    cssString.init(string);
    CSSValueID valueID = cssValueKeywordID(cssString);
    bool validPrimitive = false;
    if (valueID == CSSValueWebkitText) {
        validPrimitive = true;
    } else if (valueID == CSSValueCurrentcolor) {
        validPrimitive = true;
    } else if ((valueID >= CSSValueAqua && valueID <= CSSValueWindowtext) || valueID == CSSValueMenu
        || (quirksMode && valueID >= CSSValueWebkitFocusRingColor && valueID < CSSValueWebkitText)) {
        validPrimitive = true;
    }

    if (validPrimitive)
        return cssValuePool().createIdentifierValue(valueID);

    RGBA32 color;
    if (!CSSPropertyParser::fastParseColor(color, string, !quirksMode && string[0] != '#'))
        return nullptr;
    return cssValuePool().createColorValue(color);
}

bool CSSParserFastPaths::isValidKeywordPropertyAndValue(CSSPropertyID propertyId, CSSValueID valueID)
{
    if (valueID == CSSValueInvalid)
        return false;

    switch (propertyId) {
    case CSSPropertyAll:
        return valueID == CSSValueUnset;
    case CSSPropertyBackgroundRepeatX: // repeat | no-repeat
    case CSSPropertyBackgroundRepeatY: // repeat | no-repeat
        return valueID == CSSValueRepeat || valueID == CSSValueNoRepeat;
    case CSSPropertyBorderCollapse: // collapse | separate
        return valueID == CSSValueCollapse || valueID == CSSValueSeparate;
    case CSSPropertyBorderTopStyle: // <border-style>
    case CSSPropertyBorderRightStyle: // Defined as: none | hidden | dotted | dashed |
    case CSSPropertyBorderBottomStyle: // solid | double | groove | ridge | inset | outset
    case CSSPropertyBorderLeftStyle:
    case CSSPropertyWebkitBorderAfterStyle:
    case CSSPropertyWebkitBorderBeforeStyle:
    case CSSPropertyWebkitBorderEndStyle:
    case CSSPropertyWebkitBorderStartStyle:
    case CSSPropertyWebkitColumnRuleStyle:
        return valueID >= CSSValueNone && valueID <= CSSValueDouble;
    case CSSPropertyBoxSizing:
        return valueID == CSSValueBorderBox || valueID == CSSValueContentBox;
    case CSSPropertyCaptionSide: // top | bottom | left | right
        return valueID == CSSValueLeft || valueID == CSSValueRight || valueID == CSSValueTop || valueID == CSSValueBottom;
    case CSSPropertyClear: // none | left | right | both
        return valueID == CSSValueNone || valueID == CSSValueLeft || valueID == CSSValueRight || valueID == CSSValueBoth;
    case CSSPropertyDirection: // ltr | rtl
        return valueID == CSSValueLtr || valueID == CSSValueRtl;
    case CSSPropertyDisplay:
        // inline | block | list-item | inline-block | table |
        // inline-table | table-row-group | table-header-group | table-footer-group | table-row |
        // table-column-group | table-column | table-cell | table-caption | -webkit-box | -webkit-inline-box | none
        // flex | inline-flex | -webkit-flex | -webkit-inline-flex | grid | inline-grid
        return (valueID >= CSSValueInline && valueID <= CSSValueInlineFlex) || valueID == CSSValueWebkitFlex || valueID == CSSValueWebkitInlineFlex || valueID == CSSValueNone
            || (RuntimeEnabledFeatures::cssGridLayoutEnabled() && (valueID == CSSValueGrid || valueID == CSSValueInlineGrid));
    case CSSPropertyEmptyCells: // show | hide
        return valueID == CSSValueShow || valueID == CSSValueHide;
    case CSSPropertyFloat: // left | right | none | center (for buggy CSS, maps to none)
        return valueID == CSSValueLeft || valueID == CSSValueRight || valueID == CSSValueNone || valueID == CSSValueCenter;
    case CSSPropertyFontStyle: // normal | italic | oblique
        return valueID == CSSValueNormal || valueID == CSSValueItalic || valueID == CSSValueOblique;
    case CSSPropertyFontStretch: // normal | ultra-condensed | extra-condensed | condensed | semi-condensed | semi-expanded | expanded | extra-expanded | ultra-expanded
        return valueID == CSSValueNormal || (valueID >= CSSValueUltraCondensed && valueID <= CSSValueUltraExpanded);
    case CSSPropertyImageRendering: // auto | optimizeContrast | pixelated
        return valueID == CSSValueAuto || valueID == CSSValueWebkitOptimizeContrast || (RuntimeEnabledFeatures::imageRenderingPixelatedEnabled() && valueID == CSSValuePixelated);
    case CSSPropertyIsolation: // auto | isolate
        ASSERT(RuntimeEnabledFeatures::cssCompositingEnabled());
        return valueID == CSSValueAuto || valueID == CSSValueIsolate;
    case CSSPropertyListStylePosition: // inside | outside
        return valueID == CSSValueInside || valueID == CSSValueOutside;
    case CSSPropertyListStyleType:
        // See section CSS_PROP_LIST_STYLE_TYPE of file CSSValueKeywords.in
        // for the list of supported list-style-types.
        return (valueID >= CSSValueDisc && valueID <= CSSValueKatakanaIroha) || valueID == CSSValueNone;
    case CSSPropertyObjectFit:
        return valueID == CSSValueFill || valueID == CSSValueContain || valueID == CSSValueCover || valueID == CSSValueNone || valueID == CSSValueScaleDown;
    case CSSPropertyOutlineStyle: // (<border-style> except hidden) | auto
        return valueID == CSSValueAuto || valueID == CSSValueNone || (valueID >= CSSValueInset && valueID <= CSSValueDouble);
    case CSSPropertyOverflowWrap: // normal | break-word
    case CSSPropertyWordWrap:
        return valueID == CSSValueNormal || valueID == CSSValueBreakWord;
    case CSSPropertyOverflowX: // visible | hidden | scroll | auto | overlay
        return valueID == CSSValueVisible || valueID == CSSValueHidden || valueID == CSSValueScroll || valueID == CSSValueAuto || valueID == CSSValueOverlay;
    case CSSPropertyOverflowY: // visible | hidden | scroll | auto | overlay | -webkit-paged-x | -webkit-paged-y
        return valueID == CSSValueVisible || valueID == CSSValueHidden || valueID == CSSValueScroll || valueID == CSSValueAuto || valueID == CSSValueOverlay || valueID == CSSValueWebkitPagedX || valueID == CSSValueWebkitPagedY;
    case CSSPropertyPageBreakAfter: // auto | always | avoid | left | right
    case CSSPropertyPageBreakBefore:
    case CSSPropertyWebkitColumnBreakAfter:
    case CSSPropertyWebkitColumnBreakBefore:
        return valueID == CSSValueAuto || valueID == CSSValueAlways || valueID == CSSValueAvoid || valueID == CSSValueLeft || valueID == CSSValueRight;
    case CSSPropertyPageBreakInside: // avoid | auto
    case CSSPropertyWebkitColumnBreakInside:
        return valueID == CSSValueAuto || valueID == CSSValueAvoid;
    case CSSPropertyPointerEvents:
        // none | visiblePainted | visibleFill | visibleStroke | visible |
        // painted | fill | stroke | auto | all | bounding-box
        return valueID == CSSValueVisible || valueID == CSSValueNone || valueID == CSSValueAll || valueID == CSSValueAuto || (valueID >= CSSValueVisiblepainted && valueID <= CSSValueBoundingBox);
    case CSSPropertyPosition: // static | relative | absolute | fixed
        return valueID == CSSValueStatic || valueID == CSSValueRelative || valueID == CSSValueAbsolute || valueID == CSSValueFixed;
    case CSSPropertyResize: // none | both | horizontal | vertical | auto
        return valueID == CSSValueNone || valueID == CSSValueBoth || valueID == CSSValueHorizontal || valueID == CSSValueVertical || valueID == CSSValueAuto;
    case CSSPropertyScrollBehavior: // instant | smooth
        ASSERT(RuntimeEnabledFeatures::cssomSmoothScrollEnabled());
        return valueID == CSSValueInstant || valueID == CSSValueSmooth;
    case CSSPropertySpeak: // none | normal | spell-out | digits | literal-punctuation | no-punctuation
        return valueID == CSSValueNone || valueID == CSSValueNormal || valueID == CSSValueSpellOut || valueID == CSSValueDigits || valueID == CSSValueLiteralPunctuation || valueID == CSSValueNoPunctuation;
    case CSSPropertyTableLayout: // auto | fixed
        return valueID == CSSValueAuto || valueID == CSSValueFixed;
    case CSSPropertyTextAlignLast:
        // auto | start | end | left | right | center | justify
        ASSERT(RuntimeEnabledFeatures::css3TextEnabled());
        return (valueID >= CSSValueLeft && valueID <= CSSValueJustify) || valueID == CSSValueStart || valueID == CSSValueEnd || valueID == CSSValueAuto;
    case CSSPropertyTextDecorationStyle:
        // solid | double | dotted | dashed | wavy
        ASSERT(RuntimeEnabledFeatures::css3TextDecorationsEnabled());
        return valueID == CSSValueSolid || valueID == CSSValueDouble || valueID == CSSValueDotted || valueID == CSSValueDashed || valueID == CSSValueWavy;
    case CSSPropertyTextJustify:
        // auto | none | inter-word | distribute
        ASSERT(RuntimeEnabledFeatures::css3TextEnabled());
        return valueID == CSSValueInterWord || valueID == CSSValueDistribute || valueID == CSSValueAuto || valueID == CSSValueNone;
    case CSSPropertyTextOverflow: // clip | ellipsis
        return valueID == CSSValueClip || valueID == CSSValueEllipsis;
    case CSSPropertyTextRendering: // auto | optimizeSpeed | optimizeLegibility | geometricPrecision
        return valueID == CSSValueAuto || valueID == CSSValueOptimizespeed || valueID == CSSValueOptimizelegibility || valueID == CSSValueGeometricprecision;
    case CSSPropertyTextTransform: // capitalize | uppercase | lowercase | none
        return (valueID >= CSSValueCapitalize && valueID <= CSSValueLowercase) || valueID == CSSValueNone;
    case CSSPropertyUnicodeBidi:
        return valueID == CSSValueNormal || valueID == CSSValueEmbed
            || valueID == CSSValueBidiOverride || valueID == CSSValueWebkitIsolate
            || valueID == CSSValueWebkitIsolateOverride || valueID == CSSValueWebkitPlaintext;
    case CSSPropertyTouchActionDelay: // none | script
        ASSERT(RuntimeEnabledFeatures::cssTouchActionDelayEnabled());
        return valueID == CSSValueScript || valueID == CSSValueNone;
    case CSSPropertyVisibility: // visible | hidden | collapse
        return valueID == CSSValueVisible || valueID == CSSValueHidden || valueID == CSSValueCollapse;
    case CSSPropertyWebkitAppearance:
        return (valueID >= CSSValueCheckbox && valueID <= CSSValueTextarea) || valueID == CSSValueNone;
    case CSSPropertyBackfaceVisibility:
    case CSSPropertyWebkitBackfaceVisibility:
        return valueID == CSSValueVisible || valueID == CSSValueHidden;
    case CSSPropertyMixBlendMode:
        ASSERT(RuntimeEnabledFeatures::cssCompositingEnabled());
        return valueID == CSSValueNormal || valueID == CSSValueMultiply || valueID == CSSValueScreen || valueID == CSSValueOverlay
            || valueID == CSSValueDarken || valueID == CSSValueLighten || valueID == CSSValueColorDodge || valueID == CSSValueColorBurn
            || valueID == CSSValueHardLight || valueID == CSSValueSoftLight || valueID == CSSValueDifference || valueID == CSSValueExclusion
            || valueID == CSSValueHue || valueID == CSSValueSaturation || valueID == CSSValueColor || valueID == CSSValueLuminosity;
    case CSSPropertyWebkitBoxAlign:
        return valueID == CSSValueStretch || valueID == CSSValueStart || valueID == CSSValueEnd || valueID == CSSValueCenter || valueID == CSSValueBaseline;
    case CSSPropertyWebkitBoxDecorationBreak:
        return valueID == CSSValueClone || valueID == CSSValueSlice;
    case CSSPropertyWebkitBoxDirection:
        return valueID == CSSValueNormal || valueID == CSSValueReverse;
    case CSSPropertyWebkitBoxLines:
        return valueID == CSSValueSingle || valueID == CSSValueMultiple;
    case CSSPropertyWebkitBoxOrient:
        return valueID == CSSValueHorizontal || valueID == CSSValueVertical || valueID == CSSValueInlineAxis || valueID == CSSValueBlockAxis;
    case CSSPropertyWebkitBoxPack:
        return valueID == CSSValueStart || valueID == CSSValueEnd || valueID == CSSValueCenter || valueID == CSSValueJustify;
    case CSSPropertyColumnFill:
        ASSERT(RuntimeEnabledFeatures::regionBasedColumnsEnabled());
        return valueID == CSSValueAuto || valueID == CSSValueBalance;
    case CSSPropertyAlignContent:
        // FIXME: Per CSS alignment, this property should accept an optional <overflow-position>. We should share this parsing code with 'justify-self'.
        return valueID == CSSValueFlexStart || valueID == CSSValueFlexEnd || valueID == CSSValueCenter || valueID == CSSValueSpaceBetween || valueID == CSSValueSpaceAround || valueID == CSSValueStretch;
    case CSSPropertyAlignItems:
        // FIXME: Per CSS alignment, this property should accept the same arguments as 'justify-self' so we should share its parsing code.
        return valueID == CSSValueFlexStart || valueID == CSSValueFlexEnd || valueID == CSSValueCenter || valueID == CSSValueBaseline || valueID == CSSValueStretch;
    case CSSPropertyAlignSelf:
        // FIXME: Per CSS alignment, this property should accept the same arguments as 'justify-self' so we should share its parsing code.
        return valueID == CSSValueAuto || valueID == CSSValueFlexStart || valueID == CSSValueFlexEnd || valueID == CSSValueCenter || valueID == CSSValueBaseline || valueID == CSSValueStretch;
    case CSSPropertyFlexDirection:
        return valueID == CSSValueRow || valueID == CSSValueRowReverse || valueID == CSSValueColumn || valueID == CSSValueColumnReverse;
    case CSSPropertyFlexWrap:
        return valueID == CSSValueNowrap || valueID == CSSValueWrap || valueID == CSSValueWrapReverse;
    case CSSPropertyJustifyContent:
        // FIXME: Per CSS alignment, this property should accept an optional <overflow-position>. We should share this parsing code with 'justify-self'.
        return valueID == CSSValueFlexStart || valueID == CSSValueFlexEnd || valueID == CSSValueCenter || valueID == CSSValueSpaceBetween || valueID == CSSValueSpaceAround;
    case CSSPropertyFontKerning:
        return valueID == CSSValueAuto || valueID == CSSValueNormal || valueID == CSSValueNone;
    case CSSPropertyWebkitFontSmoothing:
        return valueID == CSSValueAuto || valueID == CSSValueNone || valueID == CSSValueAntialiased || valueID == CSSValueSubpixelAntialiased;
    case CSSPropertyWebkitLineBreak: // auto | loose | normal | strict | after-white-space
        return valueID == CSSValueAuto || valueID == CSSValueLoose || valueID == CSSValueNormal || valueID == CSSValueStrict || valueID == CSSValueAfterWhiteSpace;
    case CSSPropertyWebkitMarginAfterCollapse:
    case CSSPropertyWebkitMarginBeforeCollapse:
    case CSSPropertyWebkitMarginBottomCollapse:
    case CSSPropertyWebkitMarginTopCollapse:
        return valueID == CSSValueCollapse || valueID == CSSValueSeparate || valueID == CSSValueDiscard;
    case CSSPropertyInternalMarqueeDirection:
        return valueID == CSSValueForwards || valueID == CSSValueBackwards || valueID == CSSValueAhead || valueID == CSSValueReverse || valueID == CSSValueLeft || valueID == CSSValueRight || valueID == CSSValueDown
            || valueID == CSSValueUp || valueID == CSSValueAuto;
    case CSSPropertyInternalMarqueeStyle:
        return valueID == CSSValueNone || valueID == CSSValueSlide || valueID == CSSValueScroll || valueID == CSSValueAlternate;
    case CSSPropertyWebkitPrintColorAdjust:
        return valueID == CSSValueExact || valueID == CSSValueEconomy;
    case CSSPropertyWebkitRtlOrdering:
        return valueID == CSSValueLogical || valueID == CSSValueVisual;
    case CSSPropertyWebkitRubyPosition:
        return valueID == CSSValueBefore || valueID == CSSValueAfter;
    case CSSPropertyWebkitTextCombine:
        return valueID == CSSValueNone || valueID == CSSValueHorizontal;
    case CSSPropertyWebkitTextEmphasisPosition:
        return valueID == CSSValueOver || valueID == CSSValueUnder;
    case CSSPropertyWebkitTextSecurity: // disc | circle | square | none
        return valueID == CSSValueDisc || valueID == CSSValueCircle || valueID == CSSValueSquare || valueID == CSSValueNone;
    case CSSPropertyTransformStyle:
    case CSSPropertyWebkitTransformStyle:
        return valueID == CSSValueFlat || valueID == CSSValuePreserve3d;
    case CSSPropertyWebkitUserDrag: // auto | none | element
        return valueID == CSSValueAuto || valueID == CSSValueNone || valueID == CSSValueElement;
    case CSSPropertyWebkitUserModify: // read-only | read-write
        return valueID == CSSValueReadOnly || valueID == CSSValueReadWrite || valueID == CSSValueReadWritePlaintextOnly;
    case CSSPropertyWebkitUserSelect: // auto | none | text | all
        return valueID == CSSValueAuto || valueID == CSSValueNone || valueID == CSSValueText || valueID == CSSValueAll;
    case CSSPropertyWebkitWritingMode:
        return valueID >= CSSValueHorizontalTb && valueID <= CSSValueHorizontalBt;
    case CSSPropertyWhiteSpace: // normal | pre | nowrap
        return valueID == CSSValueNormal || valueID == CSSValuePre || valueID == CSSValuePreWrap || valueID == CSSValuePreLine || valueID == CSSValueNowrap;
    case CSSPropertyWordBreak: // normal | break-all | break-word (this is a custom extension)
        return valueID == CSSValueNormal || valueID == CSSValueBreakAll || valueID == CSSValueBreakWord;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
}

bool CSSParserFastPaths::isKeywordPropertyID(CSSPropertyID propertyId)
{
    switch (propertyId) {
    case CSSPropertyAll:
    case CSSPropertyMixBlendMode:
    case CSSPropertyIsolation:
    case CSSPropertyBackgroundRepeatX:
    case CSSPropertyBackgroundRepeatY:
    case CSSPropertyBorderBottomStyle:
    case CSSPropertyBorderCollapse:
    case CSSPropertyBorderLeftStyle:
    case CSSPropertyBorderRightStyle:
    case CSSPropertyBorderTopStyle:
    case CSSPropertyBoxSizing:
    case CSSPropertyCaptionSide:
    case CSSPropertyClear:
    case CSSPropertyDirection:
    case CSSPropertyDisplay:
    case CSSPropertyEmptyCells:
    case CSSPropertyFloat:
    case CSSPropertyFontStyle:
    case CSSPropertyFontStretch:
    case CSSPropertyImageRendering:
    case CSSPropertyListStylePosition:
    case CSSPropertyListStyleType:
    case CSSPropertyObjectFit:
    case CSSPropertyOutlineStyle:
    case CSSPropertyOverflowWrap:
    case CSSPropertyOverflowX:
    case CSSPropertyOverflowY:
    case CSSPropertyPageBreakAfter:
    case CSSPropertyPageBreakBefore:
    case CSSPropertyPageBreakInside:
    case CSSPropertyPointerEvents:
    case CSSPropertyPosition:
    case CSSPropertyResize:
    case CSSPropertyScrollBehavior:
    case CSSPropertySpeak:
    case CSSPropertyTableLayout:
    case CSSPropertyTextAlignLast:
    case CSSPropertyTextDecorationStyle:
    case CSSPropertyTextJustify:
    case CSSPropertyTextOverflow:
    case CSSPropertyTextRendering:
    case CSSPropertyTextTransform:
    case CSSPropertyTouchActionDelay:
    case CSSPropertyUnicodeBidi:
    case CSSPropertyVisibility:
    case CSSPropertyWebkitAppearance:
    case CSSPropertyBackfaceVisibility:
    case CSSPropertyWebkitBackfaceVisibility:
    case CSSPropertyWebkitBorderAfterStyle:
    case CSSPropertyWebkitBorderBeforeStyle:
    case CSSPropertyWebkitBorderEndStyle:
    case CSSPropertyWebkitBorderStartStyle:
    case CSSPropertyWebkitBoxAlign:
    case CSSPropertyWebkitBoxDecorationBreak:
    case CSSPropertyWebkitBoxDirection:
    case CSSPropertyWebkitBoxLines:
    case CSSPropertyWebkitBoxOrient:
    case CSSPropertyWebkitBoxPack:
    case CSSPropertyWebkitColumnBreakAfter:
    case CSSPropertyWebkitColumnBreakBefore:
    case CSSPropertyWebkitColumnBreakInside:
    case CSSPropertyColumnFill:
    case CSSPropertyWebkitColumnRuleStyle:
    case CSSPropertyAlignContent:
    case CSSPropertyFlexDirection:
    case CSSPropertyFlexWrap:
    case CSSPropertyJustifyContent:
    case CSSPropertyFontKerning:
    case CSSPropertyWebkitFontSmoothing:
    case CSSPropertyWebkitLineBreak:
    case CSSPropertyWebkitMarginAfterCollapse:
    case CSSPropertyWebkitMarginBeforeCollapse:
    case CSSPropertyWebkitMarginBottomCollapse:
    case CSSPropertyWebkitMarginTopCollapse:
    case CSSPropertyInternalMarqueeDirection:
    case CSSPropertyInternalMarqueeStyle:
    case CSSPropertyWebkitPrintColorAdjust:
    case CSSPropertyWebkitRtlOrdering:
    case CSSPropertyWebkitRubyPosition:
    case CSSPropertyWebkitTextCombine:
    case CSSPropertyWebkitTextEmphasisPosition:
    case CSSPropertyWebkitTextSecurity:
    case CSSPropertyTransformStyle:
    case CSSPropertyWebkitTransformStyle:
    case CSSPropertyWebkitUserDrag:
    case CSSPropertyWebkitUserModify:
    case CSSPropertyWebkitUserSelect:
    case CSSPropertyWebkitWritingMode:
    case CSSPropertyWhiteSpace:
    case CSSPropertyWordBreak:
    case CSSPropertyWordWrap:
        return true;
    case CSSPropertyAlignItems:
    case CSSPropertyAlignSelf:
        return !RuntimeEnabledFeatures::cssGridLayoutEnabled();
    default:
        return false;
    }
}

static PassRefPtrWillBeRawPtr<CSSValue> parseKeywordValue(CSSPropertyID propertyId, const String& string)
{
    ASSERT(!string.isEmpty());

    if (!CSSParserFastPaths::isKeywordPropertyID(propertyId)) {
        // All properties accept the values of "initial" and "inherit".
        String lowerCaseString = string.lower();
        if (lowerCaseString != "initial" && lowerCaseString != "inherit")
            return nullptr;

        // Parse initial/inherit shorthands using the BisonCSSParser.
        if (shorthandForProperty(propertyId).length())
            return nullptr;
    }

    CSSParserString cssString;
    cssString.init(string);
    CSSValueID valueID = cssValueKeywordID(cssString);

    if (!valueID)
        return nullptr;

    if (valueID == CSSValueInherit)
        return cssValuePool().createInheritedValue();
    if (valueID == CSSValueInitial)
        return cssValuePool().createExplicitInitialValue();
    if (CSSParserFastPaths::isValidKeywordPropertyAndValue(propertyId, valueID))
        return cssValuePool().createIdentifierValue(valueID);
    return nullptr;
}

template <typename CharType>
static bool parseTransformTranslateArguments(CharType*& pos, CharType* end, unsigned expectedCount, CSSTransformValue* transformValue)
{
    while (expectedCount) {
        size_t delimiter = WTF::find(pos, end - pos, expectedCount == 1 ? ')' : ',');
        if (delimiter == kNotFound)
            return false;
        unsigned argumentLength = static_cast<unsigned>(delimiter);
        CSSPrimitiveValue::UnitType unit = CSSPrimitiveValue::CSS_NUMBER;
        double number;
        if (!parseSimpleLength(pos, argumentLength, unit, number))
            return false;
        if (unit != CSSPrimitiveValue::CSS_PX && (number || unit != CSSPrimitiveValue::CSS_NUMBER))
            return false;
        transformValue->append(cssValuePool().createValue(number, CSSPrimitiveValue::CSS_PX));
        pos += argumentLength + 1;
        --expectedCount;
    }
    return true;
}

template <typename CharType>
static bool parseTransformNumberArguments(CharType*& pos, CharType* end, unsigned expectedCount, CSSTransformValue* transformValue)
{
    while (expectedCount) {
        size_t delimiter = WTF::find(pos, end - pos, expectedCount == 1 ? ')' : ',');
        if (delimiter == kNotFound)
            return false;
        unsigned argumentLength = static_cast<unsigned>(delimiter);
        bool ok;
        double number = charactersToDouble(pos, argumentLength, &ok);
        if (!ok)
            return false;
        transformValue->append(cssValuePool().createValue(number, CSSPrimitiveValue::CSS_NUMBER));
        pos += argumentLength + 1;
        --expectedCount;
    }
    return true;
}

template <typename CharType>
static PassRefPtrWillBeRawPtr<CSSTransformValue> parseSimpleTransformValue(CharType*& pos, CharType* end)
{
    static const int shortestValidTransformStringLength = 12;

    if (end - pos < shortestValidTransformStringLength)
        return nullptr;

    const bool isTranslate = toASCIILower(pos[0]) == 't'
        && toASCIILower(pos[1]) == 'r'
        && toASCIILower(pos[2]) == 'a'
        && toASCIILower(pos[3]) == 'n'
        && toASCIILower(pos[4]) == 's'
        && toASCIILower(pos[5]) == 'l'
        && toASCIILower(pos[6]) == 'a'
        && toASCIILower(pos[7]) == 't'
        && toASCIILower(pos[8]) == 'e';

    if (isTranslate) {
        CSSTransformValue::TransformOperationType transformType;
        unsigned expectedArgumentCount = 1;
        unsigned argumentStart = 11;
        CharType c9 = toASCIILower(pos[9]);
        if (c9 == 'x' && pos[10] == '(') {
            transformType = CSSTransformValue::TranslateXTransformOperation;
        } else if (c9 == 'y' && pos[10] == '(') {
            transformType = CSSTransformValue::TranslateYTransformOperation;
        } else if (c9 == 'z' && pos[10] == '(') {
            transformType = CSSTransformValue::TranslateZTransformOperation;
        } else if (c9 == '(') {
            transformType = CSSTransformValue::TranslateTransformOperation;
            expectedArgumentCount = 2;
            argumentStart = 10;
        } else if (c9 == '3' && toASCIILower(pos[10]) == 'd' && pos[11] == '(') {
            transformType = CSSTransformValue::Translate3DTransformOperation;
            expectedArgumentCount = 3;
            argumentStart = 12;
        } else {
            return nullptr;
        }
        pos += argumentStart;
        RefPtrWillBeRawPtr<CSSTransformValue> transformValue = CSSTransformValue::create(transformType);
        if (!parseTransformTranslateArguments(pos, end, expectedArgumentCount, transformValue.get()))
            return nullptr;
        return transformValue.release();
    }

    const bool isMatrix3d = toASCIILower(pos[0]) == 'm'
        && toASCIILower(pos[1]) == 'a'
        && toASCIILower(pos[2]) == 't'
        && toASCIILower(pos[3]) == 'r'
        && toASCIILower(pos[4]) == 'i'
        && toASCIILower(pos[5]) == 'x'
        && pos[6] == '3'
        && toASCIILower(pos[7]) == 'd'
        && pos[8] == '(';

    if (isMatrix3d) {
        pos += 9;
        RefPtrWillBeRawPtr<CSSTransformValue> transformValue = CSSTransformValue::create(CSSTransformValue::Matrix3DTransformOperation);
        if (!parseTransformNumberArguments(pos, end, 16, transformValue.get()))
            return nullptr;
        return transformValue.release();
    }

    const bool isScale3d = toASCIILower(pos[0]) == 's'
        && toASCIILower(pos[1]) == 'c'
        && toASCIILower(pos[2]) == 'a'
        && toASCIILower(pos[3]) == 'l'
        && toASCIILower(pos[4]) == 'e'
        && pos[5] == '3'
        && toASCIILower(pos[6]) == 'd'
        && pos[7] == '(';

    if (isScale3d) {
        pos += 8;
        RefPtrWillBeRawPtr<CSSTransformValue> transformValue = CSSTransformValue::create(CSSTransformValue::Scale3DTransformOperation);
        if (!parseTransformNumberArguments(pos, end, 3, transformValue.get()))
            return nullptr;
        return transformValue.release();
    }

    return nullptr;
}

template <typename CharType>
static PassRefPtrWillBeRawPtr<CSSValueList> parseSimpleTransformList(CharType*& pos, CharType* end)
{
    RefPtrWillBeRawPtr<CSSValueList> transformList = nullptr;
    while (pos < end) {
        while (pos < end && isCSSSpace(*pos))
            ++pos;
        RefPtrWillBeRawPtr<CSSTransformValue> transformValue = parseSimpleTransformValue(pos, end);
        if (!transformValue)
            return nullptr;
        if (!transformList)
            transformList = CSSValueList::createSpaceSeparated();
        transformList->append(transformValue.release());
        if (pos < end) {
            if (isCSSSpace(*pos))
                return nullptr;
        }
    }
    return transformList.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> parseSimpleTransform(CSSPropertyID propertyID, const String& string)
{
    if (propertyID != CSSPropertyTransform && propertyID != CSSPropertyWebkitTransform)
        return nullptr;
    if (string.isEmpty())
        return nullptr;
    if (string.is8Bit()) {
        const LChar* pos = string.characters8();
        const LChar* end = pos + string.length();
        return parseSimpleTransformList(pos, end);
    }
    const UChar* pos = string.characters16();
    const UChar* end = pos + string.length();
    return parseSimpleTransformList(pos, end);
}

PassRefPtrWillBeRawPtr<CSSValue> CSSParserFastPaths::maybeParseValue(CSSPropertyID propertyID, const String& string, CSSParserMode parserMode)
{
    if (RefPtrWillBeRawPtr<CSSValue> length = parseSimpleLengthValue(propertyID, string, parserMode))
        return length.release();
    if (RefPtrWillBeRawPtr<CSSValue> color = parseColorValue(propertyID, string, parserMode))
        return color.release();
    if (RefPtrWillBeRawPtr<CSSValue> keyword = parseKeywordValue(propertyID, string))
        return keyword.release();
    if (RefPtrWillBeRawPtr<CSSValue> transform = parseSimpleTransform(propertyID, string))
        return transform.release();
    return nullptr;
}

} // namespace blink
