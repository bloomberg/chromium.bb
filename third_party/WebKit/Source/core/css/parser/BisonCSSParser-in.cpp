/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Nicholas Shanks <webkit@nickshanks.com>
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
 */

#include "config.h"
#include "core/css/parser/BisonCSSParser.h"

#include "CSSValueKeywords.h"
#include "RuntimeEnabledFeatures.h"
#include "StylePropertyShorthand.h"
#include "core/css/CSSArrayFunctionValue.h"
#include "core/css/CSSAspectRatioValue.h"
#include "core/css/CSSBasicShapes.h"
#include "core/css/CSSBorderImage.h"
#include "core/css/CSSCanvasValue.h"
#include "core/css/CSSCrossfadeValue.h"
#include "core/css/CSSCursorImageValue.h"
#include "core/css/CSSFontFaceSrcValue.h"
#include "core/css/CSSFontFeatureValue.h"
#include "core/css/CSSFunctionValue.h"
#include "core/css/CSSGradientValue.h"
#include "core/css/CSSGridLineNamesValue.h"
#include "core/css/CSSGridTemplateAreasValue.h"
#include "core/css/CSSImageSetValue.h"
#include "core/css/CSSImageValue.h"
#include "core/css/CSSInheritedValue.h"
#include "core/css/CSSInitialValue.h"
#include "core/css/CSSKeyframeRule.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/css/CSSLineBoxContainValue.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSPropertySourceData.h"
#include "core/css/CSSReflectValue.h"
#include "core/css/CSSSVGDocumentValue.h"
#include "core/css/CSSSelector.h"
#include "core/css/CSSShadowValue.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/CSSTimingFunctionValue.h"
#include "core/css/CSSTransformValue.h"
#include "core/css/CSSUnicodeRangeValue.h"
#include "core/css/CSSValueList.h"
#include "core/css/CSSValuePool.h"
#include "core/css/Counter.h"
#include "core/css/HashTools.h"
#include "core/css/MediaList.h"
#include "core/css/MediaQueryExp.h"
#include "core/css/Pair.h"
#include "core/css/Rect.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleRuleImport.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/parser/CSSParserIdioms.h"
#include "core/dom/Document.h"
#include "core/frame/FrameHost.h"
#include "core/frame/PageConsole.h"
#include "core/frame/Settings.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/rendering/RenderTheme.h"
#include "core/svg/SVGParserUtilities.h"
#include "platform/FloatConversion.h"
#include "wtf/BitArray.h"
#include "wtf/HexNumber.h"
#include "wtf/text/StringBuffer.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/StringImpl.h"
#include "wtf/text/TextEncoding.h"
#include <limits.h>

#define YYDEBUG 0

#if YYDEBUG > 0
extern int cssyydebug;
#endif

int cssyyparse(WebCore::BisonCSSParser*);

using namespace std;
using namespace WTF;

namespace WebCore {

static const unsigned INVALID_NUM_PARSED_PROPERTIES = UINT_MAX;
static const double MAX_SCALE = 1000000;

template <unsigned N>
static bool equal(const CSSParserString& a, const char (&b)[N])
{
    unsigned length = N - 1; // Ignore the trailing null character
    if (a.length() != length)
        return false;

    return a.is8Bit() ? WTF::equal(a.characters8(), reinterpret_cast<const LChar*>(b), length) : WTF::equal(a.characters16(), reinterpret_cast<const LChar*>(b), length);
}

template <unsigned N>
static bool equalIgnoringCase(const CSSParserString& a, const char (&b)[N])
{
    unsigned length = N - 1; // Ignore the trailing null character
    if (a.length() != length)
        return false;

    return a.is8Bit() ? WTF::equalIgnoringCase(b, a.characters8(), length) : WTF::equalIgnoringCase(b, a.characters16(), length);
}

template <unsigned N>
static bool equalIgnoringCase(CSSParserValue* value, const char (&b)[N])
{
    ASSERT(value->unit == CSSPrimitiveValue::CSS_IDENT || value->unit == CSSPrimitiveValue::CSS_STRING);
    return equalIgnoringCase(value->string, b);
}

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> createPrimitiveValuePair(PassRefPtrWillBeRawPtr<CSSPrimitiveValue> first, PassRefPtrWillBeRawPtr<CSSPrimitiveValue> second, Pair::IdenticalValuesPolicy identicalValuesPolicy = Pair::DropIdenticalValues)
{
    return cssValuePool().createValue(Pair::create(first, second, identicalValuesPolicy));
}

class AnimationParseContext {
public:
    AnimationParseContext()
        : m_animationPropertyKeywordAllowed(true)
        , m_firstAnimationCommitted(false)
        , m_hasSeenAnimationPropertyKeyword(false)
    {
    }

    void commitFirstAnimation()
    {
        m_firstAnimationCommitted = true;
    }

    bool hasCommittedFirstAnimation() const
    {
        return m_firstAnimationCommitted;
    }

    void commitAnimationPropertyKeyword()
    {
        m_animationPropertyKeywordAllowed = false;
    }

    bool animationPropertyKeywordAllowed() const
    {
        return m_animationPropertyKeywordAllowed;
    }

    bool hasSeenAnimationPropertyKeyword() const
    {
        return m_hasSeenAnimationPropertyKeyword;
    }

    void sawAnimationPropertyKeyword()
    {
        m_hasSeenAnimationPropertyKeyword = true;
    }

private:
    bool m_animationPropertyKeywordAllowed;
    bool m_firstAnimationCommitted;
    bool m_hasSeenAnimationPropertyKeyword;
};

BisonCSSParser::BisonCSSParser(const CSSParserContext& context)
    : m_context(context)
    , m_important(false)
    , m_id(CSSPropertyInvalid)
    , m_styleSheet(0)
    , m_supportsCondition(false)
    , m_selectorListForParseSelector(0)
    , m_numParsedPropertiesBeforeMarginBox(INVALID_NUM_PARSED_PROPERTIES)
    , m_hasFontFaceOnlyValues(false)
    , m_hadSyntacticallyValidCSSRule(false)
    , m_logErrors(false)
    , m_ignoreErrors(false)
    , m_defaultNamespace(starAtom)
    , m_observer(0)
    , m_source(0)
    , m_ruleHeaderType(CSSRuleSourceData::UNKNOWN_RULE)
    , m_allowImportRules(true)
    , m_allowNamespaceDeclarations(true)
    , m_inViewport(false)
    , m_tokenizer(*this)
{
#if YYDEBUG > 0
    cssyydebug = 1;
#endif
    CSSPropertySourceData::init();
}

BisonCSSParser::~BisonCSSParser()
{
    clearProperties();

    deleteAllValues(m_floatingSelectors);
    deleteAllValues(m_floatingSelectorVectors);
    deleteAllValues(m_floatingValueLists);
    deleteAllValues(m_floatingFunctions);
}

void BisonCSSParser::setupParser(const char* prefix, unsigned prefixLength, const String& string, const char* suffix, unsigned suffixLength)
{
    m_tokenizer.setupTokenizer(prefix, prefixLength, string, suffix, suffixLength);
    m_ruleHasHeader = true;
}

void BisonCSSParser::parseSheet(StyleSheetContents* sheet, const String& string, const TextPosition& startPosition, CSSParserObserver* observer, bool logErrors)
{
    setStyleSheet(sheet);
    m_defaultNamespace = starAtom; // Reset the default namespace.
    TemporaryChange<CSSParserObserver*> scopedObsever(m_observer, observer);
    m_logErrors = logErrors && sheet->singleOwnerDocument() && !sheet->baseURL().isEmpty() && sheet->singleOwnerDocument()->frameHost();
    m_ignoreErrors = false;
    m_tokenizer.m_lineNumber = 0;
    m_startPosition = startPosition;
    m_source = &string;
    m_tokenizer.m_internal = false;
    setupParser("", string, "");
    cssyyparse(this);
    sheet->shrinkToFit();
    m_source = 0;
    m_rule = nullptr;
    m_lineEndings.clear();
    m_ignoreErrors = false;
    m_logErrors = false;
    m_tokenizer.m_internal = true;
}

PassRefPtrWillBeRawPtr<StyleRuleBase> BisonCSSParser::parseRule(StyleSheetContents* sheet, const String& string)
{
    setStyleSheet(sheet);
    m_allowNamespaceDeclarations = false;
    setupParser("@-internal-rule ", string, "");
    cssyyparse(this);
    return m_rule.release();
}

PassRefPtr<StyleKeyframe> BisonCSSParser::parseKeyframeRule(StyleSheetContents* sheet, const String& string)
{
    setStyleSheet(sheet);
    setupParser("@-internal-keyframe-rule ", string, "");
    cssyyparse(this);
    return m_keyframe.release();
}

PassOwnPtr<Vector<double> > BisonCSSParser::parseKeyframeKeyList(const String& string)
{
    setupParser("@-internal-keyframe-key-list ", string, "");
    cssyyparse(this);
    ASSERT(m_valueList);
    return StyleKeyframe::createKeyList(m_valueList.get());
}

bool BisonCSSParser::parseSupportsCondition(const String& string)
{
    m_supportsCondition = false;
    setupParser("@-internal-supports-condition ", string, "");
    cssyyparse(this);
    return m_supportsCondition;
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
    case CSSPropertyTextLineThroughColor:
    case CSSPropertyTextOverlineColor:
    case CSSPropertyTextUnderlineColor:
    case CSSPropertyWebkitBorderAfterColor:
    case CSSPropertyWebkitBorderBeforeColor:
    case CSSPropertyWebkitBorderEndColor:
    case CSSPropertyWebkitBorderStartColor:
    case CSSPropertyWebkitColumnRuleColor:
    case CSSPropertyWebkitTextEmphasisColor:
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTextStrokeColor:
        return true;
    case CSSPropertyTextDecorationColor:
        return RuntimeEnabledFeatures::css3TextDecorationsEnabled();
    default:
        return false;
    }
}

static bool parseColorValue(MutableStylePropertySet* declaration, CSSPropertyID propertyId, const String& string, bool important, CSSParserMode cssParserMode)
{
    ASSERT(!string.isEmpty());
    bool quirksMode = isQuirksModeBehavior(cssParserMode);
    if (!isColorPropertyID(propertyId))
        return false;
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

    if (validPrimitive) {
        RefPtrWillBeRawPtr<CSSValue> value = cssValuePool().createIdentifierValue(valueID);
        declaration->addParsedProperty(CSSProperty(propertyId, value.release(), important));
        return true;
    }
    RGBA32 color;
    if (!CSSPropertyParser::fastParseColor(color, string, !quirksMode && string[0] != '#'))
        return false;
    RefPtrWillBeRawPtr<CSSValue> value = cssValuePool().createColorValue(color);
    declaration->addParsedProperty(CSSProperty(propertyId, value.release(), important));
    return true;
}

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
        acceptsNegativeNumbers = false;
        return true;
    case CSSPropertyShapeMargin:
    case CSSPropertyShapePadding:
        acceptsNegativeNumbers = false;
        return RuntimeEnabledFeatures::cssShapesEnabled();
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
static inline bool parseSimpleLength(const CharacterType* characters, unsigned length, CSSPrimitiveValue::UnitTypes& unit, double& number)
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

static bool parseSimpleLengthValue(MutableStylePropertySet* declaration, CSSPropertyID propertyId, const String& string, bool important, CSSParserMode cssParserMode)
{
    ASSERT(!string.isEmpty());
    bool acceptsNegativeNumbers;

    // In @viewport, width and height are shorthands, not simple length values.
    if (isCSSViewportParsingEnabledForMode(cssParserMode) || !isSimpleLengthPropertyID(propertyId, acceptsNegativeNumbers))
        return false;

    unsigned length = string.length();
    double number;
    CSSPrimitiveValue::UnitTypes unit = CSSPrimitiveValue::CSS_NUMBER;

    if (string.is8Bit()) {
        if (!parseSimpleLength(string.characters8(), length, unit, number))
            return false;
    } else {
        if (!parseSimpleLength(string.characters16(), length, unit, number))
            return false;
    }

    if (unit == CSSPrimitiveValue::CSS_NUMBER) {
        bool quirksMode = isQuirksModeBehavior(cssParserMode);
        if (number && !quirksMode)
            return false;
        unit = CSSPrimitiveValue::CSS_PX;
    }
    if (number < 0 && !acceptsNegativeNumbers)
        return false;

    RefPtrWillBeRawPtr<CSSValue> value = cssValuePool().createValue(number, unit);
    declaration->addParsedProperty(CSSProperty(propertyId, value.release(), important));
    return true;
}

static inline bool isValidKeywordPropertyAndValue(CSSPropertyID propertyId, int valueID, const CSSParserContext& parserContext)
{
    if (!valueID)
        return false;

    switch (propertyId) {
    case CSSPropertyBorderCollapse: // collapse | separate | inherit
        if (valueID == CSSValueCollapse || valueID == CSSValueSeparate)
            return true;
        break;
    case CSSPropertyBorderTopStyle: // <border-style> | inherit
    case CSSPropertyBorderRightStyle: // Defined as: none | hidden | dotted | dashed |
    case CSSPropertyBorderBottomStyle: // solid | double | groove | ridge | inset | outset
    case CSSPropertyBorderLeftStyle:
    case CSSPropertyWebkitBorderAfterStyle:
    case CSSPropertyWebkitBorderBeforeStyle:
    case CSSPropertyWebkitBorderEndStyle:
    case CSSPropertyWebkitBorderStartStyle:
    case CSSPropertyWebkitColumnRuleStyle:
        if (valueID >= CSSValueNone && valueID <= CSSValueDouble)
            return true;
        break;
    case CSSPropertyBoxSizing:
         if (valueID == CSSValueBorderBox || valueID == CSSValueContentBox)
             return true;
         break;
    case CSSPropertyCaptionSide: // top | bottom | left | right | inherit
        if (valueID == CSSValueLeft || valueID == CSSValueRight || valueID == CSSValueTop || valueID == CSSValueBottom)
            return true;
        break;
    case CSSPropertyClear: // none | left | right | both | inherit
        if (valueID == CSSValueNone || valueID == CSSValueLeft || valueID == CSSValueRight || valueID == CSSValueBoth)
            return true;
        break;
    case CSSPropertyDirection: // ltr | rtl | inherit
        if (valueID == CSSValueLtr || valueID == CSSValueRtl)
            return true;
        break;
    case CSSPropertyDisplay:
        // inline | block | list-item | inline-block | table |
        // inline-table | table-row-group | table-header-group | table-footer-group | table-row |
        // table-column-group | table-column | table-cell | table-caption | -webkit-box | -webkit-inline-box | none | inherit
        // flex | inline-flex | -webkit-flex | -webkit-inline-flex | grid | inline-grid | lazy-block
        if ((valueID >= CSSValueInline && valueID <= CSSValueInlineFlex) || valueID == CSSValueWebkitFlex || valueID == CSSValueWebkitInlineFlex || valueID == CSSValueNone)
            return true;
        if (valueID == CSSValueGrid || valueID == CSSValueInlineGrid)
            return RuntimeEnabledFeatures::cssGridLayoutEnabled();
        break;

    case CSSPropertyEmptyCells: // show | hide | inherit
        if (valueID == CSSValueShow || valueID == CSSValueHide)
            return true;
        break;
    case CSSPropertyFloat: // left | right | none | center (for buggy CSS, maps to none)
        if (valueID == CSSValueLeft || valueID == CSSValueRight || valueID == CSSValueNone || valueID == CSSValueCenter)
            return true;
        break;
    case CSSPropertyFontStyle: // normal | italic | oblique | inherit
        if (valueID == CSSValueNormal || valueID == CSSValueItalic || valueID == CSSValueOblique)
            return true;
        break;
    case CSSPropertyImageRendering: // auto | optimizeContrast
        if (valueID == CSSValueAuto || valueID == CSSValueWebkitOptimizeContrast)
            return true;
        break;
    case CSSPropertyIsolation: // auto | isolate
        if (valueID == CSSValueAuto || valueID == CSSValueIsolate)
            return RuntimeEnabledFeatures::cssCompositingEnabled();
        break;
    case CSSPropertyListStylePosition: // inside | outside | inherit
        if (valueID == CSSValueInside || valueID == CSSValueOutside)
            return true;
        break;
    case CSSPropertyListStyleType:
        // See section CSS_PROP_LIST_STYLE_TYPE of file CSSValueKeywords.in
        // for the list of supported list-style-types.
        if ((valueID >= CSSValueDisc && valueID <= CSSValueKatakanaIroha) || valueID == CSSValueNone)
            return true;
        break;
    case CSSPropertyObjectFit:
        if (RuntimeEnabledFeatures::objectFitPositionEnabled()) {
            if (valueID == CSSValueFill || valueID == CSSValueContain || valueID == CSSValueCover || valueID == CSSValueNone || valueID == CSSValueScaleDown)
                return true;
        }
        break;
    case CSSPropertyOutlineStyle: // (<border-style> except hidden) | auto | inherit
        if (valueID == CSSValueAuto || valueID == CSSValueNone || (valueID >= CSSValueInset && valueID <= CSSValueDouble))
            return true;
        break;
    case CSSPropertyOverflowWrap: // normal | break-word
    case CSSPropertyWordWrap:
        if (valueID == CSSValueNormal || valueID == CSSValueBreakWord)
            return true;
        break;
    case CSSPropertyOverflowX: // visible | hidden | scroll | auto | overlay | inherit
        if (valueID == CSSValueVisible || valueID == CSSValueHidden || valueID == CSSValueScroll || valueID == CSSValueAuto || valueID == CSSValueOverlay)
            return true;
        break;
    case CSSPropertyOverflowY: // visible | hidden | scroll | auto | overlay | inherit | -webkit-paged-x | -webkit-paged-y
        if (valueID == CSSValueVisible || valueID == CSSValueHidden || valueID == CSSValueScroll || valueID == CSSValueAuto || valueID == CSSValueOverlay || valueID == CSSValueWebkitPagedX || valueID == CSSValueWebkitPagedY)
            return true;
        break;
    case CSSPropertyPageBreakAfter: // auto | always | avoid | left | right | inherit
    case CSSPropertyPageBreakBefore:
    case CSSPropertyWebkitColumnBreakAfter:
    case CSSPropertyWebkitColumnBreakBefore:
        if (valueID == CSSValueAuto || valueID == CSSValueAlways || valueID == CSSValueAvoid || valueID == CSSValueLeft || valueID == CSSValueRight)
            return true;
        break;
    case CSSPropertyPageBreakInside: // avoid | auto | inherit
    case CSSPropertyWebkitColumnBreakInside:
        if (valueID == CSSValueAuto || valueID == CSSValueAvoid)
            return true;
        break;
    case CSSPropertyPointerEvents:
        // none | visiblePainted | visibleFill | visibleStroke | visible |
        // painted | fill | stroke | auto | all | bounding-box | inherit
        if (valueID == CSSValueVisible || valueID == CSSValueNone || valueID == CSSValueAll || valueID == CSSValueAuto || (valueID >= CSSValueVisiblepainted && valueID <= CSSValueBoundingBox))
            return true;
        break;
    case CSSPropertyPosition: // static | relative | absolute | fixed | sticky | inherit
        if (valueID == CSSValueStatic || valueID == CSSValueRelative || valueID == CSSValueAbsolute || valueID == CSSValueFixed
            || (RuntimeEnabledFeatures::cssStickyPositionEnabled() && valueID == CSSValueSticky))
            return true;
        break;
    case CSSPropertyResize: // none | both | horizontal | vertical | auto
        if (valueID == CSSValueNone || valueID == CSSValueBoth || valueID == CSSValueHorizontal || valueID == CSSValueVertical || valueID == CSSValueAuto)
            return true;
        break;
    case CSSPropertyScrollBehavior: // instant | smooth
        if (valueID == CSSValueInstant || valueID == CSSValueSmooth)
            return RuntimeEnabledFeatures::cssomSmoothScrollEnabled();
    case CSSPropertySpeak: // none | normal | spell-out | digits | literal-punctuation | no-punctuation | inherit
        if (valueID == CSSValueNone || valueID == CSSValueNormal || valueID == CSSValueSpellOut || valueID == CSSValueDigits || valueID == CSSValueLiteralPunctuation || valueID == CSSValueNoPunctuation)
            return true;
        break;
    case CSSPropertyTableLayout: // auto | fixed | inherit
        if (valueID == CSSValueAuto || valueID == CSSValueFixed)
            return true;
        break;
    case CSSPropertyTextAlignLast:
        // auto | start | end | left | right | center | justify
        if (RuntimeEnabledFeatures::css3TextEnabled()
            && ((valueID >= CSSValueLeft && valueID <= CSSValueJustify) || valueID == CSSValueStart || valueID == CSSValueEnd || valueID == CSSValueAuto))
            return true;
        break;
    case CSSPropertyTextJustify:
        // auto | none | inter-word | distribute
        if (RuntimeEnabledFeatures::css3TextEnabled()
            && (valueID == CSSValueInterWord || valueID == CSSValueDistribute || valueID == CSSValueAuto || valueID == CSSValueNone))
            return true;
        break;
    case CSSPropertyTextLineThroughMode:
    case CSSPropertyTextOverlineMode:
    case CSSPropertyTextUnderlineMode:
        if (valueID == CSSValueContinuous || valueID == CSSValueSkipWhiteSpace)
            return true;
        break;
    case CSSPropertyTextLineThroughStyle:
    case CSSPropertyTextOverlineStyle:
    case CSSPropertyTextUnderlineStyle:
        if (valueID == CSSValueNone || valueID == CSSValueSolid || valueID == CSSValueDouble || valueID == CSSValueDashed || valueID == CSSValueDotDash || valueID == CSSValueDotDotDash || valueID == CSSValueWave)
            return true;
        break;
    case CSSPropertyTextOverflow: // clip | ellipsis
        if (valueID == CSSValueClip || valueID == CSSValueEllipsis)
            return true;
        break;
    case CSSPropertyTextRendering: // auto | optimizeSpeed | optimizeLegibility | geometricPrecision
        if (valueID == CSSValueAuto || valueID == CSSValueOptimizespeed || valueID == CSSValueOptimizelegibility || valueID == CSSValueGeometricprecision)
            return true;
        break;
    case CSSPropertyTextTransform: // capitalize | uppercase | lowercase | none | inherit
        if ((valueID >= CSSValueCapitalize && valueID <= CSSValueLowercase) || valueID == CSSValueNone)
            return true;
        break;
    case CSSPropertyTouchActionDelay: // none | script
        if (RuntimeEnabledFeatures::cssTouchActionDelayEnabled() && (valueID == CSSValueScript || valueID == CSSValueNone))
            return true;
        break;
    case CSSPropertyVisibility: // visible | hidden | collapse | inherit
        if (valueID == CSSValueVisible || valueID == CSSValueHidden || valueID == CSSValueCollapse)
            return true;
        break;
    case CSSPropertyWebkitAppearance:
        if ((valueID >= CSSValueCheckbox && valueID <= CSSValueTextarea) || valueID == CSSValueNone)
            return true;
        break;
    case CSSPropertyWebkitBackfaceVisibility:
        if (valueID == CSSValueVisible || valueID == CSSValueHidden)
            return true;
        break;
    case CSSPropertyMixBlendMode:
        if (RuntimeEnabledFeatures::cssCompositingEnabled() && (valueID == CSSValueNormal || valueID == CSSValueMultiply || valueID == CSSValueScreen
            || valueID == CSSValueOverlay || valueID == CSSValueDarken || valueID == CSSValueLighten ||  valueID == CSSValueColorDodge
            || valueID == CSSValueColorBurn || valueID == CSSValueHardLight || valueID == CSSValueSoftLight || valueID == CSSValueDifference
            || valueID == CSSValueExclusion || valueID == CSSValueHue || valueID == CSSValueSaturation || valueID == CSSValueColor
            || valueID == CSSValueLuminosity))
            return true;
        break;
    case CSSPropertyWebkitBorderFit:
        if (valueID == CSSValueBorder || valueID == CSSValueLines)
            return true;
        break;
    case CSSPropertyWebkitBoxAlign:
        if (valueID == CSSValueStretch || valueID == CSSValueStart || valueID == CSSValueEnd || valueID == CSSValueCenter || valueID == CSSValueBaseline)
            return true;
        break;
    case CSSPropertyWebkitBoxDecorationBreak:
         if (valueID == CSSValueClone || valueID == CSSValueSlice)
             return true;
         break;
    case CSSPropertyWebkitBoxDirection:
        if (valueID == CSSValueNormal || valueID == CSSValueReverse)
            return true;
        break;
    case CSSPropertyWebkitBoxLines:
        if (valueID == CSSValueSingle || valueID == CSSValueMultiple)
                return true;
        break;
    case CSSPropertyWebkitBoxOrient:
        if (valueID == CSSValueHorizontal || valueID == CSSValueVertical || valueID == CSSValueInlineAxis || valueID == CSSValueBlockAxis)
            return true;
        break;
    case CSSPropertyWebkitBoxPack:
        if (valueID == CSSValueStart || valueID == CSSValueEnd || valueID == CSSValueCenter || valueID == CSSValueJustify)
            return true;
        break;
    case CSSPropertyInternalCallback:
        // This property is only injected programmatically, not parsed from stylesheets.
        return false;
    case CSSPropertyColumnFill:
        if (RuntimeEnabledFeatures::regionBasedColumnsEnabled()) {
            if (valueID == CSSValueAuto || valueID == CSSValueBalance)
                return true;
        }
        break;
    case CSSPropertyAlignContent:
        // FIXME: Per CSS alignment, this property should accept an optional <overflow-position>. We should share this parsing code with 'justify-self'.
         if (valueID == CSSValueFlexStart || valueID == CSSValueFlexEnd || valueID == CSSValueCenter || valueID == CSSValueSpaceBetween || valueID == CSSValueSpaceAround || valueID == CSSValueStretch)
             return true;
         break;
    case CSSPropertyAlignItems:
        // FIXME: Per CSS alignment, this property should accept the same arguments as 'justify-self' so we should share its parsing code.
        if (valueID == CSSValueFlexStart || valueID == CSSValueFlexEnd || valueID == CSSValueCenter || valueID == CSSValueBaseline || valueID == CSSValueStretch)
            return true;
        break;
    case CSSPropertyAlignSelf:
        // FIXME: Per CSS alignment, this property should accept the same arguments as 'justify-self' so we should share its parsing code.
        if (valueID == CSSValueAuto || valueID == CSSValueFlexStart || valueID == CSSValueFlexEnd || valueID == CSSValueCenter || valueID == CSSValueBaseline || valueID == CSSValueStretch)
            return true;
        break;
    case CSSPropertyFlexDirection:
        if (valueID == CSSValueRow || valueID == CSSValueRowReverse || valueID == CSSValueColumn || valueID == CSSValueColumnReverse)
            return true;
        break;
    case CSSPropertyFlexWrap:
        if (valueID == CSSValueNowrap || valueID == CSSValueWrap || valueID == CSSValueWrapReverse)
             return true;
        break;
    case CSSPropertyJustifyContent:
        // FIXME: Per CSS alignment, this property should accept an optional <overflow-position>. We should share this parsing code with 'justify-self'.
        if (valueID == CSSValueFlexStart || valueID == CSSValueFlexEnd || valueID == CSSValueCenter || valueID == CSSValueSpaceBetween || valueID == CSSValueSpaceAround)
            return true;
        break;
    case CSSPropertyFontKerning:
        if (valueID == CSSValueAuto || valueID == CSSValueNormal || valueID == CSSValueNone)
            return true;
        break;
    case CSSPropertyWebkitFontSmoothing:
        if (valueID == CSSValueAuto || valueID == CSSValueNone || valueID == CSSValueAntialiased || valueID == CSSValueSubpixelAntialiased)
            return true;
        break;
    case CSSPropertyGridAutoFlow:
        if (valueID == CSSValueNone || valueID == CSSValueRow || valueID == CSSValueColumn)
            return RuntimeEnabledFeatures::cssGridLayoutEnabled();
        break;
    case CSSPropertyWebkitLineBreak: // auto | loose | normal | strict | after-white-space
        if (valueID == CSSValueAuto || valueID == CSSValueLoose || valueID == CSSValueNormal || valueID == CSSValueStrict || valueID == CSSValueAfterWhiteSpace)
            return true;
        break;
    case CSSPropertyWebkitMarginAfterCollapse:
    case CSSPropertyWebkitMarginBeforeCollapse:
    case CSSPropertyWebkitMarginBottomCollapse:
    case CSSPropertyWebkitMarginTopCollapse:
        if (valueID == CSSValueCollapse || valueID == CSSValueSeparate || valueID == CSSValueDiscard)
            return true;
        break;
    case CSSPropertyInternalMarqueeDirection:
        if (valueID == CSSValueForwards || valueID == CSSValueBackwards || valueID == CSSValueAhead || valueID == CSSValueReverse || valueID == CSSValueLeft || valueID == CSSValueRight || valueID == CSSValueDown
            || valueID == CSSValueUp || valueID == CSSValueAuto)
            return true;
        break;
    case CSSPropertyInternalMarqueeStyle:
        if (valueID == CSSValueNone || valueID == CSSValueSlide || valueID == CSSValueScroll || valueID == CSSValueAlternate)
            return true;
        break;
    case CSSPropertyWebkitPrintColorAdjust:
        if (valueID == CSSValueExact || valueID == CSSValueEconomy)
            return true;
        break;
    case CSSPropertyWebkitRtlOrdering:
        if (valueID == CSSValueLogical || valueID == CSSValueVisual)
            return true;
        break;

    case CSSPropertyWebkitRubyPosition:
        if (valueID == CSSValueBefore || valueID == CSSValueAfter)
            return true;
        break;

    case CSSPropertyWebkitTextCombine:
        if (valueID == CSSValueNone || valueID == CSSValueHorizontal)
            return true;
        break;
    case CSSPropertyWebkitTextEmphasisPosition:
        if (valueID == CSSValueOver || valueID == CSSValueUnder)
            return true;
        break;
    case CSSPropertyWebkitTextSecurity:
        // disc | circle | square | none | inherit
        if (valueID == CSSValueDisc || valueID == CSSValueCircle || valueID == CSSValueSquare || valueID == CSSValueNone)
            return true;
        break;
    case CSSPropertyWebkitTransformStyle:
        if (valueID == CSSValueFlat || valueID == CSSValuePreserve3d)
            return true;
        break;
    case CSSPropertyWebkitUserDrag: // auto | none | element
        if (valueID == CSSValueAuto || valueID == CSSValueNone || valueID == CSSValueElement)
            return true;
        break;
    case CSSPropertyWebkitUserModify: // read-only | read-write
        if (valueID == CSSValueReadOnly || valueID == CSSValueReadWrite || valueID == CSSValueReadWritePlaintextOnly)
            return true;
        break;
    case CSSPropertyWebkitUserSelect: // auto | none | text | all
        if (valueID == CSSValueAuto || valueID == CSSValueNone || valueID == CSSValueText || valueID == CSSValueAll)
            return true;
        break;
    case CSSPropertyWebkitWrapFlow:
        if (!RuntimeEnabledFeatures::cssExclusionsEnabled())
            return false;
        if (valueID == CSSValueAuto || valueID == CSSValueBoth || valueID == CSSValueStart || valueID == CSSValueEnd || valueID == CSSValueMaximum || valueID == CSSValueClear)
            return true;
        break;
    case CSSPropertyWebkitWrapThrough:
        if (!RuntimeEnabledFeatures::cssExclusionsEnabled())
            return false;
        if (valueID == CSSValueWrap || valueID == CSSValueNone)
            return true;
        break;
    case CSSPropertyWebkitWritingMode:
        if (valueID >= CSSValueHorizontalTb && valueID <= CSSValueHorizontalBt)
            return true;
        break;
    case CSSPropertyWhiteSpace: // normal | pre | nowrap | inherit
        if (valueID == CSSValueNormal || valueID == CSSValuePre || valueID == CSSValuePreWrap || valueID == CSSValuePreLine || valueID == CSSValueNowrap)
            return true;
        break;
    case CSSPropertyWordBreak: // normal | break-all | break-word (this is a custom extension)
        if (valueID == CSSValueNormal || valueID == CSSValueBreakAll || valueID == CSSValueBreakWord)
            return true;
        break;
    default:
        ASSERT_NOT_REACHED();
        return false;
    }
    return false;
}

static inline bool isKeywordPropertyID(CSSPropertyID propertyId)
{
    switch (propertyId) {
    case CSSPropertyMixBlendMode:
    case CSSPropertyIsolation:
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
    case CSSPropertyTextJustify:
    case CSSPropertyTextLineThroughMode:
    case CSSPropertyTextLineThroughStyle:
    case CSSPropertyTextOverflow:
    case CSSPropertyTextOverlineMode:
    case CSSPropertyTextOverlineStyle:
    case CSSPropertyTextRendering:
    case CSSPropertyTextTransform:
    case CSSPropertyTextUnderlineMode:
    case CSSPropertyTextUnderlineStyle:
    case CSSPropertyTouchActionDelay:
    case CSSPropertyVisibility:
    case CSSPropertyWebkitAppearance:
    case CSSPropertyWebkitBackfaceVisibility:
    case CSSPropertyWebkitBorderAfterStyle:
    case CSSPropertyWebkitBorderBeforeStyle:
    case CSSPropertyWebkitBorderEndStyle:
    case CSSPropertyWebkitBorderFit:
    case CSSPropertyWebkitBorderStartStyle:
    case CSSPropertyWebkitBoxAlign:
    case CSSPropertyWebkitBoxDecorationBreak:
    case CSSPropertyWebkitBoxDirection:
    case CSSPropertyWebkitBoxLines:
    case CSSPropertyWebkitBoxOrient:
    case CSSPropertyWebkitBoxPack:
    case CSSPropertyInternalCallback:
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
    case CSSPropertyGridAutoFlow:
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
    case CSSPropertyWebkitTransformStyle:
    case CSSPropertyWebkitUserDrag:
    case CSSPropertyWebkitUserModify:
    case CSSPropertyWebkitUserSelect:
    case CSSPropertyWebkitWrapFlow:
    case CSSPropertyWebkitWrapThrough:
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

static bool parseKeywordValue(MutableStylePropertySet* declaration, CSSPropertyID propertyId, const String& string, bool important, const CSSParserContext& parserContext)
{
    ASSERT(!string.isEmpty());

    if (!isKeywordPropertyID(propertyId)) {
        // All properties accept the values of "initial" and "inherit".
        String lowerCaseString = string.lower();
        if (lowerCaseString != "initial" && lowerCaseString != "inherit")
            return false;

        // Parse initial/inherit shorthands using the BisonCSSParser.
        if (shorthandForProperty(propertyId).length())
            return false;
    }

    CSSParserString cssString;
    cssString.init(string);
    CSSValueID valueID = cssValueKeywordID(cssString);

    if (!valueID)
        return false;

    RefPtrWillBeRawPtr<CSSValue> value;
    if (valueID == CSSValueInherit)
        value = cssValuePool().createInheritedValue();
    else if (valueID == CSSValueInitial)
        value = cssValuePool().createExplicitInitialValue();
    else if (isValidKeywordPropertyAndValue(propertyId, valueID, parserContext))
        value = cssValuePool().createIdentifierValue(valueID);
    else
        return false;

    declaration->addParsedProperty(CSSProperty(propertyId, value.release(), important));
    return true;
}

template <typename CharType>
static bool parseTransformTranslateArguments(CharType*& pos, CharType* end, unsigned expectedCount, CSSTransformValue* transformValue)
{
    while (expectedCount) {
        size_t delimiter = WTF::find(pos, end - pos, expectedCount == 1 ? ')' : ',');
        if (delimiter == kNotFound)
            return false;
        unsigned argumentLength = static_cast<unsigned>(delimiter);
        CSSPrimitiveValue::UnitTypes unit = CSSPrimitiveValue::CSS_NUMBER;
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
static PassRefPtrWillBeRawPtr<CSSTransformValue> parseTranslateTransformValue(CharType*& pos, CharType* end)
{
    static const int shortestValidTransformStringLength = 12;

    if (end - pos < shortestValidTransformStringLength)
        return nullptr;

    if ((pos[0] != 't' && pos[0] != 'T')
        || (pos[1] != 'r' && pos[1] != 'R')
        || (pos[2] != 'a' && pos[2] != 'A')
        || (pos[3] != 'n' && pos[3] != 'N')
        || (pos[4] != 's' && pos[4] != 'S')
        || (pos[5] != 'l' && pos[5] != 'L')
        || (pos[6] != 'a' && pos[6] != 'A')
        || (pos[7] != 't' && pos[7] != 'T')
        || (pos[8] != 'e' && pos[8] != 'E'))
        return nullptr;

    CSSTransformValue::TransformOperationType transformType;
    unsigned expectedArgumentCount = 1;
    unsigned argumentStart = 11;
    if ((pos[9] == 'x' || pos[9] == 'X') && pos[10] == '(') {
        transformType = CSSTransformValue::TranslateXTransformOperation;
    } else if ((pos[9] == 'y' || pos[9] == 'Y') && pos[10] == '(') {
        transformType = CSSTransformValue::TranslateYTransformOperation;
    } else if ((pos[9] == 'z' || pos[9] == 'Z') && pos[10] == '(') {
        transformType = CSSTransformValue::TranslateZTransformOperation;
    } else if (pos[9] == '(') {
        transformType = CSSTransformValue::TranslateTransformOperation;
        expectedArgumentCount = 2;
        argumentStart = 10;
    } else if (pos[9] == '3' && (pos[10] == 'd' || pos[10] == 'D') && pos[11] == '(') {
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

template <typename CharType>
static PassRefPtrWillBeRawPtr<CSSValueList> parseTranslateTransformList(CharType*& pos, CharType* end)
{
    RefPtrWillBeRawPtr<CSSValueList> transformList;
    while (pos < end) {
        while (pos < end && isCSSSpace(*pos))
            ++pos;
        RefPtrWillBeRawPtr<CSSTransformValue> transformValue = parseTranslateTransformValue(pos, end);
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

static bool parseTranslateTransform(MutableStylePropertySet* properties, CSSPropertyID propertyID, const String& string, bool important)
{
    if (propertyID != CSSPropertyWebkitTransform)
        return false;
    if (string.isEmpty())
        return false;
    RefPtrWillBeRawPtr<CSSValueList> transformList;
    if (string.is8Bit()) {
        const LChar* pos = string.characters8();
        const LChar* end = pos + string.length();
        transformList = parseTranslateTransformList(pos, end);
        if (!transformList)
            return false;
    } else {
        const UChar* pos = string.characters16();
        const UChar* end = pos + string.length();
        transformList = parseTranslateTransformList(pos, end);
        if (!transformList)
            return false;
    }
    properties->addParsedProperty(CSSProperty(CSSPropertyWebkitTransform, transformList.release(), important));
    return true;
}

PassRefPtrWillBeRawPtr<CSSValueList> BisonCSSParser::parseFontFaceValue(const AtomicString& string)
{
    if (string.isEmpty())
        return nullptr;
    RefPtr<MutableStylePropertySet> dummyStyle = MutableStylePropertySet::create();
    if (!parseValue(dummyStyle.get(), CSSPropertyFontFamily, string, false, HTMLQuirksMode, 0))
        return nullptr;

    RefPtrWillBeRawPtr<CSSValue> fontFamily = dummyStyle->getPropertyCSSValue(CSSPropertyFontFamily);
    if (!fontFamily->isValueList())
        return nullptr;

    return toCSSValueList(dummyStyle->getPropertyCSSValue(CSSPropertyFontFamily).get());
}

PassRefPtrWillBeRawPtr<CSSValue> BisonCSSParser::parseAnimationTimingFunctionValue(const String& string)
{
    if (string.isEmpty())
        return nullptr;
    RefPtr<MutableStylePropertySet> style = MutableStylePropertySet::create();
    if (!parseValue(style.get(), CSSPropertyAnimationTimingFunction, string, false, HTMLStandardMode, 0))
        return nullptr;

    return style->getPropertyCSSValue(CSSPropertyAnimationTimingFunction);
}

bool BisonCSSParser::parseValue(MutableStylePropertySet* declaration, CSSPropertyID propertyID, const String& string, bool important, const Document& document)
{
    ASSERT(!string.isEmpty());

    CSSParserContext context(document, UseCounter::getFrom(&document));

    if (parseSimpleLengthValue(declaration, propertyID, string, important, context.mode()))
        return true;
    if (parseColorValue(declaration, propertyID, string, important, context.mode()))
        return true;
    if (parseKeywordValue(declaration, propertyID, string, important, context))
        return true;

    BisonCSSParser parser(context);
    return parser.parseValue(declaration, propertyID, string, important, static_cast<StyleSheetContents*>(0));
}

bool BisonCSSParser::parseValue(MutableStylePropertySet* declaration, CSSPropertyID propertyID, const String& string, bool important, CSSParserMode cssParserMode, StyleSheetContents* contextStyleSheet)
{
    ASSERT(!string.isEmpty());
    if (parseSimpleLengthValue(declaration, propertyID, string, important, cssParserMode))
        return true;
    if (parseColorValue(declaration, propertyID, string, important, cssParserMode))
        return true;

    CSSParserContext context(cssParserMode, 0);
    if (contextStyleSheet) {
        context = contextStyleSheet->parserContext();
        context.setMode(cssParserMode);
    }

    if (parseKeywordValue(declaration, propertyID, string, important, context))
        return true;
    if (parseTranslateTransform(declaration, propertyID, string, important))
        return true;

    BisonCSSParser parser(context);
    return parser.parseValue(declaration, propertyID, string, important, contextStyleSheet);
}

bool BisonCSSParser::parseValue(MutableStylePropertySet* declaration, CSSPropertyID propertyID, const String& string, bool important, StyleSheetContents* contextStyleSheet)
{
    // FIXME: Check RuntimeCSSEnabled::isPropertyEnabled or isValueEnabledForProperty.

    if (m_context.useCounter())
        m_context.useCounter()->count(m_context, propertyID);

    setStyleSheet(contextStyleSheet);

    setupParser("@-internal-value ", string, "");

    m_id = propertyID;
    m_important = important;

    {
        StyleDeclarationScope scope(this, declaration);
        cssyyparse(this);
    }

    m_rule = nullptr;
    m_id = CSSPropertyInvalid;

    bool ok = false;
    if (m_hasFontFaceOnlyValues)
        deleteFontFaceOnlyValues();
    if (!m_parsedProperties.isEmpty()) {
        ok = true;
        declaration->addParsedProperties(m_parsedProperties);
        clearProperties();
    }

    return ok;
}

// The color will only be changed when string contains a valid CSS color, so callers
// can set it to a default color and ignore the boolean result.
bool BisonCSSParser::parseColor(RGBA32& color, const String& string, bool strict)
{
    // First try creating a color specified by name, rgba(), rgb() or "#" syntax.
    if (CSSPropertyParser::fastParseColor(color, string, strict))
        return true;

    BisonCSSParser parser(strictCSSParserContext());

    // In case the fast-path parser didn't understand the color, try the full parser.
    if (!parser.parseColor(string))
        return false;

    CSSValue* value = parser.m_parsedProperties.first().value();
    if (!value->isPrimitiveValue())
        return false;

    CSSPrimitiveValue* primitiveValue = toCSSPrimitiveValue(value);
    if (!primitiveValue->isRGBColor())
        return false;

    color = primitiveValue->getRGBA32Value();
    return true;
}

bool BisonCSSParser::parseColor(const String& string)
{
    setupParser("@-internal-decls color:", string, "");
    cssyyparse(this);
    m_rule = nullptr;

    return !m_parsedProperties.isEmpty() && m_parsedProperties.first().id() == CSSPropertyColor;
}

// FIXME: This is copied from SVGCSSParser.cpp
static bool isSystemColor(int id)
{
    return (id >= CSSValueActiveborder && id <= CSSValueWindowtext) || id == CSSValueMenu;
}

bool BisonCSSParser::parseSystemColor(RGBA32& color, const String& string, Document* document)
{
    if (!document)
        return false;

    CSSParserString cssColor;
    cssColor.init(string);
    CSSValueID id = cssValueKeywordID(cssColor);
    if (!isSystemColor(id))
        return false;

    Color parsedColor = RenderTheme::theme().systemColor(id);
    color = parsedColor.rgb();
    return true;
}

void BisonCSSParser::parseSelector(const String& string, CSSSelectorList& selectorList)
{
    m_selectorListForParseSelector = &selectorList;

    setupParser("@-internal-selector ", string, "");

    cssyyparse(this);

    m_selectorListForParseSelector = 0;
}

PassRefPtr<ImmutableStylePropertySet> BisonCSSParser::parseInlineStyleDeclaration(const String& string, Element* element)
{
    Document& document = element->document();
    CSSParserContext context = CSSParserContext(document.elementSheet()->contents()->parserContext(), UseCounter::getFrom(&document));
    context.setMode((element->isHTMLElement() && !document.inQuirksMode()) ? HTMLStandardMode : HTMLQuirksMode);
    return BisonCSSParser(context).parseDeclaration(string, document.elementSheet()->contents());
}

PassRefPtr<ImmutableStylePropertySet> BisonCSSParser::parseDeclaration(const String& string, StyleSheetContents* contextStyleSheet)
{
    setStyleSheet(contextStyleSheet);

    setupParser("@-internal-decls ", string, "");
    cssyyparse(this);
    m_rule = nullptr;

    if (m_hasFontFaceOnlyValues)
        deleteFontFaceOnlyValues();

    RefPtr<ImmutableStylePropertySet> style = createStylePropertySet();
    clearProperties();
    return style.release();
}


bool BisonCSSParser::parseDeclaration(MutableStylePropertySet* declaration, const String& string, CSSParserObserver* observer, StyleSheetContents* contextStyleSheet)
{
    setStyleSheet(contextStyleSheet);

    TemporaryChange<CSSParserObserver*> scopedObsever(m_observer, observer);

    setupParser("@-internal-decls ", string, "");
    if (m_observer) {
        m_observer->startRuleHeader(CSSRuleSourceData::STYLE_RULE, 0);
        m_observer->endRuleHeader(1);
        m_observer->startRuleBody(0);
    }

    {
        StyleDeclarationScope scope(this, declaration);
        cssyyparse(this);
    }

    m_rule = nullptr;

    bool ok = false;
    if (m_hasFontFaceOnlyValues)
        deleteFontFaceOnlyValues();
    if (!m_parsedProperties.isEmpty()) {
        ok = true;
        declaration->addParsedProperties(m_parsedProperties);
        clearProperties();
    }

    if (m_observer)
        m_observer->endRuleBody(string.length(), false);

    return ok;
}

PassRefPtr<MediaQuerySet> BisonCSSParser::parseMediaQueryList(const String& string)
{
    ASSERT(!m_mediaList);

    // can't use { because tokenizer state switches from mediaquery to initial state when it sees { token.
    // instead insert one " " (which is caught by maybe_space in CSSGrammar.y)
    setupParser("@-internal-medialist ", string, "");
    cssyyparse(this);

    ASSERT(m_mediaList);
    return m_mediaList.release();
}

static inline void filterProperties(bool important, const CSSPropertyParser::ParsedPropertyVector& input, Vector<CSSProperty, 256>& output, size_t& unusedEntries, BitArray<numCSSProperties>& seenProperties)
{
    // Add properties in reverse order so that highest priority definitions are reached first. Duplicate definitions can then be ignored when found.
    for (int i = input.size() - 1; i >= 0; --i) {
        const CSSProperty& property = input[i];
        if (property.isImportant() != important)
            continue;
        const unsigned propertyIDIndex = property.id() - firstCSSProperty;
        if (seenProperties.get(propertyIDIndex))
            continue;
        seenProperties.set(propertyIDIndex);
        output[--unusedEntries] = property;
    }
}

PassRefPtr<ImmutableStylePropertySet> BisonCSSParser::createStylePropertySet()
{
    BitArray<numCSSProperties> seenProperties;
    size_t unusedEntries = m_parsedProperties.size();
    Vector<CSSProperty, 256> results(unusedEntries);

    // Important properties have higher priority, so add them first. Duplicate definitions can then be ignored when found.
    filterProperties(true, m_parsedProperties, results, unusedEntries, seenProperties);
    filterProperties(false, m_parsedProperties, results, unusedEntries, seenProperties);
    if (unusedEntries)
        results.remove(0, unusedEntries);

    CSSParserMode mode = inViewport() ? CSSViewportRuleMode : m_context.mode();

    return ImmutableStylePropertySet::create(results.data(), results.size(), mode);
}

void CSSPropertyParser::addPropertyWithPrefixingVariant(CSSPropertyID propId, PassRefPtrWillBeRawPtr<CSSValue> value, bool important, bool implicit)
{
    RefPtrWillBeRawPtr<CSSValue> val = value.get();
    addProperty(propId, value, important, implicit);

    CSSPropertyID prefixingVariant = prefixingVariantForPropertyId(propId);
    if (prefixingVariant == propId)
        return;

    if (m_currentShorthand) {
        // We can't use ShorthandScope here as we can already be inside one (e.g we are parsing CSSTransition).
        m_currentShorthand = prefixingVariantForPropertyId(m_currentShorthand);
        addProperty(prefixingVariant, val.release(), important, implicit);
        m_currentShorthand = prefixingVariantForPropertyId(m_currentShorthand);
    } else {
        addProperty(prefixingVariant, val.release(), important, implicit);
    }
}

void CSSPropertyParser::addProperty(CSSPropertyID propId, PassRefPtrWillBeRawPtr<CSSValue> value, bool important, bool implicit)
{
    // This property doesn't belong to a shorthand.
    if (!m_currentShorthand) {
        m_parsedProperties.append(CSSProperty(propId, value, important, false, CSSPropertyInvalid, m_implicitShorthand || implicit));
        return;
    }

    Vector<StylePropertyShorthand, 4> shorthands;
    getMatchingShorthandsForLonghand(propId, &shorthands);
    // The longhand does not belong to multiple shorthands.
    if (shorthands.size() == 1)
        m_parsedProperties.append(CSSProperty(propId, value, important, true, CSSPropertyInvalid, m_implicitShorthand || implicit));
    else
        m_parsedProperties.append(CSSProperty(propId, value, important, true, indexOfShorthandForLonghand(m_currentShorthand, shorthands), m_implicitShorthand || implicit));
}

void BisonCSSParser::rollbackLastProperties(int num)
{
    ASSERT(num >= 0);
    ASSERT(m_parsedProperties.size() >= static_cast<unsigned>(num));
    m_parsedProperties.shrink(m_parsedProperties.size() - num);
}

void CSSPropertyParser::rollbackLastProperties(int num)
{
    ASSERT(num >= 0);
    ASSERT(m_parsedProperties.size() >= static_cast<unsigned>(num));
    m_parsedProperties.shrink(m_parsedProperties.size() - num);
}

void BisonCSSParser::clearProperties()
{
    m_parsedProperties.clear();
    m_numParsedPropertiesBeforeMarginBox = INVALID_NUM_PARSED_PROPERTIES;
    m_hasFontFaceOnlyValues = false;
}

KURL CSSPropertyParser::completeURL(const String& url) const
{
    return m_context.completeURL(url);
}

bool CSSPropertyParser::validCalculationUnit(CSSParserValue* value, Units unitflags, ReleaseParsedCalcValueCondition releaseCalc)
{
    bool mustBeNonNegative = unitflags & FNonNeg;

    if (!parseCalculation(value, mustBeNonNegative ? ValueRangeNonNegative : ValueRangeAll))
        return false;

    bool b = false;
    switch (m_parsedCalculation->category()) {
    case CalcLength:
        b = (unitflags & FLength);
        break;
    case CalcPercent:
        b = (unitflags & FPercent);
        if (b && mustBeNonNegative && m_parsedCalculation->isNegative())
            b = false;
        break;
    case CalcNumber:
        b = (unitflags & FNumber);
        if (!b && (unitflags & FInteger) && m_parsedCalculation->isInt())
            b = true;
        if (b && mustBeNonNegative && m_parsedCalculation->isNegative())
            b = false;
        break;
    case CalcPercentLength:
        b = (unitflags & FPercent) && (unitflags & FLength);
        break;
    case CalcPercentNumber:
        b = (unitflags & FPercent) && (unitflags & FNumber);
        break;
    case CalcOther:
        break;
    }
    if (!b || releaseCalc == ReleaseParsedCalcValue)
        m_parsedCalculation.release();
    return b;
}

inline bool CSSPropertyParser::shouldAcceptUnitLessValues(CSSParserValue* value, Units unitflags, CSSParserMode cssParserMode)
{
    // Quirks mode and presentation attributes accept unit less values.
    return (unitflags & (FLength | FAngle | FTime)) && (!value->fValue || isUnitLessLengthParsingEnabledForMode(cssParserMode));
}

bool CSSPropertyParser::validUnit(CSSParserValue* value, Units unitflags, CSSParserMode cssParserMode, ReleaseParsedCalcValueCondition releaseCalc)
{
    if (isCalculation(value))
        return validCalculationUnit(value, unitflags, releaseCalc);

    bool b = false;
    switch (value->unit) {
    case CSSPrimitiveValue::CSS_NUMBER:
        b = (unitflags & FNumber);
        if (!b && shouldAcceptUnitLessValues(value, unitflags, cssParserMode)) {
            value->unit = (unitflags & FLength) ? CSSPrimitiveValue::CSS_PX :
                          ((unitflags & FAngle) ? CSSPrimitiveValue::CSS_DEG : CSSPrimitiveValue::CSS_MS);
            b = true;
        }
        if (!b && (unitflags & FInteger) && value->isInt)
            b = true;
        if (!b && (unitflags & FPositiveInteger) && value->isInt && value->fValue > 0)
            b = true;
        break;
    case CSSPrimitiveValue::CSS_PERCENTAGE:
        b = (unitflags & FPercent);
        break;
    case CSSParserValue::Q_EMS:
    case CSSPrimitiveValue::CSS_EMS:
    case CSSPrimitiveValue::CSS_REMS:
    case CSSPrimitiveValue::CSS_CHS:
    case CSSPrimitiveValue::CSS_EXS:
    case CSSPrimitiveValue::CSS_PX:
    case CSSPrimitiveValue::CSS_CM:
    case CSSPrimitiveValue::CSS_MM:
    case CSSPrimitiveValue::CSS_IN:
    case CSSPrimitiveValue::CSS_PT:
    case CSSPrimitiveValue::CSS_PC:
    case CSSPrimitiveValue::CSS_VW:
    case CSSPrimitiveValue::CSS_VH:
    case CSSPrimitiveValue::CSS_VMIN:
    case CSSPrimitiveValue::CSS_VMAX:
        b = (unitflags & FLength);
        break;
    case CSSPrimitiveValue::CSS_MS:
    case CSSPrimitiveValue::CSS_S:
        b = (unitflags & FTime);
        break;
    case CSSPrimitiveValue::CSS_DEG:
    case CSSPrimitiveValue::CSS_RAD:
    case CSSPrimitiveValue::CSS_GRAD:
    case CSSPrimitiveValue::CSS_TURN:
        b = (unitflags & FAngle);
        break;
    case CSSPrimitiveValue::CSS_DPPX:
    case CSSPrimitiveValue::CSS_DPI:
    case CSSPrimitiveValue::CSS_DPCM:
        b = (unitflags & FResolution);
        break;
    case CSSPrimitiveValue::CSS_HZ:
    case CSSPrimitiveValue::CSS_KHZ:
    case CSSPrimitiveValue::CSS_DIMENSION:
    default:
        break;
    }
    if (b && unitflags & FNonNeg && value->fValue < 0)
        b = false;
    return b;
}

inline PassRefPtrWillBeRawPtr<CSSPrimitiveValue> CSSPropertyParser::createPrimitiveNumericValue(CSSParserValue* value)
{
    if (m_parsedCalculation) {
        ASSERT(isCalculation(value));
        return CSSPrimitiveValue::create(m_parsedCalculation.release());
    }

    ASSERT((value->unit >= CSSPrimitiveValue::CSS_NUMBER && value->unit <= CSSPrimitiveValue::CSS_KHZ)
        || (value->unit >= CSSPrimitiveValue::CSS_TURN && value->unit <= CSSPrimitiveValue::CSS_CHS)
        || (value->unit >= CSSPrimitiveValue::CSS_VW && value->unit <= CSSPrimitiveValue::CSS_VMAX)
        || (value->unit >= CSSPrimitiveValue::CSS_DPPX && value->unit <= CSSPrimitiveValue::CSS_DPCM));
    return cssValuePool().createValue(value->fValue, static_cast<CSSPrimitiveValue::UnitTypes>(value->unit));
}

inline PassRefPtrWillBeRawPtr<CSSPrimitiveValue> CSSPropertyParser::createPrimitiveStringValue(CSSParserValue* value)
{
    ASSERT(value->unit == CSSPrimitiveValue::CSS_STRING || value->unit == CSSPrimitiveValue::CSS_IDENT);
    return cssValuePool().createValue(value->string, CSSPrimitiveValue::CSS_STRING);
}

static inline bool isComma(CSSParserValue* value)
{
    return value && value->unit == CSSParserValue::Operator && value->iValue == ',';
}

static inline bool isForwardSlashOperator(CSSParserValue* value)
{
    ASSERT(value);
    return value->unit == CSSParserValue::Operator && value->iValue == '/';
}

static bool isGeneratedImageValue(CSSParserValue* val)
{
    if (val->unit != CSSParserValue::Function)
        return false;

    return equalIgnoringCase(val->function->name, "-webkit-gradient(")
        || equalIgnoringCase(val->function->name, "-webkit-linear-gradient(")
        || equalIgnoringCase(val->function->name, "linear-gradient(")
        || equalIgnoringCase(val->function->name, "-webkit-repeating-linear-gradient(")
        || equalIgnoringCase(val->function->name, "repeating-linear-gradient(")
        || equalIgnoringCase(val->function->name, "-webkit-radial-gradient(")
        || equalIgnoringCase(val->function->name, "radial-gradient(")
        || equalIgnoringCase(val->function->name, "-webkit-repeating-radial-gradient(")
        || equalIgnoringCase(val->function->name, "repeating-radial-gradient(")
        || equalIgnoringCase(val->function->name, "-webkit-canvas(")
        || equalIgnoringCase(val->function->name, "-webkit-cross-fade(");
}

bool CSSPropertyParser::validWidthOrHeight(CSSParserValue* value)
{
    int id = value->id;
    if (id == CSSValueIntrinsic || id == CSSValueMinIntrinsic || id == CSSValueWebkitMinContent || id == CSSValueWebkitMaxContent || id == CSSValueWebkitFillAvailable || id == CSSValueWebkitFitContent)
        return true;
    return !id && validUnit(value, FLength | FPercent | FNonNeg);
}

inline PassRefPtrWillBeRawPtr<CSSPrimitiveValue> CSSPropertyParser::parseValidPrimitive(CSSValueID identifier, CSSParserValue* value)
{
    if (identifier)
        return cssValuePool().createIdentifierValue(identifier);
    if (value->unit == CSSPrimitiveValue::CSS_STRING)
        return createPrimitiveStringValue(value);
    if (value->unit >= CSSPrimitiveValue::CSS_NUMBER && value->unit <= CSSPrimitiveValue::CSS_KHZ)
        return createPrimitiveNumericValue(value);
    if (value->unit >= CSSPrimitiveValue::CSS_TURN && value->unit <= CSSPrimitiveValue::CSS_CHS)
        return createPrimitiveNumericValue(value);
    if (value->unit >= CSSPrimitiveValue::CSS_VW && value->unit <= CSSPrimitiveValue::CSS_VMAX)
        return createPrimitiveNumericValue(value);
    if (value->unit >= CSSPrimitiveValue::CSS_DPPX && value->unit <= CSSPrimitiveValue::CSS_DPCM)
        return createPrimitiveNumericValue(value);
    if (value->unit >= CSSParserValue::Q_EMS)
        return CSSPrimitiveValue::createAllowingMarginQuirk(value->fValue, CSSPrimitiveValue::CSS_EMS);
    if (isCalculation(value))
        return CSSPrimitiveValue::create(m_parsedCalculation.release());

    return nullptr;
}

void CSSPropertyParser::addExpandedPropertyForValue(CSSPropertyID propId, PassRefPtrWillBeRawPtr<CSSValue> prpValue, bool important)
{
    const StylePropertyShorthand& shorthand = shorthandForProperty(propId);
    unsigned shorthandLength = shorthand.length();
    if (!shorthandLength) {
        addProperty(propId, prpValue, important);
        return;
    }

    RefPtrWillBeRawPtr<CSSValue> value = prpValue;
    ShorthandScope scope(this, propId);
    const CSSPropertyID* longhands = shorthand.properties();
    for (unsigned i = 0; i < shorthandLength; ++i)
        addProperty(longhands[i], value, important);
}

void BisonCSSParser::setCurrentProperty(CSSPropertyID propId)
{
    m_id = propId;
}

bool BisonCSSParser::parseValue(CSSPropertyID propId, bool important)
{
    CSSPropertyParser parser(m_valueList, m_context, m_inViewport, m_important, m_parsedProperties, m_hasFontFaceOnlyValues);
    return parser.parseValue(propId, important);
}

bool CSSPropertyParser::parseValue(CSSPropertyID propId, bool important)
{
    if (!isInternalPropertyAndValueParsingEnabledForMode(m_context.mode()) && isInternalProperty(propId))
        return false;

    // We don't count the UA style sheet in our statistics.
    if (m_context.useCounter())
        m_context.useCounter()->count(m_context, propId);

    if (!m_valueList)
        return false;

    CSSParserValue* value = m_valueList->current();

    if (!value)
        return false;

    if (inViewport()) {
        // Allow @viewport rules from UA stylesheets even if the feature is disabled.
        if (!RuntimeEnabledFeatures::cssViewportEnabled() && !isUASheetBehavior(m_context.mode()))
            return false;

        return parseViewportProperty(propId, important);
    }

    // Note: m_parsedCalculation is used to pass the calc value to validUnit and then cleared at the end of this function.
    // FIXME: This is to avoid having to pass parsedCalc to all validUnit callers.
    ASSERT(!m_parsedCalculation);

    CSSValueID id = value->id;

    int num = inShorthand() ? 1 : m_valueList->size();

    if (id == CSSValueInherit) {
        if (num != 1)
            return false;
        addExpandedPropertyForValue(propId, cssValuePool().createInheritedValue(), important);
        return true;
    }
    else if (id == CSSValueInitial) {
        if (num != 1)
            return false;
        addExpandedPropertyForValue(propId, cssValuePool().createExplicitInitialValue(), important);
        return true;
    }

    if (isKeywordPropertyID(propId)) {
        if (!isValidKeywordPropertyAndValue(propId, id, m_context))
            return false;
        if (m_valueList->next() && !inShorthand())
            return false;
        addProperty(propId, cssValuePool().createIdentifierValue(id), important);
        return true;
    }

    bool validPrimitive = false;
    RefPtrWillBeRawPtr<CSSValue> parsedValue;

    switch (propId) {
    case CSSPropertySize:                 // <length>{1,2} | auto | [ <page-size> || [ portrait | landscape] ]
        return parseSize(propId, important);

    case CSSPropertyQuotes:               // [<string> <string>]+ | none | inherit
        if (id)
            validPrimitive = true;
        else
            return parseQuotes(propId, important);
        break;
    case CSSPropertyUnicodeBidi: // normal | embed | bidi-override | isolate | isolate-override | plaintext | inherit
        if (id == CSSValueNormal
            || id == CSSValueEmbed
            || id == CSSValueBidiOverride
            || id == CSSValueWebkitIsolate
            || id == CSSValueWebkitIsolateOverride
            || id == CSSValueWebkitPlaintext)
            validPrimitive = true;
        break;

    case CSSPropertyContent:              // [ <string> | <uri> | <counter> | attr(X) | open-quote |
        // close-quote | no-open-quote | no-close-quote ]+ | inherit
        return parseContent(propId, important);

    case CSSPropertyClip:                 // <shape> | auto | inherit
        if (id == CSSValueAuto)
            validPrimitive = true;
        else if (value->unit == CSSParserValue::Function)
            return parseClipShape(propId, important);
        break;

    /* Start of supported CSS properties with validation. This is needed for parseShorthand to work
     * correctly and allows optimization in WebCore::applyRule(..)
     */
    case CSSPropertyOverflow: {
        ShorthandScope scope(this, propId);
        if (num != 1 || !parseValue(CSSPropertyOverflowY, important))
            return false;

        RefPtrWillBeRawPtr<CSSValue> overflowXValue;

        // FIXME: -webkit-paged-x or -webkit-paged-y only apply to overflow-y. If this value has been
        // set using the shorthand, then for now overflow-x will default to auto, but once we implement
        // pagination controls, it should default to hidden. If the overflow-y value is anything but
        // paged-x or paged-y, then overflow-x and overflow-y should have the same value.
        if (id == CSSValueWebkitPagedX || id == CSSValueWebkitPagedY)
            overflowXValue = cssValuePool().createIdentifierValue(CSSValueAuto);
        else
            overflowXValue = m_parsedProperties.last().value();
        addProperty(CSSPropertyOverflowX, overflowXValue.release(), important);
        return true;
    }

    case CSSPropertyTextAlign:
        // left | right | center | justify | -webkit-left | -webkit-right | -webkit-center | -webkit-match-parent
        // | start | end | <string> | inherit | -webkit-auto (converted to start)
        if ((id >= CSSValueWebkitAuto && id <= CSSValueWebkitMatchParent) || id == CSSValueStart || id == CSSValueEnd
            || value->unit == CSSPrimitiveValue::CSS_STRING)
            validPrimitive = true;
        break;

    case CSSPropertyFontWeight:  { // normal | bold | bolder | lighter | 100 | 200 | 300 | 400 | 500 | 600 | 700 | 800 | 900 | inherit
        if (m_valueList->size() != 1)
            return false;
        return parseFontWeight(important);
    }
    case CSSPropertyBorderSpacing: {
        if (num == 1) {
            ShorthandScope scope(this, CSSPropertyBorderSpacing);
            if (!parseValue(CSSPropertyWebkitBorderHorizontalSpacing, important))
                return false;
            CSSValue* value = m_parsedProperties.last().value();
            addProperty(CSSPropertyWebkitBorderVerticalSpacing, value, important);
            return true;
        }
        else if (num == 2) {
            ShorthandScope scope(this, CSSPropertyBorderSpacing);
            if (!parseValue(CSSPropertyWebkitBorderHorizontalSpacing, important) || !parseValue(CSSPropertyWebkitBorderVerticalSpacing, important))
                return false;
            return true;
        }
        return false;
    }
    case CSSPropertyWebkitBorderHorizontalSpacing:
    case CSSPropertyWebkitBorderVerticalSpacing:
        validPrimitive = validUnit(value, FLength | FNonNeg);
        break;
    case CSSPropertyOutlineColor:        // <color> | invert | inherit
        // Outline color has "invert" as additional keyword.
        // Also, we want to allow the special focus color even in HTML Standard parsing mode.
        if (id == CSSValueInvert || id == CSSValueWebkitFocusRingColor) {
            validPrimitive = true;
            break;
        }
        /* nobreak */
    case CSSPropertyBackgroundColor: // <color> | inherit
    case CSSPropertyBorderTopColor: // <color> | inherit
    case CSSPropertyBorderRightColor:
    case CSSPropertyBorderBottomColor:
    case CSSPropertyBorderLeftColor:
    case CSSPropertyWebkitBorderStartColor:
    case CSSPropertyWebkitBorderEndColor:
    case CSSPropertyWebkitBorderBeforeColor:
    case CSSPropertyWebkitBorderAfterColor:
    case CSSPropertyColor: // <color> | inherit
    case CSSPropertyTextDecorationColor: // CSS3 text decoration colors
    case CSSPropertyTextLineThroughColor:
    case CSSPropertyTextUnderlineColor:
    case CSSPropertyTextOverlineColor:
    case CSSPropertyWebkitColumnRuleColor:
    case CSSPropertyWebkitTextEmphasisColor:
    case CSSPropertyWebkitTextFillColor:
    case CSSPropertyWebkitTextStrokeColor:
        if (propId == CSSPropertyTextDecorationColor
            && !RuntimeEnabledFeatures::css3TextDecorationsEnabled())
            return false;

        if ((id >= CSSValueAqua && id <= CSSValueWebkitText) || id == CSSValueMenu) {
            validPrimitive = isValueAllowedInMode(id, m_context.mode());
        } else {
            parsedValue = parseColor();
            if (parsedValue)
                m_valueList->next();
        }
        break;

    case CSSPropertyCursor: {
        // Grammar defined by CSS3 UI and modified by CSS4 images:
        // [ [<image> [<x> <y>]?,]*
        // [ auto | crosshair | default | pointer | progress | move | e-resize | ne-resize |
        // nw-resize | n-resize | se-resize | sw-resize | s-resize | w-resize | ew-resize |
        // ns-resize | nesw-resize | nwse-resize | col-resize | row-resize | text | wait | help |
        // vertical-text | cell | context-menu | alias | copy | no-drop | not-allowed | -webkit-zoom-in
        // -webkit-zoom-out | all-scroll | -webkit-grab | -webkit-grabbing ] ] | inherit
        RefPtrWillBeRawPtr<CSSValueList> list;
        while (value) {
            RefPtrWillBeRawPtr<CSSValue> image = nullptr;
            if (value->unit == CSSPrimitiveValue::CSS_URI) {
                String uri = value->string;
                if (!uri.isNull())
                    image = CSSImageValue::create(completeURL(uri));
            } else if (value->unit == CSSParserValue::Function && equalIgnoringCase(value->function->name, "-webkit-image-set(")) {
                image = parseImageSet(m_valueList.get());
                if (!image)
                    break;
            } else
                break;

            Vector<int> coords;
            value = m_valueList->next();
            while (value && value->unit == CSSPrimitiveValue::CSS_NUMBER) {
                coords.append(int(value->fValue));
                value = m_valueList->next();
            }
            bool hasHotSpot = false;
            IntPoint hotSpot(-1, -1);
            int nrcoords = coords.size();
            if (nrcoords > 0 && nrcoords != 2)
                return false;
            if (nrcoords == 2) {
                hasHotSpot = true;
                hotSpot = IntPoint(coords[0], coords[1]);
            }

            if (!list)
                list = CSSValueList::createCommaSeparated();

            if (image)
                list->append(CSSCursorImageValue::create(image, hasHotSpot, hotSpot));

            if (!value || !(value->unit == CSSParserValue::Operator && value->iValue == ','))
                return false;
            value = m_valueList->next(); // comma
        }
        if (list) {
            if (!value)
                return false;
            if (inQuirksMode() && value->id == CSSValueHand) // MSIE 5 compatibility :/
                list->append(cssValuePool().createIdentifierValue(CSSValuePointer));
            else if ((value->id >= CSSValueAuto && value->id <= CSSValueWebkitGrabbing) || value->id == CSSValueCopy || value->id == CSSValueNone)
                list->append(cssValuePool().createIdentifierValue(value->id));
            m_valueList->next();
            parsedValue = list.release();
            break;
        } else if (value) {
            id = value->id;
            if (inQuirksMode() && value->id == CSSValueHand) { // MSIE 5 compatibility :/
                id = CSSValuePointer;
                validPrimitive = true;
            } else if ((value->id >= CSSValueAuto && value->id <= CSSValueWebkitGrabbing) || value->id == CSSValueCopy || value->id == CSSValueNone)
                validPrimitive = true;
        } else {
            ASSERT_NOT_REACHED();
            return false;
        }
        break;
    }

    case CSSPropertyBackgroundBlendMode:
    case CSSPropertyBackgroundAttachment:
    case CSSPropertyBackgroundClip:
    case CSSPropertyWebkitBackgroundClip:
    case CSSPropertyWebkitBackgroundComposite:
    case CSSPropertyBackgroundImage:
    case CSSPropertyBackgroundOrigin:
    case CSSPropertyMaskSourceType:
    case CSSPropertyWebkitBackgroundOrigin:
    case CSSPropertyBackgroundPosition:
    case CSSPropertyBackgroundPositionX:
    case CSSPropertyBackgroundPositionY:
    case CSSPropertyBackgroundSize:
    case CSSPropertyWebkitBackgroundSize:
    case CSSPropertyBackgroundRepeat:
    case CSSPropertyBackgroundRepeatX:
    case CSSPropertyBackgroundRepeatY:
    case CSSPropertyWebkitMaskClip:
    case CSSPropertyWebkitMaskComposite:
    case CSSPropertyWebkitMaskImage:
    case CSSPropertyWebkitMaskOrigin:
    case CSSPropertyWebkitMaskPosition:
    case CSSPropertyWebkitMaskPositionX:
    case CSSPropertyWebkitMaskPositionY:
    case CSSPropertyWebkitMaskSize:
    case CSSPropertyWebkitMaskRepeat:
    case CSSPropertyWebkitMaskRepeatX:
    case CSSPropertyWebkitMaskRepeatY:
    {
        RefPtrWillBeRawPtr<CSSValue> val1;
        RefPtrWillBeRawPtr<CSSValue> val2;
        CSSPropertyID propId1, propId2;
        bool result = false;
        if (parseFillProperty(propId, propId1, propId2, val1, val2)) {
            OwnPtr<ShorthandScope> shorthandScope;
            if (propId == CSSPropertyBackgroundPosition ||
                propId == CSSPropertyBackgroundRepeat ||
                propId == CSSPropertyWebkitMaskPosition ||
                propId == CSSPropertyWebkitMaskRepeat) {
                shorthandScope = adoptPtr(new ShorthandScope(this, propId));
            }
            addProperty(propId1, val1.release(), important);
            if (val2)
                addProperty(propId2, val2.release(), important);
            result = true;
        }
        m_implicitShorthand = false;
        return result;
    }
    case CSSPropertyObjectPosition:
        return RuntimeEnabledFeatures::objectFitPositionEnabled() && parseObjectPosition(important);
    case CSSPropertyListStyleImage:     // <uri> | none | inherit
    case CSSPropertyBorderImageSource:
    case CSSPropertyWebkitMaskBoxImageSource:
        if (id == CSSValueNone) {
            parsedValue = cssValuePool().createIdentifierValue(CSSValueNone);
            m_valueList->next();
        } else if (value->unit == CSSPrimitiveValue::CSS_URI) {
            parsedValue = CSSImageValue::create(completeURL(value->string));
            m_valueList->next();
        } else if (isGeneratedImageValue(value)) {
            if (parseGeneratedImage(m_valueList.get(), parsedValue))
                m_valueList->next();
            else
                return false;
        }
        else if (value->unit == CSSParserValue::Function && equalIgnoringCase(value->function->name, "-webkit-image-set(")) {
            parsedValue = parseImageSet(m_valueList.get());
            if (!parsedValue)
                return false;
            m_valueList->next();
        }
        break;

    case CSSPropertyWebkitTextStrokeWidth:
    case CSSPropertyOutlineWidth:        // <border-width> | inherit
    case CSSPropertyBorderTopWidth:     //// <border-width> | inherit
    case CSSPropertyBorderRightWidth:   //   Which is defined as
    case CSSPropertyBorderBottomWidth:  //   thin | medium | thick | <length>
    case CSSPropertyBorderLeftWidth:
    case CSSPropertyWebkitBorderStartWidth:
    case CSSPropertyWebkitBorderEndWidth:
    case CSSPropertyWebkitBorderBeforeWidth:
    case CSSPropertyWebkitBorderAfterWidth:
    case CSSPropertyWebkitColumnRuleWidth:
        if (id == CSSValueThin || id == CSSValueMedium || id == CSSValueThick)
            validPrimitive = true;
        else
            validPrimitive = validUnit(value, FLength | FNonNeg);
        break;

    case CSSPropertyLetterSpacing:       // normal | <length> | inherit
    case CSSPropertyWordSpacing:         // normal | <length> | inherit
        if (id == CSSValueNormal)
            validPrimitive = true;
        else
            validPrimitive = validUnit(value, FLength);
        break;

    case CSSPropertyTextIndent:
        parsedValue = parseTextIndent();
        break;

    case CSSPropertyPaddingTop:          //// <padding-width> | inherit
    case CSSPropertyPaddingRight:        //   Which is defined as
    case CSSPropertyPaddingBottom:       //   <length> | <percentage>
    case CSSPropertyPaddingLeft:         ////
    case CSSPropertyWebkitPaddingStart:
    case CSSPropertyWebkitPaddingEnd:
    case CSSPropertyWebkitPaddingBefore:
    case CSSPropertyWebkitPaddingAfter:
        validPrimitive = (!id && validUnit(value, FLength | FPercent | FNonNeg));
        break;

    case CSSPropertyMaxWidth:
    case CSSPropertyWebkitMaxLogicalWidth:
    case CSSPropertyMaxHeight:
    case CSSPropertyWebkitMaxLogicalHeight:
        validPrimitive = (id == CSSValueNone || validWidthOrHeight(value));
        break;

    case CSSPropertyMinWidth:
    case CSSPropertyWebkitMinLogicalWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyWebkitMinLogicalHeight:
        validPrimitive = validWidthOrHeight(value);
        break;

    case CSSPropertyWidth:
    case CSSPropertyWebkitLogicalWidth:
    case CSSPropertyHeight:
    case CSSPropertyWebkitLogicalHeight:
        validPrimitive = (id == CSSValueAuto || validWidthOrHeight(value));
        break;

    case CSSPropertyFontSize:
        return parseFontSize(important);

    case CSSPropertyFontVariant:         // normal | small-caps | inherit
        return parseFontVariant(important);

    case CSSPropertyVerticalAlign:
        // baseline | sub | super | top | text-top | middle | bottom | text-bottom |
        // <percentage> | <length> | inherit

        if (id >= CSSValueBaseline && id <= CSSValueWebkitBaselineMiddle)
            validPrimitive = true;
        else
            validPrimitive = (!id && validUnit(value, FLength | FPercent));
        break;

    case CSSPropertyBottom:               // <length> | <percentage> | auto | inherit
    case CSSPropertyLeft:                 // <length> | <percentage> | auto | inherit
    case CSSPropertyRight:                // <length> | <percentage> | auto | inherit
    case CSSPropertyTop:                  // <length> | <percentage> | auto | inherit
    case CSSPropertyMarginTop:           //// <margin-width> | inherit
    case CSSPropertyMarginRight:         //   Which is defined as
    case CSSPropertyMarginBottom:        //   <length> | <percentage> | auto | inherit
    case CSSPropertyMarginLeft:          ////
    case CSSPropertyWebkitMarginStart:
    case CSSPropertyWebkitMarginEnd:
    case CSSPropertyWebkitMarginBefore:
    case CSSPropertyWebkitMarginAfter:
        if (id == CSSValueAuto)
            validPrimitive = true;
        else
            validPrimitive = (!id && validUnit(value, FLength | FPercent));
        break;

    case CSSPropertyOrphans: // <integer> | inherit | auto (We've added support for auto for backwards compatibility)
    case CSSPropertyWidows: // <integer> | inherit | auto (Ditto)
        if (id == CSSValueAuto)
            validPrimitive = true;
        else
            validPrimitive = (!id && validUnit(value, FPositiveInteger, HTMLQuirksMode));
        break;

    case CSSPropertyZIndex: // auto | <integer> | inherit
        if (id == CSSValueAuto)
            validPrimitive = true;
        else
            validPrimitive = (!id && validUnit(value, FInteger, HTMLQuirksMode));
        break;

    case CSSPropertyLineHeight:
        return parseLineHeight(important);
    case CSSPropertyCounterIncrement:    // [ <identifier> <integer>? ]+ | none | inherit
        if (id != CSSValueNone)
            return parseCounter(propId, 1, important);
        validPrimitive = true;
        break;
    case CSSPropertyCounterReset:        // [ <identifier> <integer>? ]+ | none | inherit
        if (id != CSSValueNone)
            return parseCounter(propId, 0, important);
        validPrimitive = true;
        break;
    case CSSPropertyFontFamily:
        // [[ <family-name> | <generic-family> ],]* [<family-name> | <generic-family>] | inherit
    {
        parsedValue = parseFontFamily();
        break;
    }

    case CSSPropertyTextDecoration:
        // Fall through 'text-decoration-line' parsing if CSS 3 Text Decoration
        // is disabled to match CSS 2.1 rules for parsing 'text-decoration'.
        if (RuntimeEnabledFeatures::css3TextDecorationsEnabled()) {
            // [ <text-decoration-line> || <text-decoration-style> || <text-decoration-color> ] | inherit
            return parseShorthand(CSSPropertyTextDecoration, textDecorationShorthand(), important);
        }
    case CSSPropertyWebkitTextDecorationsInEffect:
    case CSSPropertyTextDecorationLine:
        // none | [ underline || overline || line-through || blink ] | inherit
        return parseTextDecoration(propId, important);

    case CSSPropertyTextDecorationStyle:
        // solid | double | dotted | dashed | wavy
        if (RuntimeEnabledFeatures::css3TextDecorationsEnabled()
            && (id == CSSValueSolid || id == CSSValueDouble || id == CSSValueDotted || id == CSSValueDashed || id == CSSValueWavy))
            validPrimitive = true;
        break;

    case CSSPropertyTextUnderlinePosition:
        // auto | under | inherit
        if (RuntimeEnabledFeatures::css3TextDecorationsEnabled())
            return parseTextUnderlinePosition(important);
        return false;

    case CSSPropertyZoom:          // normal | reset | document | <number> | <percentage> | inherit
        if (id == CSSValueNormal || id == CSSValueReset || id == CSSValueDocument)
            validPrimitive = true;
        else
            validPrimitive = (!id && validUnit(value, FNumber | FPercent | FNonNeg, HTMLStandardMode));
        break;

    case CSSPropertySrc: // Only used within @font-face and @-webkit-filter, so cannot use inherit | initial or be !important. This is a list of urls or local references.
        return parseFontFaceSrc();

    case CSSPropertyUnicodeRange:
        return parseFontFaceUnicodeRange();

    /* CSS3 properties */

    case CSSPropertyBorderImage:
    case CSSPropertyWebkitMaskBoxImage:
        return parseBorderImageShorthand(propId, important);
    case CSSPropertyWebkitBorderImage: {
        if (RefPtrWillBeRawPtr<CSSValue> result = parseBorderImage(propId)) {
            addProperty(propId, result, important);
            return true;
        }
        return false;
    }

    case CSSPropertyBorderImageOutset:
    case CSSPropertyWebkitMaskBoxImageOutset: {
        RefPtrWillBeRawPtr<CSSPrimitiveValue> result;
        if (parseBorderImageOutset(result)) {
            addProperty(propId, result, important);
            return true;
        }
        break;
    }
    case CSSPropertyBorderImageRepeat:
    case CSSPropertyWebkitMaskBoxImageRepeat: {
        RefPtrWillBeRawPtr<CSSValue> result;
        if (parseBorderImageRepeat(result)) {
            addProperty(propId, result, important);
            return true;
        }
        break;
    }
    case CSSPropertyBorderImageSlice:
    case CSSPropertyWebkitMaskBoxImageSlice: {
        RefPtrWillBeRawPtr<CSSBorderImageSliceValue> result;
        if (parseBorderImageSlice(propId, result)) {
            addProperty(propId, result, important);
            return true;
        }
        break;
    }
    case CSSPropertyBorderImageWidth:
    case CSSPropertyWebkitMaskBoxImageWidth: {
        RefPtrWillBeRawPtr<CSSPrimitiveValue> result;
        if (parseBorderImageWidth(result)) {
            addProperty(propId, result, important);
            return true;
        }
        break;
    }
    case CSSPropertyBorderTopRightRadius:
    case CSSPropertyBorderTopLeftRadius:
    case CSSPropertyBorderBottomLeftRadius:
    case CSSPropertyBorderBottomRightRadius: {
        if (num != 1 && num != 2)
            return false;
        validPrimitive = validUnit(value, FLength | FPercent | FNonNeg);
        if (!validPrimitive)
            return false;
        RefPtrWillBeRawPtr<CSSPrimitiveValue> parsedValue1 = createPrimitiveNumericValue(value);
        RefPtrWillBeRawPtr<CSSPrimitiveValue> parsedValue2;
        if (num == 2) {
            value = m_valueList->next();
            validPrimitive = validUnit(value, FLength | FPercent | FNonNeg);
            if (!validPrimitive)
                return false;
            parsedValue2 = createPrimitiveNumericValue(value);
        } else
            parsedValue2 = parsedValue1;

        addProperty(propId, createPrimitiveValuePair(parsedValue1.release(), parsedValue2.release()), important);
        return true;
    }
    case CSSPropertyTabSize:
        validPrimitive = validUnit(value, FInteger | FNonNeg);
        break;
    case CSSPropertyWebkitAspectRatio:
        return parseAspectRatio(important);
    case CSSPropertyBorderRadius:
    case CSSPropertyWebkitBorderRadius:
        return parseBorderRadius(propId, important);
    case CSSPropertyOutlineOffset:
        validPrimitive = validUnit(value, FLength);
        break;
    case CSSPropertyTextShadow: // CSS2 property, dropped in CSS2.1, back in CSS3, so treat as CSS3
    case CSSPropertyBoxShadow:
    case CSSPropertyWebkitBoxShadow:
        if (id == CSSValueNone)
            validPrimitive = true;
        else {
            RefPtrWillBeRawPtr<CSSValueList> shadowValueList = parseShadow(m_valueList.get(), propId);
            if (shadowValueList) {
                addProperty(propId, shadowValueList.release(), important);
                m_valueList->next();
                return true;
            }
            return false;
        }
        break;
    case CSSPropertyWebkitBoxReflect:
        if (id == CSSValueNone)
            validPrimitive = true;
        else
            return parseReflect(propId, important);
        break;
    case CSSPropertyOpacity:
        validPrimitive = validUnit(value, FNumber);
        break;
    case CSSPropertyWebkitBoxFlex:
        validPrimitive = validUnit(value, FNumber);
        break;
    case CSSPropertyWebkitBoxFlexGroup:
        validPrimitive = validUnit(value, FInteger | FNonNeg, HTMLStandardMode);
        break;
    case CSSPropertyWebkitBoxOrdinalGroup:
        validPrimitive = validUnit(value, FInteger | FNonNeg, HTMLStandardMode) && value->fValue;
        break;
    case CSSPropertyWebkitFilter:
        if (id == CSSValueNone)
            validPrimitive = true;
        else {
            RefPtrWillBeRawPtr<CSSValue> val = parseFilter();
            if (val) {
                addProperty(propId, val, important);
                return true;
            }
            return false;
        }
        break;
    case CSSPropertyFlex: {
        ShorthandScope scope(this, propId);
        if (id == CSSValueNone) {
            addProperty(CSSPropertyFlexGrow, cssValuePool().createValue(0, CSSPrimitiveValue::CSS_NUMBER), important);
            addProperty(CSSPropertyFlexShrink, cssValuePool().createValue(0, CSSPrimitiveValue::CSS_NUMBER), important);
            addProperty(CSSPropertyFlexBasis, cssValuePool().createIdentifierValue(CSSValueAuto), important);
            return true;
        }
        return parseFlex(m_valueList.get(), important);
    }
    case CSSPropertyFlexBasis:
        // FIXME: Support intrinsic dimensions too.
        if (id == CSSValueAuto)
            validPrimitive = true;
        else
            validPrimitive = (!id && validUnit(value, FLength | FPercent | FNonNeg));
        break;
    case CSSPropertyFlexGrow:
    case CSSPropertyFlexShrink:
        validPrimitive = validUnit(value, FNumber | FNonNeg);
        break;
    case CSSPropertyOrder:
        validPrimitive = validUnit(value, FInteger, HTMLStandardMode);
        break;
    case CSSPropertyInternalMarqueeIncrement:
        if (id == CSSValueSmall || id == CSSValueLarge || id == CSSValueMedium)
            validPrimitive = true;
        else
            validPrimitive = validUnit(value, FLength | FPercent);
        break;
    case CSSPropertyInternalMarqueeRepetition:
        if (id == CSSValueInfinite)
            validPrimitive = true;
        else
            validPrimitive = validUnit(value, FInteger | FNonNeg);
        break;
    case CSSPropertyInternalMarqueeSpeed:
        if (id == CSSValueNormal || id == CSSValueSlow || id == CSSValueFast)
            validPrimitive = true;
        else
            validPrimitive = validUnit(value, FTime | FInteger | FNonNeg);
        break;
    case CSSPropertyWebkitTransform:
        if (id == CSSValueNone)
            validPrimitive = true;
        else {
            RefPtrWillBeRawPtr<CSSValue> transformValue = parseTransform();
            if (transformValue) {
                addProperty(propId, transformValue.release(), important);
                return true;
            }
            return false;
        }
        break;
    case CSSPropertyWebkitTransformOrigin:
    case CSSPropertyWebkitTransformOriginX:
    case CSSPropertyWebkitTransformOriginY:
    case CSSPropertyWebkitTransformOriginZ: {
        RefPtrWillBeRawPtr<CSSValue> val1;
        RefPtrWillBeRawPtr<CSSValue> val2;
        RefPtrWillBeRawPtr<CSSValue> val3;
        CSSPropertyID propId1, propId2, propId3;
        if (parseTransformOrigin(propId, propId1, propId2, propId3, val1, val2, val3)) {
            addProperty(propId1, val1.release(), important);
            if (val2)
                addProperty(propId2, val2.release(), important);
            if (val3)
                addProperty(propId3, val3.release(), important);
            return true;
        }
        return false;
    }
    case CSSPropertyWebkitPerspective:
        if (id == CSSValueNone)
            validPrimitive = true;
        else {
            // Accepting valueless numbers is a quirk of the -webkit prefixed version of the property.
            if (validUnit(value, FNumber | FLength | FNonNeg)) {
                RefPtrWillBeRawPtr<CSSValue> val = createPrimitiveNumericValue(value);
                if (val) {
                    addProperty(propId, val.release(), important);
                    return true;
                }
                return false;
            }
        }
        break;
    case CSSPropertyWebkitPerspectiveOrigin:
    case CSSPropertyWebkitPerspectiveOriginX:
    case CSSPropertyWebkitPerspectiveOriginY: {
        RefPtrWillBeRawPtr<CSSValue> val1;
        RefPtrWillBeRawPtr<CSSValue> val2;
        CSSPropertyID propId1, propId2;
        if (parsePerspectiveOrigin(propId, propId1, propId2, val1, val2)) {
            addProperty(propId1, val1.release(), important);
            if (val2)
                addProperty(propId2, val2.release(), important);
            return true;
        }
        return false;
    }
    case CSSPropertyAnimationDelay:
    case CSSPropertyAnimationDirection:
    case CSSPropertyAnimationDuration:
    case CSSPropertyAnimationFillMode:
    case CSSPropertyAnimationName:
    case CSSPropertyAnimationPlayState:
    case CSSPropertyAnimationIterationCount:
    case CSSPropertyAnimationTimingFunction:
        if (!RuntimeEnabledFeatures::cssAnimationUnprefixedEnabled())
            break;
    case CSSPropertyWebkitAnimationDelay:
    case CSSPropertyWebkitAnimationDirection:
    case CSSPropertyWebkitAnimationDuration:
    case CSSPropertyWebkitAnimationFillMode:
    case CSSPropertyWebkitAnimationName:
    case CSSPropertyWebkitAnimationPlayState:
    case CSSPropertyWebkitAnimationIterationCount:
    case CSSPropertyWebkitAnimationTimingFunction:
    case CSSPropertyTransitionDelay:
    case CSSPropertyTransitionDuration:
    case CSSPropertyTransitionTimingFunction:
    case CSSPropertyTransitionProperty:
    case CSSPropertyWebkitTransitionDelay:
    case CSSPropertyWebkitTransitionDuration:
    case CSSPropertyWebkitTransitionTimingFunction:
    case CSSPropertyWebkitTransitionProperty: {
        RefPtrWillBeRawPtr<CSSValue> val;
        AnimationParseContext context;
        if (parseAnimationProperty(propId, val, context)) {
            addPropertyWithPrefixingVariant(propId, val.release(), important);
            return true;
        }
        return false;
    }

    case CSSPropertyJustifySelf:
        if (!RuntimeEnabledFeatures::cssGridLayoutEnabled())
            return false;

        return parseItemPositionOverflowPosition(propId, important);
    case CSSPropertyGridAutoColumns:
    case CSSPropertyGridAutoRows:
        if (!RuntimeEnabledFeatures::cssGridLayoutEnabled())
            return false;
        parsedValue = parseGridTrackSize(*m_valueList);
        break;

    case CSSPropertyGridTemplateColumns:
    case CSSPropertyGridTemplateRows:
        if (!RuntimeEnabledFeatures::cssGridLayoutEnabled())
            return false;
        return parseGridTrackList(propId, important);

    case CSSPropertyGridColumnEnd:
    case CSSPropertyGridColumnStart:
    case CSSPropertyGridRowEnd:
    case CSSPropertyGridRowStart:
        if (!RuntimeEnabledFeatures::cssGridLayoutEnabled())
            return false;
        parsedValue = parseGridPosition();
        break;

    case CSSPropertyGridColumn:
    case CSSPropertyGridRow:
        if (!RuntimeEnabledFeatures::cssGridLayoutEnabled())
            return false;
        return parseGridItemPositionShorthand(propId, important);

    case CSSPropertyGridArea:
        if (!RuntimeEnabledFeatures::cssGridLayoutEnabled())
            return false;
        return parseGridAreaShorthand(important);

    case CSSPropertyGridTemplateAreas:
        if (!RuntimeEnabledFeatures::cssGridLayoutEnabled())
            return false;
        parsedValue = parseGridTemplateAreas();
        break;

    case CSSPropertyWebkitMarginCollapse: {
        if (num == 1) {
            ShorthandScope scope(this, CSSPropertyWebkitMarginCollapse);
            if (!parseValue(webkitMarginCollapseShorthand().properties()[0], important))
                return false;
            CSSValue* value = m_parsedProperties.last().value();
            addProperty(webkitMarginCollapseShorthand().properties()[1], value, important);
            return true;
        }
        else if (num == 2) {
            ShorthandScope scope(this, CSSPropertyWebkitMarginCollapse);
            if (!parseValue(webkitMarginCollapseShorthand().properties()[0], important) || !parseValue(webkitMarginCollapseShorthand().properties()[1], important))
                return false;
            return true;
        }
        return false;
    }
    case CSSPropertyTextLineThroughWidth:
    case CSSPropertyTextOverlineWidth:
    case CSSPropertyTextUnderlineWidth:
        if (id == CSSValueAuto || id == CSSValueNormal || id == CSSValueThin ||
            id == CSSValueMedium || id == CSSValueThick)
            validPrimitive = true;
        else
            validPrimitive = !id && validUnit(value, FNumber | FLength | FPercent);
        break;
    case CSSPropertyWebkitColumnCount:
        parsedValue = parseColumnCount();
        break;
    case CSSPropertyWebkitColumnGap:         // normal | <length>
        if (id == CSSValueNormal)
            validPrimitive = true;
        else
            validPrimitive = validUnit(value, FLength | FNonNeg);
        break;
    case CSSPropertyWebkitColumnAxis:
        if (id == CSSValueHorizontal || id == CSSValueVertical || id == CSSValueAuto)
            validPrimitive = true;
        break;
    case CSSPropertyWebkitColumnProgression:
        if (id == CSSValueNormal || id == CSSValueReverse)
            validPrimitive = true;
        break;
    case CSSPropertyWebkitColumnSpan: // none | all | 1 (will be dropped in the unprefixed property)
        if (id == CSSValueAll || id == CSSValueNone)
            validPrimitive = true;
        else
            validPrimitive = validUnit(value, FNumber | FNonNeg) && value->fValue == 1;
        break;
    case CSSPropertyWebkitColumnWidth:         // auto | <length>
        parsedValue = parseColumnWidth();
        break;
    case CSSPropertyWillChange:
        if (!RuntimeEnabledFeatures::cssWillChangeEnabled())
            return false;
        return parseWillChange(important);
    // End of CSS3 properties

    // Apple specific properties.  These will never be standardized and are purely to
    // support custom WebKit-based Apple applications.
    case CSSPropertyWebkitLineClamp:
        // When specifying number of lines, don't allow 0 as a valid value
        // When specifying either type of unit, require non-negative integers
        validPrimitive = (!id && (value->unit == CSSPrimitiveValue::CSS_PERCENTAGE || value->fValue) && validUnit(value, FInteger | FPercent | FNonNeg, HTMLQuirksMode));
        break;

    case CSSPropertyWebkitFontSizeDelta:           // <length>
        validPrimitive = validUnit(value, FLength);
        break;

    case CSSPropertyWebkitHighlight:
        if (id == CSSValueNone || value->unit == CSSPrimitiveValue::CSS_STRING)
            validPrimitive = true;
        break;

    case CSSPropertyWebkitHyphenateCharacter:
        if (id == CSSValueAuto || value->unit == CSSPrimitiveValue::CSS_STRING)
            validPrimitive = true;
        break;

    case CSSPropertyWebkitLocale:
        if (id == CSSValueAuto || value->unit == CSSPrimitiveValue::CSS_STRING)
            validPrimitive = true;
        break;

    // End Apple-specific properties

    case CSSPropertyWebkitAppRegion:
        if (id >= CSSValueDrag && id <= CSSValueNoDrag)
            validPrimitive = true;
        break;

    case CSSPropertyWebkitTapHighlightColor:
        if ((id >= CSSValueAqua && id <= CSSValueWindowtext) || id == CSSValueMenu
            || (id >= CSSValueWebkitFocusRingColor && id < CSSValueWebkitText && inQuirksMode())) {
            validPrimitive = true;
        } else {
            parsedValue = parseColor();
            if (parsedValue)
                m_valueList->next();
        }
        break;

        /* shorthand properties */
    case CSSPropertyBackground: {
        // Position must come before color in this array because a plain old "0" is a legal color
        // in quirks mode but it's usually the X coordinate of a position.
        const CSSPropertyID properties[] = { CSSPropertyBackgroundImage, CSSPropertyBackgroundRepeat,
                                   CSSPropertyBackgroundAttachment, CSSPropertyBackgroundPosition, CSSPropertyBackgroundOrigin,
                                   CSSPropertyBackgroundClip, CSSPropertyBackgroundColor, CSSPropertyBackgroundSize };
        return parseFillShorthand(propId, properties, WTF_ARRAY_LENGTH(properties), important);
    }
    case CSSPropertyWebkitMask: {
        const CSSPropertyID properties[] = { CSSPropertyWebkitMaskImage, CSSPropertyWebkitMaskRepeat,
            CSSPropertyWebkitMaskPosition, CSSPropertyWebkitMaskOrigin, CSSPropertyWebkitMaskClip, CSSPropertyWebkitMaskSize };
        return parseFillShorthand(propId, properties, WTF_ARRAY_LENGTH(properties), important);
    }
    case CSSPropertyBorder:
        // [ 'border-width' || 'border-style' || <color> ] | inherit
    {
        if (parseShorthand(propId, parsingShorthandForProperty(CSSPropertyBorder), important)) {
            // The CSS3 Borders and Backgrounds specification says that border also resets border-image. It's as
            // though a value of none was specified for the image.
            addExpandedPropertyForValue(CSSPropertyBorderImage, cssValuePool().createImplicitInitialValue(), important);
            return true;
        }
        return false;
    }
    case CSSPropertyBorderTop:
        // [ 'border-top-width' || 'border-style' || <color> ] | inherit
        return parseShorthand(propId, borderTopShorthand(), important);
    case CSSPropertyBorderRight:
        // [ 'border-right-width' || 'border-style' || <color> ] | inherit
        return parseShorthand(propId, borderRightShorthand(), important);
    case CSSPropertyBorderBottom:
        // [ 'border-bottom-width' || 'border-style' || <color> ] | inherit
        return parseShorthand(propId, borderBottomShorthand(), important);
    case CSSPropertyBorderLeft:
        // [ 'border-left-width' || 'border-style' || <color> ] | inherit
        return parseShorthand(propId, borderLeftShorthand(), important);
    case CSSPropertyWebkitBorderStart:
        return parseShorthand(propId, webkitBorderStartShorthand(), important);
    case CSSPropertyWebkitBorderEnd:
        return parseShorthand(propId, webkitBorderEndShorthand(), important);
    case CSSPropertyWebkitBorderBefore:
        return parseShorthand(propId, webkitBorderBeforeShorthand(), important);
    case CSSPropertyWebkitBorderAfter:
        return parseShorthand(propId, webkitBorderAfterShorthand(), important);
    case CSSPropertyOutline:
        // [ 'outline-color' || 'outline-style' || 'outline-width' ] | inherit
        return parseShorthand(propId, outlineShorthand(), important);
    case CSSPropertyBorderColor:
        // <color>{1,4} | inherit
        return parse4Values(propId, borderColorShorthand().properties(), important);
    case CSSPropertyBorderWidth:
        // <border-width>{1,4} | inherit
        return parse4Values(propId, borderWidthShorthand().properties(), important);
    case CSSPropertyBorderStyle:
        // <border-style>{1,4} | inherit
        return parse4Values(propId, borderStyleShorthand().properties(), important);
    case CSSPropertyMargin:
        // <margin-width>{1,4} | inherit
        return parse4Values(propId, marginShorthand().properties(), important);
    case CSSPropertyPadding:
        // <padding-width>{1,4} | inherit
        return parse4Values(propId, paddingShorthand().properties(), important);
    case CSSPropertyFlexFlow:
        return parseShorthand(propId, flexFlowShorthand(), important);
    case CSSPropertyFont:
        // [ [ 'font-style' || 'font-variant' || 'font-weight' ]? 'font-size' [ / 'line-height' ]?
        // 'font-family' ] | caption | icon | menu | message-box | small-caption | status-bar | inherit
        if (id >= CSSValueCaption && id <= CSSValueStatusBar)
            validPrimitive = true;
        else
            return parseFont(important);
        break;
    case CSSPropertyListStyle:
        return parseShorthand(propId, listStyleShorthand(), important);
    case CSSPropertyWebkitColumns:
        return parseColumnsShorthand(important);
    case CSSPropertyWebkitColumnRule:
        return parseShorthand(propId, webkitColumnRuleShorthand(), important);
    case CSSPropertyWebkitTextStroke:
        return parseShorthand(propId, webkitTextStrokeShorthand(), important);
    case CSSPropertyAnimation:
        if (!RuntimeEnabledFeatures::cssAnimationUnprefixedEnabled())
            break;
    case CSSPropertyWebkitAnimation:
        return parseAnimationShorthand(propId, important);
    case CSSPropertyTransition:
    case CSSPropertyWebkitTransition:
        return parseTransitionShorthand(propId, important);
    case CSSPropertyInvalid:
        return false;
    case CSSPropertyPage:
        return parsePage(propId, important);
    case CSSPropertyFontStretch:
        return false;
    // CSS Text Layout Module Level 3: Vertical writing support
    case CSSPropertyWebkitTextEmphasis:
        return parseShorthand(propId, webkitTextEmphasisShorthand(), important);

    case CSSPropertyWebkitTextEmphasisStyle:
        return parseTextEmphasisStyle(important);

    case CSSPropertyWebkitTextOrientation:
        // FIXME: For now just support sideways, sideways-right, upright and vertical-right.
        if (id == CSSValueSideways || id == CSSValueSidewaysRight || id == CSSValueVerticalRight || id == CSSValueUpright)
            validPrimitive = true;
        break;

    case CSSPropertyWebkitLineBoxContain:
        if (id == CSSValueNone)
            validPrimitive = true;
        else
            return parseLineBoxContain(important);
        break;
    case CSSPropertyWebkitFontFeatureSettings:
        if (id == CSSValueNormal)
            validPrimitive = true;
        else
            return parseFontFeatureSettings(important);
        break;

    case CSSPropertyFontVariantLigatures:
        if (id == CSSValueNormal)
            validPrimitive = true;
        else
            return parseFontVariantLigatures(important);
        break;
    case CSSPropertyWebkitClipPath:
        if (id == CSSValueNone) {
            validPrimitive = true;
        } else if (value->unit == CSSParserValue::Function) {
            parsedValue = parseBasicShape();
        } else if (value->unit == CSSPrimitiveValue::CSS_URI) {
            parsedValue = CSSPrimitiveValue::create(value->string, CSSPrimitiveValue::CSS_URI);
            addProperty(propId, parsedValue.release(), important);
            return true;
        }
        break;
    case CSSPropertyShapeInside:
    case CSSPropertyShapeOutside:
        parsedValue = parseShapeProperty(propId);
        break;
    case CSSPropertyShapeMargin:
    case CSSPropertyShapePadding:
        validPrimitive = (RuntimeEnabledFeatures::cssShapesEnabled() && !id && validUnit(value, FLength | FNonNeg));
        break;
    case CSSPropertyShapeImageThreshold:
        validPrimitive = (RuntimeEnabledFeatures::cssShapesEnabled() && !id && validUnit(value, FNumber));
        break;

    case CSSPropertyTouchAction:
        // auto | none | [pan-x || pan-y]
        return parseTouchAction(important);

    case CSSPropertyAlignSelf:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        return parseItemPositionOverflowPosition(propId, important);

    case CSSPropertyAlignItems:
        ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());
        return parseItemPositionOverflowPosition(propId, important);

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
    case CSSPropertySpeak:
    case CSSPropertyTableLayout:
    case CSSPropertyTextAlignLast:
    case CSSPropertyTextJustify:
    case CSSPropertyTextLineThroughMode:
    case CSSPropertyTextLineThroughStyle:
    case CSSPropertyTextOverflow:
    case CSSPropertyTextOverlineMode:
    case CSSPropertyTextOverlineStyle:
    case CSSPropertyTextRendering:
    case CSSPropertyTextTransform:
    case CSSPropertyTextUnderlineMode:
    case CSSPropertyTextUnderlineStyle:
    case CSSPropertyTouchActionDelay:
    case CSSPropertyVisibility:
    case CSSPropertyWebkitAppearance:
    case CSSPropertyWebkitBackfaceVisibility:
    case CSSPropertyWebkitBorderAfterStyle:
    case CSSPropertyWebkitBorderBeforeStyle:
    case CSSPropertyWebkitBorderEndStyle:
    case CSSPropertyWebkitBorderFit:
    case CSSPropertyWebkitBorderStartStyle:
    case CSSPropertyWebkitBoxAlign:
    case CSSPropertyWebkitBoxDecorationBreak:
    case CSSPropertyWebkitBoxDirection:
    case CSSPropertyWebkitBoxLines:
    case CSSPropertyWebkitBoxOrient:
    case CSSPropertyWebkitBoxPack:
    case CSSPropertyInternalCallback:
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
    case CSSPropertyGridAutoFlow:
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
    case CSSPropertyWebkitTransformStyle:
    case CSSPropertyWebkitUserDrag:
    case CSSPropertyWebkitUserModify:
    case CSSPropertyWebkitUserSelect:
    case CSSPropertyWebkitWrapFlow:
    case CSSPropertyWebkitWrapThrough:
    case CSSPropertyWebkitWritingMode:
    case CSSPropertyWhiteSpace:
    case CSSPropertyWordBreak:
    case CSSPropertyWordWrap:
    case CSSPropertyMixBlendMode:
    case CSSPropertyIsolation:
        // These properties should be handled before in isValidKeywordPropertyAndValue().
        ASSERT_NOT_REACHED();
        return false;
    // Properties below are validated inside parseViewportProperty, because we
    // check for parser state. We need to invalidate if someone adds them outside
    // a @viewport rule.
    case CSSPropertyMaxZoom:
    case CSSPropertyMinZoom:
    case CSSPropertyOrientation:
    case CSSPropertyUserZoom:
        validPrimitive = false;
        break;
    default:
        return parseSVGValue(propId, important);
    }

    if (validPrimitive) {
        parsedValue = parseValidPrimitive(id, value);
        m_valueList->next();
    }
    ASSERT(!m_parsedCalculation);
    if (parsedValue) {
        if (!m_valueList->current() || inShorthand()) {
            addProperty(propId, parsedValue.release(), important);
            return true;
        }
    }
    return false;
}

void CSSPropertyParser::addFillValue(RefPtrWillBeRawPtr<CSSValue>& lval, PassRefPtrWillBeRawPtr<CSSValue> rval)
{
    if (lval) {
        if (lval->isBaseValueList())
            toCSSValueList(lval.get())->append(rval);
        else {
            PassRefPtrWillBeRawPtr<CSSValue> oldlVal(lval.release());
            PassRefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            list->append(oldlVal);
            list->append(rval);
            lval = list;
        }
    }
    else
        lval = rval;
}

static bool parseBackgroundClip(CSSParserValue* parserValue, RefPtrWillBeRawPtr<CSSValue>& cssValue)
{
    if (parserValue->id == CSSValueBorderBox || parserValue->id == CSSValuePaddingBox
        || parserValue->id == CSSValueContentBox || parserValue->id == CSSValueWebkitText) {
        cssValue = cssValuePool().createIdentifierValue(parserValue->id);
        return true;
    }
    return false;
}

const int cMaxFillProperties = 9;

bool CSSPropertyParser::parseFillShorthand(CSSPropertyID propId, const CSSPropertyID* properties, int numProperties, bool important)
{
    ASSERT(numProperties <= cMaxFillProperties);
    if (numProperties > cMaxFillProperties)
        return false;

    ShorthandScope scope(this, propId);

    bool parsedProperty[cMaxFillProperties] = { false };
    RefPtrWillBeRawPtr<CSSValue> values[cMaxFillProperties];
    RefPtrWillBeRawPtr<CSSValue> clipValue;
    RefPtrWillBeRawPtr<CSSValue> positionYValue;
    RefPtrWillBeRawPtr<CSSValue> repeatYValue;
    bool foundClip = false;
    int i;
    bool foundPositionCSSProperty = false;

    while (m_valueList->current()) {
        CSSParserValue* val = m_valueList->current();
        if (val->unit == CSSParserValue::Operator && val->iValue == ',') {
            // We hit the end.  Fill in all remaining values with the initial value.
            m_valueList->next();
            for (i = 0; i < numProperties; ++i) {
                if (properties[i] == CSSPropertyBackgroundColor && parsedProperty[i])
                    // Color is not allowed except as the last item in a list for backgrounds.
                    // Reject the entire property.
                    return false;

                if (!parsedProperty[i] && properties[i] != CSSPropertyBackgroundColor) {
                    addFillValue(values[i], cssValuePool().createImplicitInitialValue());
                    if (properties[i] == CSSPropertyBackgroundPosition || properties[i] == CSSPropertyWebkitMaskPosition)
                        addFillValue(positionYValue, cssValuePool().createImplicitInitialValue());
                    if (properties[i] == CSSPropertyBackgroundRepeat || properties[i] == CSSPropertyWebkitMaskRepeat)
                        addFillValue(repeatYValue, cssValuePool().createImplicitInitialValue());
                    if ((properties[i] == CSSPropertyBackgroundOrigin || properties[i] == CSSPropertyWebkitMaskOrigin) && !parsedProperty[i]) {
                        // If background-origin wasn't present, then reset background-clip also.
                        addFillValue(clipValue, cssValuePool().createImplicitInitialValue());
                    }
                }
                parsedProperty[i] = false;
            }
            if (!m_valueList->current())
                break;
        }

        bool sizeCSSPropertyExpected = false;
        if (isForwardSlashOperator(val) && foundPositionCSSProperty) {
            sizeCSSPropertyExpected = true;
            m_valueList->next();
        }

        foundPositionCSSProperty = false;
        bool found = false;
        for (i = 0; !found && i < numProperties; ++i) {

            if (sizeCSSPropertyExpected && (properties[i] != CSSPropertyBackgroundSize && properties[i] != CSSPropertyWebkitMaskSize))
                continue;
            if (!sizeCSSPropertyExpected && (properties[i] == CSSPropertyBackgroundSize || properties[i] == CSSPropertyWebkitMaskSize))
                continue;

            if (!parsedProperty[i]) {
                RefPtrWillBeRawPtr<CSSValue> val1;
                RefPtrWillBeRawPtr<CSSValue> val2;
                CSSPropertyID propId1, propId2;
                CSSParserValue* parserValue = m_valueList->current();
                // parseFillProperty() may modify m_implicitShorthand, so we MUST reset it
                // before EACH return below.
                if (parseFillProperty(properties[i], propId1, propId2, val1, val2)) {
                    parsedProperty[i] = found = true;
                    addFillValue(values[i], val1.release());
                    if (properties[i] == CSSPropertyBackgroundPosition || properties[i] == CSSPropertyWebkitMaskPosition)
                        addFillValue(positionYValue, val2.release());
                    if (properties[i] == CSSPropertyBackgroundRepeat || properties[i] == CSSPropertyWebkitMaskRepeat)
                        addFillValue(repeatYValue, val2.release());
                    if (properties[i] == CSSPropertyBackgroundOrigin || properties[i] == CSSPropertyWebkitMaskOrigin) {
                        // Reparse the value as a clip, and see if we succeed.
                        if (parseBackgroundClip(parserValue, val1))
                            addFillValue(clipValue, val1.release()); // The property parsed successfully.
                        else
                            addFillValue(clipValue, cssValuePool().createImplicitInitialValue()); // Some value was used for origin that is not supported by clip. Just reset clip instead.
                    }
                    if (properties[i] == CSSPropertyBackgroundClip || properties[i] == CSSPropertyWebkitMaskClip) {
                        // Update clipValue
                        addFillValue(clipValue, val1.release());
                        foundClip = true;
                    }
                    if (properties[i] == CSSPropertyBackgroundPosition || properties[i] == CSSPropertyWebkitMaskPosition)
                        foundPositionCSSProperty = true;
                }
            }
        }

        // if we didn't find at least one match, this is an
        // invalid shorthand and we have to ignore it
        if (!found) {
            m_implicitShorthand = false;
            return false;
        }
    }

    // Now add all of the properties we found.
    for (i = 0; i < numProperties; i++) {
        // Fill in any remaining properties with the initial value.
        if (!parsedProperty[i]) {
            addFillValue(values[i], cssValuePool().createImplicitInitialValue());
            if (properties[i] == CSSPropertyBackgroundPosition || properties[i] == CSSPropertyWebkitMaskPosition)
                addFillValue(positionYValue, cssValuePool().createImplicitInitialValue());
            if (properties[i] == CSSPropertyBackgroundRepeat || properties[i] == CSSPropertyWebkitMaskRepeat)
                addFillValue(repeatYValue, cssValuePool().createImplicitInitialValue());
            if (properties[i] == CSSPropertyBackgroundOrigin || properties[i] == CSSPropertyWebkitMaskOrigin) {
                // If background-origin wasn't present, then reset background-clip also.
                addFillValue(clipValue, cssValuePool().createImplicitInitialValue());
            }
        }
        if (properties[i] == CSSPropertyBackgroundPosition) {
            addProperty(CSSPropertyBackgroundPositionX, values[i].release(), important);
            // it's OK to call positionYValue.release() since we only see CSSPropertyBackgroundPosition once
            addProperty(CSSPropertyBackgroundPositionY, positionYValue.release(), important);
        } else if (properties[i] == CSSPropertyWebkitMaskPosition) {
            addProperty(CSSPropertyWebkitMaskPositionX, values[i].release(), important);
            // it's OK to call positionYValue.release() since we only see CSSPropertyWebkitMaskPosition once
            addProperty(CSSPropertyWebkitMaskPositionY, positionYValue.release(), important);
        } else if (properties[i] == CSSPropertyBackgroundRepeat) {
            addProperty(CSSPropertyBackgroundRepeatX, values[i].release(), important);
            // it's OK to call repeatYValue.release() since we only see CSSPropertyBackgroundPosition once
            addProperty(CSSPropertyBackgroundRepeatY, repeatYValue.release(), important);
        } else if (properties[i] == CSSPropertyWebkitMaskRepeat) {
            addProperty(CSSPropertyWebkitMaskRepeatX, values[i].release(), important);
            // it's OK to call repeatYValue.release() since we only see CSSPropertyBackgroundPosition once
            addProperty(CSSPropertyWebkitMaskRepeatY, repeatYValue.release(), important);
        } else if ((properties[i] == CSSPropertyBackgroundClip || properties[i] == CSSPropertyWebkitMaskClip) && !foundClip)
            // Value is already set while updating origin
            continue;
        else if (properties[i] == CSSPropertyBackgroundSize && !parsedProperty[i] && m_context.useLegacyBackgroundSizeShorthandBehavior())
            continue;
        else
            addProperty(properties[i], values[i].release(), important);

        // Add in clip values when we hit the corresponding origin property.
        if (properties[i] == CSSPropertyBackgroundOrigin && !foundClip)
            addProperty(CSSPropertyBackgroundClip, clipValue.release(), important);
        else if (properties[i] == CSSPropertyWebkitMaskOrigin && !foundClip)
            addProperty(CSSPropertyWebkitMaskClip, clipValue.release(), important);
    }

    m_implicitShorthand = false;
    return true;
}

void CSSPropertyParser::addAnimationValue(RefPtrWillBeRawPtr<CSSValue>& lval, PassRefPtrWillBeRawPtr<CSSValue> rval)
{
    if (lval) {
        if (lval->isValueList())
            toCSSValueList(lval.get())->append(rval);
        else {
            PassRefPtrWillBeRawPtr<CSSValue> oldVal(lval.release());
            PassRefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
            list->append(oldVal);
            list->append(rval);
            lval = list;
        }
    }
    else
        lval = rval;
}

bool CSSPropertyParser::parseAnimationShorthand(CSSPropertyID propId, bool important)
{
    const StylePropertyShorthand& animationProperties = parsingShorthandForProperty(propId);
    const unsigned numProperties = 8;

    // The list of properties in the shorthand should be the same
    // length as the list with animation name in last position, even though they are
    // in a different order.
    ASSERT(numProperties == animationProperties.length());
    ASSERT(numProperties == shorthandForProperty(propId).length());

    ShorthandScope scope(this, propId);

    bool parsedProperty[numProperties] = { false };
    AnimationParseContext context;
    RefPtrWillBeRawPtr<CSSValue> values[numProperties];

    unsigned i;
    while (m_valueList->current()) {
        CSSParserValue* val = m_valueList->current();
        if (val->unit == CSSParserValue::Operator && val->iValue == ',') {
            // We hit the end.  Fill in all remaining values with the initial value.
            m_valueList->next();
            for (i = 0; i < numProperties; ++i) {
                if (!parsedProperty[i])
                    addAnimationValue(values[i], cssValuePool().createImplicitInitialValue());
                parsedProperty[i] = false;
            }
            if (!m_valueList->current())
                break;
            context.commitFirstAnimation();
        }

        bool found = false;
        for (i = 0; i < numProperties; ++i) {
            if (!parsedProperty[i]) {
                RefPtrWillBeRawPtr<CSSValue> val;
                if (parseAnimationProperty(animationProperties.properties()[i], val, context)) {
                    parsedProperty[i] = found = true;
                    addAnimationValue(values[i], val.release());
                    break;
                }
            }
        }

        // if we didn't find at least one match, this is an
        // invalid shorthand and we have to ignore it
        if (!found)
            return false;
    }

    for (i = 0; i < numProperties; ++i) {
        // If we didn't find the property, set an intial value.
        if (!parsedProperty[i])
            addAnimationValue(values[i], cssValuePool().createImplicitInitialValue());

        addProperty(animationProperties.properties()[i], values[i].release(), important);
    }

    return true;
}

bool CSSPropertyParser::parseTransitionShorthand(CSSPropertyID propId, bool important)
{
    const unsigned numProperties = 4;
    const StylePropertyShorthand& shorthand = shorthandForProperty(propId);
    ASSERT(numProperties == shorthand.length());

    ShorthandScope scope(this, propId);

    bool parsedProperty[numProperties] = { false };
    AnimationParseContext context;
    RefPtrWillBeRawPtr<CSSValue> values[numProperties];

    unsigned i;
    while (m_valueList->current()) {
        CSSParserValue* val = m_valueList->current();
        if (val->unit == CSSParserValue::Operator && val->iValue == ',') {
            // We hit the end. Fill in all remaining values with the initial value.
            m_valueList->next();
            for (i = 0; i < numProperties; ++i) {
                if (!parsedProperty[i])
                    addAnimationValue(values[i], cssValuePool().createImplicitInitialValue());
                parsedProperty[i] = false;
            }
            if (!m_valueList->current())
                break;
            context.commitFirstAnimation();
        }

        bool found = false;
        for (i = 0; !found && i < numProperties; ++i) {
            if (!parsedProperty[i]) {
                RefPtrWillBeRawPtr<CSSValue> val;
                if (parseAnimationProperty(shorthand.properties()[i], val, context)) {
                    parsedProperty[i] = found = true;
                    addAnimationValue(values[i], val.release());
                }

                // There are more values to process but 'none' or 'all' were already defined as the animation property, the declaration becomes invalid.
                if (!context.animationPropertyKeywordAllowed() && context.hasCommittedFirstAnimation())
                    return false;
            }
        }

        // if we didn't find at least one match, this is an
        // invalid shorthand and we have to ignore it
        if (!found)
            return false;
    }

    // Fill in any remaining properties with the initial value.
    for (i = 0; i < numProperties; ++i) {
        if (!parsedProperty[i])
            addAnimationValue(values[i], cssValuePool().createImplicitInitialValue());
    }

    // Now add all of the properties we found.
    for (i = 0; i < numProperties; i++)
        addPropertyWithPrefixingVariant(shorthand.properties()[i], values[i].release(), important);

    return true;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseColumnWidth()
{
    CSSParserValue* value = m_valueList->current();
    // Always parse lengths in strict mode here, since it would be ambiguous otherwise when used in
    // the 'columns' shorthand property.
    if (value->id == CSSValueAuto
        || (validUnit(value, FLength | FNonNeg, HTMLStandardMode) && value->fValue)) {
        RefPtrWillBeRawPtr<CSSValue> parsedValue = parseValidPrimitive(value->id, value);
        m_valueList->next();
        return parsedValue;
    }
    return nullptr;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseColumnCount()
{
    CSSParserValue* value = m_valueList->current();
    if (value->id == CSSValueAuto
        || (!value->id && validUnit(value, FPositiveInteger, HTMLQuirksMode))) {
        RefPtrWillBeRawPtr<CSSValue> parsedValue = parseValidPrimitive(value->id, value);
        m_valueList->next();
        return parsedValue;
    }
    return nullptr;
}

bool CSSPropertyParser::parseColumnsShorthand(bool important)
{
    RefPtrWillBeRawPtr<CSSValue> columnWidth;
    RefPtrWillBeRawPtr<CSSValue> columnCount;
    bool hasPendingExplicitAuto = false;

    for (unsigned propertiesParsed = 0; CSSParserValue* value = m_valueList->current(); propertiesParsed++) {
        if (propertiesParsed >= 2)
            return false; // Too many values for this shorthand. Invalid declaration.
        if (!propertiesParsed && value->id == CSSValueAuto) {
            // 'auto' is a valid value for any of the two longhands, and at this point we
            // don't know which one(s) it is meant for. We need to see if there are other
            // values first.
            m_valueList->next();
            hasPendingExplicitAuto = true;
        } else {
            if (!columnWidth) {
                if ((columnWidth = parseColumnWidth()))
                    continue;
            }
            if (!columnCount) {
                if ((columnCount = parseColumnCount()))
                    continue;
            }
            // If we didn't find at least one match, this is an
            // invalid shorthand and we have to ignore it.
            return false;
        }
    }
    if (hasPendingExplicitAuto) {
        // Time to assign the previously skipped 'auto' value to a property. If both properties are
        // unassigned at this point (i.e. 'columns:auto'), it doesn't matter that much which one we
        // set (although it does make a slight difference to web-inspector). The one we don't set
        // here will get an implicit 'auto' value further down.
        if (!columnWidth) {
            columnWidth = cssValuePool().createIdentifierValue(CSSValueAuto);
        } else {
            ASSERT(!columnCount);
            columnCount = cssValuePool().createIdentifierValue(CSSValueAuto);
        }
    }
    ASSERT(columnCount || columnWidth);

    // Any unassigned property at this point will become implicit 'auto'.
    if (columnWidth)
        addProperty(CSSPropertyWebkitColumnWidth, columnWidth, important);
    else
        addProperty(CSSPropertyWebkitColumnWidth, cssValuePool().createIdentifierValue(CSSValueAuto), important, true /* implicit */);
    if (columnCount)
        addProperty(CSSPropertyWebkitColumnCount, columnCount, important);
    else
        addProperty(CSSPropertyWebkitColumnCount, cssValuePool().createIdentifierValue(CSSValueAuto), important, true /* implicit */);
    return true;
}

bool CSSPropertyParser::parseShorthand(CSSPropertyID propId, const StylePropertyShorthand& shorthand, bool important)
{
    // We try to match as many properties as possible
    // We set up an array of booleans to mark which property has been found,
    // and we try to search for properties until it makes no longer any sense.
    ShorthandScope scope(this, propId);

    bool found = false;
    unsigned propertiesParsed = 0;
    bool propertyFound[6] = { false, false, false, false, false, false }; // 6 is enough size.

    while (m_valueList->current()) {
        found = false;
        for (unsigned propIndex = 0; !found && propIndex < shorthand.length(); ++propIndex) {
            if (!propertyFound[propIndex] && parseValue(shorthand.properties()[propIndex], important)) {
                propertyFound[propIndex] = found = true;
                propertiesParsed++;
            }
        }

        // if we didn't find at least one match, this is an
        // invalid shorthand and we have to ignore it
        if (!found)
            return false;
    }

    if (propertiesParsed == shorthand.length())
        return true;

    // Fill in any remaining properties with the initial value.
    ImplicitScope implicitScope(this, PropertyImplicit);
    const StylePropertyShorthand* const* const propertiesForInitialization = shorthand.propertiesForInitialization();
    for (unsigned i = 0; i < shorthand.length(); ++i) {
        if (propertyFound[i])
            continue;

        if (propertiesForInitialization) {
            const StylePropertyShorthand& initProperties = *(propertiesForInitialization[i]);
            for (unsigned propIndex = 0; propIndex < initProperties.length(); ++propIndex)
                addProperty(initProperties.properties()[propIndex], cssValuePool().createImplicitInitialValue(), important);
        } else
            addProperty(shorthand.properties()[i], cssValuePool().createImplicitInitialValue(), important);
    }

    return true;
}

bool CSSPropertyParser::parse4Values(CSSPropertyID propId, const CSSPropertyID *properties,  bool important)
{
    /* From the CSS 2 specs, 8.3
     * If there is only one value, it applies to all sides. If there are two values, the top and
     * bottom margins are set to the first value and the right and left margins are set to the second.
     * If there are three values, the top is set to the first value, the left and right are set to the
     * second, and the bottom is set to the third. If there are four values, they apply to the top,
     * right, bottom, and left, respectively.
     */

    int num = inShorthand() ? 1 : m_valueList->size();

    ShorthandScope scope(this, propId);

    // the order is top, right, bottom, left
    switch (num) {
        case 1: {
            if (!parseValue(properties[0], important))
                return false;
            CSSValue* value = m_parsedProperties.last().value();
            ImplicitScope implicitScope(this, PropertyImplicit);
            addProperty(properties[1], value, important);
            addProperty(properties[2], value, important);
            addProperty(properties[3], value, important);
            break;
        }
        case 2: {
            if (!parseValue(properties[0], important) || !parseValue(properties[1], important))
                return false;
            CSSValue* value = m_parsedProperties[m_parsedProperties.size() - 2].value();
            ImplicitScope implicitScope(this, PropertyImplicit);
            addProperty(properties[2], value, important);
            value = m_parsedProperties[m_parsedProperties.size() - 2].value();
            addProperty(properties[3], value, important);
            break;
        }
        case 3: {
            if (!parseValue(properties[0], important) || !parseValue(properties[1], important) || !parseValue(properties[2], important))
                return false;
            CSSValue* value = m_parsedProperties[m_parsedProperties.size() - 2].value();
            ImplicitScope implicitScope(this, PropertyImplicit);
            addProperty(properties[3], value, important);
            break;
        }
        case 4: {
            if (!parseValue(properties[0], important) || !parseValue(properties[1], important) ||
                !parseValue(properties[2], important) || !parseValue(properties[3], important))
                return false;
            break;
        }
        default: {
            return false;
        }
    }

    return true;
}

// auto | <identifier>
bool CSSPropertyParser::parsePage(CSSPropertyID propId, bool important)
{
    ASSERT(propId == CSSPropertyPage);

    if (m_valueList->size() != 1)
        return false;

    CSSParserValue* value = m_valueList->current();
    if (!value)
        return false;

    if (value->id == CSSValueAuto) {
        addProperty(propId, cssValuePool().createIdentifierValue(value->id), important);
        return true;
    } else if (value->id == 0 && value->unit == CSSPrimitiveValue::CSS_IDENT) {
        addProperty(propId, createPrimitiveStringValue(value), important);
        return true;
    }
    return false;
}

// <length>{1,2} | auto | [ <page-size> || [ portrait | landscape] ]
bool CSSPropertyParser::parseSize(CSSPropertyID propId, bool important)
{
    ASSERT(propId == CSSPropertySize);

    if (m_valueList->size() > 2)
        return false;

    CSSParserValue* value = m_valueList->current();
    if (!value)
        return false;

    RefPtrWillBeRawPtr<CSSValueList> parsedValues = CSSValueList::createSpaceSeparated();

    // First parameter.
    SizeParameterType paramType = parseSizeParameter(parsedValues.get(), value, None);
    if (paramType == None)
        return false;

    // Second parameter, if any.
    value = m_valueList->next();
    if (value) {
        paramType = parseSizeParameter(parsedValues.get(), value, paramType);
        if (paramType == None)
            return false;
    }

    addProperty(propId, parsedValues.release(), important);
    return true;
}

CSSPropertyParser::SizeParameterType CSSPropertyParser::parseSizeParameter(CSSValueList* parsedValues, CSSParserValue* value, SizeParameterType prevParamType)
{
    switch (value->id) {
    case CSSValueAuto:
        if (prevParamType == None) {
            parsedValues->append(cssValuePool().createIdentifierValue(value->id));
            return Auto;
        }
        return None;
    case CSSValueLandscape:
    case CSSValuePortrait:
        if (prevParamType == None || prevParamType == PageSize) {
            parsedValues->append(cssValuePool().createIdentifierValue(value->id));
            return Orientation;
        }
        return None;
    case CSSValueA3:
    case CSSValueA4:
    case CSSValueA5:
    case CSSValueB4:
    case CSSValueB5:
    case CSSValueLedger:
    case CSSValueLegal:
    case CSSValueLetter:
        if (prevParamType == None || prevParamType == Orientation) {
            // Normalize to Page Size then Orientation order by prepending.
            // This is not specified by the CSS3 Paged Media specification, but for simpler processing later (StyleResolver::applyPageSizeProperty).
            parsedValues->prepend(cssValuePool().createIdentifierValue(value->id));
            return PageSize;
        }
        return None;
    case 0:
        if (validUnit(value, FLength | FNonNeg) && (prevParamType == None || prevParamType == Length)) {
            parsedValues->append(createPrimitiveNumericValue(value));
            return Length;
        }
        return None;
    default:
        return None;
    }
}

// [ <string> <string> ]+ | inherit | none
// inherit and none are handled in parseValue.
bool CSSPropertyParser::parseQuotes(CSSPropertyID propId, bool important)
{
    RefPtrWillBeRawPtr<CSSValueList> values = CSSValueList::createCommaSeparated();
    while (CSSParserValue* val = m_valueList->current()) {
        RefPtrWillBeRawPtr<CSSValue> parsedValue;
        if (val->unit == CSSPrimitiveValue::CSS_STRING)
            parsedValue = CSSPrimitiveValue::create(val->string, CSSPrimitiveValue::CSS_STRING);
        else
            break;
        values->append(parsedValue.release());
        m_valueList->next();
    }
    if (values->length()) {
        addProperty(propId, values.release(), important);
        m_valueList->next();
        return true;
    }
    return false;
}

// [ <string> | <uri> | <counter> | attr(X) | open-quote | close-quote | no-open-quote | no-close-quote ]+ | inherit
// in CSS 2.1 this got somewhat reduced:
// [ <string> | attr(X) | open-quote | close-quote | no-open-quote | no-close-quote ]+ | inherit
bool CSSPropertyParser::parseContent(CSSPropertyID propId, bool important)
{
    RefPtrWillBeRawPtr<CSSValueList> values = CSSValueList::createCommaSeparated();

    while (CSSParserValue* val = m_valueList->current()) {
        RefPtrWillBeRawPtr<CSSValue> parsedValue;
        if (val->unit == CSSPrimitiveValue::CSS_URI) {
            // url
            parsedValue = CSSImageValue::create(completeURL(val->string));
        } else if (val->unit == CSSParserValue::Function) {
            // attr(X) | counter(X [,Y]) | counters(X, Y, [,Z]) | -webkit-gradient(...)
            CSSParserValueList* args = val->function->args.get();
            if (!args)
                return false;
            if (equalIgnoringCase(val->function->name, "attr(")) {
                parsedValue = parseAttr(args);
                if (!parsedValue)
                    return false;
            } else if (equalIgnoringCase(val->function->name, "counter(")) {
                parsedValue = parseCounterContent(args, false);
                if (!parsedValue)
                    return false;
            } else if (equalIgnoringCase(val->function->name, "counters(")) {
                parsedValue = parseCounterContent(args, true);
                if (!parsedValue)
                    return false;
            } else if (equalIgnoringCase(val->function->name, "-webkit-image-set(")) {
                parsedValue = parseImageSet(m_valueList.get());
                if (!parsedValue)
                    return false;
            } else if (isGeneratedImageValue(val)) {
                if (!parseGeneratedImage(m_valueList.get(), parsedValue))
                    return false;
            } else
                return false;
        } else if (val->unit == CSSPrimitiveValue::CSS_IDENT) {
            // open-quote
            // close-quote
            // no-open-quote
            // no-close-quote
            // inherit
            // FIXME: These are not yet implemented (http://bugs.webkit.org/show_bug.cgi?id=6503).
            // none
            // normal
            switch (val->id) {
            case CSSValueOpenQuote:
            case CSSValueCloseQuote:
            case CSSValueNoOpenQuote:
            case CSSValueNoCloseQuote:
            case CSSValueNone:
            case CSSValueNormal:
                parsedValue = cssValuePool().createIdentifierValue(val->id);
            default:
                break;
            }
        } else if (val->unit == CSSPrimitiveValue::CSS_STRING) {
            parsedValue = createPrimitiveStringValue(val);
        }
        if (!parsedValue)
            break;
        values->append(parsedValue.release());
        m_valueList->next();
    }

    if (values->length()) {
        addProperty(propId, values.release(), important);
        m_valueList->next();
        return true;
    }

    return false;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseAttr(CSSParserValueList* args)
{
    if (args->size() != 1)
        return nullptr;

    CSSParserValue* a = args->current();

    if (a->unit != CSSPrimitiveValue::CSS_IDENT)
        return nullptr;

    String attrName = a->string;
    // CSS allows identifiers with "-" at the start, like "-webkit-mask-image".
    // But HTML attribute names can't have those characters, and we should not
    // even parse them inside attr().
    if (attrName[0] == '-')
        return nullptr;

    if (m_context.isHTMLDocument())
        attrName = attrName.lower();

    return cssValuePool().createValue(attrName, CSSPrimitiveValue::CSS_ATTR);
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseBackgroundColor()
{
    CSSValueID id = m_valueList->current()->id;
    if (id == CSSValueWebkitText || (id >= CSSValueAqua && id <= CSSValueWindowtext) || id == CSSValueMenu || id == CSSValueCurrentcolor ||
        (id >= CSSValueGrey && id < CSSValueWebkitText && inQuirksMode()))
        return cssValuePool().createIdentifierValue(id);
    return parseColor();
}

bool CSSPropertyParser::parseFillImage(CSSParserValueList* valueList, RefPtrWillBeRawPtr<CSSValue>& value)
{
    if (valueList->current()->id == CSSValueNone) {
        value = cssValuePool().createIdentifierValue(CSSValueNone);
        return true;
    }
    if (valueList->current()->unit == CSSPrimitiveValue::CSS_URI) {
        value = CSSImageValue::create(completeURL(valueList->current()->string));
        return true;
    }

    if (isGeneratedImageValue(valueList->current()))
        return parseGeneratedImage(valueList, value);

    if (valueList->current()->unit == CSSParserValue::Function && equalIgnoringCase(valueList->current()->function->name, "-webkit-image-set(")) {
        value = parseImageSet(m_valueList.get());
        if (value)
            return true;
    }

    return false;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseFillPositionX(CSSParserValueList* valueList)
{
    int id = valueList->current()->id;
    if (id == CSSValueLeft || id == CSSValueRight || id == CSSValueCenter) {
        int percent = 0;
        if (id == CSSValueRight)
            percent = 100;
        else if (id == CSSValueCenter)
            percent = 50;
        return cssValuePool().createValue(percent, CSSPrimitiveValue::CSS_PERCENTAGE);
    }
    if (validUnit(valueList->current(), FPercent | FLength))
        return createPrimitiveNumericValue(valueList->current());
    return nullptr;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseFillPositionY(CSSParserValueList* valueList)
{
    int id = valueList->current()->id;
    if (id == CSSValueTop || id == CSSValueBottom || id == CSSValueCenter) {
        int percent = 0;
        if (id == CSSValueBottom)
            percent = 100;
        else if (id == CSSValueCenter)
            percent = 50;
        return cssValuePool().createValue(percent, CSSPrimitiveValue::CSS_PERCENTAGE);
    }
    if (validUnit(valueList->current(), FPercent | FLength))
        return createPrimitiveNumericValue(valueList->current());
    return nullptr;
}

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> CSSPropertyParser::parseFillPositionComponent(CSSParserValueList* valueList, unsigned& cumulativeFlags, FillPositionFlag& individualFlag, FillPositionParsingMode parsingMode)
{
    CSSValueID id = valueList->current()->id;
    if (id == CSSValueLeft || id == CSSValueTop || id == CSSValueRight || id == CSSValueBottom || id == CSSValueCenter) {
        int percent = 0;
        if (id == CSSValueLeft || id == CSSValueRight) {
            if (cumulativeFlags & XFillPosition)
                return nullptr;
            cumulativeFlags |= XFillPosition;
            individualFlag = XFillPosition;
            if (id == CSSValueRight)
                percent = 100;
        }
        else if (id == CSSValueTop || id == CSSValueBottom) {
            if (cumulativeFlags & YFillPosition)
                return nullptr;
            cumulativeFlags |= YFillPosition;
            individualFlag = YFillPosition;
            if (id == CSSValueBottom)
                percent = 100;
        } else if (id == CSSValueCenter) {
            // Center is ambiguous, so we're not sure which position we've found yet, an x or a y.
            percent = 50;
            cumulativeFlags |= AmbiguousFillPosition;
            individualFlag = AmbiguousFillPosition;
        }

        if (parsingMode == ResolveValuesAsKeyword)
            return cssValuePool().createIdentifierValue(id);

        return cssValuePool().createValue(percent, CSSPrimitiveValue::CSS_PERCENTAGE);
    }
    if (validUnit(valueList->current(), FPercent | FLength)) {
        if (!cumulativeFlags) {
            cumulativeFlags |= XFillPosition;
            individualFlag = XFillPosition;
        } else if (cumulativeFlags & (XFillPosition | AmbiguousFillPosition)) {
            cumulativeFlags |= YFillPosition;
            individualFlag = YFillPosition;
        } else {
            if (m_parsedCalculation)
                m_parsedCalculation.release();
            return nullptr;
        }
        return createPrimitiveNumericValue(valueList->current());
    }
    return nullptr;
}

static bool isValueConflictingWithCurrentEdge(int value1, int value2)
{
    if ((value1 == CSSValueLeft || value1 == CSSValueRight) && (value2 == CSSValueLeft || value2 == CSSValueRight))
        return true;

    if ((value1 == CSSValueTop || value1 == CSSValueBottom) && (value2 == CSSValueTop || value2 == CSSValueBottom))
        return true;

    return false;
}

static bool isFillPositionKeyword(CSSValueID value)
{
    return value == CSSValueLeft || value == CSSValueTop || value == CSSValueBottom || value == CSSValueRight || value == CSSValueCenter;
}

void CSSPropertyParser::parse4ValuesFillPosition(CSSParserValueList* valueList, RefPtrWillBeRawPtr<CSSValue>& value1, RefPtrWillBeRawPtr<CSSValue>& value2, PassRefPtrWillBeRawPtr<CSSPrimitiveValue> parsedValue1, PassRefPtrWillBeRawPtr<CSSPrimitiveValue> parsedValue2)
{
    // [ left | right ] [ <percentage] | <length> ] && [ top | bottom ] [ <percentage> | <length> ]
    // In the case of 4 values <position> requires the second value to be a length or a percentage.
    if (isFillPositionKeyword(parsedValue2->getValueID()))
        return;

    unsigned cumulativeFlags = 0;
    FillPositionFlag value3Flag = InvalidFillPosition;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> value3 = parseFillPositionComponent(valueList, cumulativeFlags, value3Flag, ResolveValuesAsKeyword);
    if (!value3)
        return;

    CSSValueID ident1 = parsedValue1->getValueID();
    CSSValueID ident3 = value3->getValueID();

    if (ident1 == CSSValueCenter)
        return;

    if (!isFillPositionKeyword(ident3) || ident3 == CSSValueCenter)
        return;

    // We need to check if the values are not conflicting, e.g. they are not on the same edge. It is
    // needed as the second call to parseFillPositionComponent was on purpose not checking it. In the
    // case of two values top 20px is invalid but in the case of 4 values it becomes valid.
    if (isValueConflictingWithCurrentEdge(ident1, ident3))
        return;

    valueList->next();

    cumulativeFlags = 0;
    FillPositionFlag value4Flag = InvalidFillPosition;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> value4 = parseFillPositionComponent(valueList, cumulativeFlags, value4Flag, ResolveValuesAsKeyword);
    if (!value4)
        return;

    // 4th value must be a length or a percentage.
    if (isFillPositionKeyword(value4->getValueID()))
        return;

    value1 = createPrimitiveValuePair(parsedValue1, parsedValue2);
    value2 = createPrimitiveValuePair(value3, value4);

    if (ident1 == CSSValueTop || ident1 == CSSValueBottom)
        value1.swap(value2);

    valueList->next();
}
void CSSPropertyParser::parse3ValuesFillPosition(CSSParserValueList* valueList, RefPtrWillBeRawPtr<CSSValue>& value1, RefPtrWillBeRawPtr<CSSValue>& value2, PassRefPtrWillBeRawPtr<CSSPrimitiveValue> parsedValue1, PassRefPtrWillBeRawPtr<CSSPrimitiveValue> parsedValue2)
{
    unsigned cumulativeFlags = 0;
    FillPositionFlag value3Flag = InvalidFillPosition;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> value3 = parseFillPositionComponent(valueList, cumulativeFlags, value3Flag, ResolveValuesAsKeyword);

    // value3 is not an expected value, we return.
    if (!value3)
        return;

    valueList->next();

    bool swapNeeded = false;
    CSSValueID ident1 = parsedValue1->getValueID();
    CSSValueID ident2 = parsedValue2->getValueID();
    CSSValueID ident3 = value3->getValueID();

    CSSValueID firstPositionKeyword;
    CSSValueID secondPositionKeyword;

    if (ident1 == CSSValueCenter) {
        // <position> requires the first 'center' to be followed by a keyword.
        if (!isFillPositionKeyword(ident2))
            return;

        // If 'center' is the first keyword then the last one needs to be a length.
        if (isFillPositionKeyword(ident3))
            return;

        firstPositionKeyword = CSSValueLeft;
        if (ident2 == CSSValueLeft || ident2 == CSSValueRight) {
            firstPositionKeyword = CSSValueTop;
            swapNeeded = true;
        }
        value1 = createPrimitiveValuePair(cssValuePool().createIdentifierValue(firstPositionKeyword), cssValuePool().createValue(50, CSSPrimitiveValue::CSS_PERCENTAGE));
        value2 = createPrimitiveValuePair(parsedValue2, value3);
    } else if (ident3 == CSSValueCenter) {
        if (isFillPositionKeyword(ident2))
            return;

        secondPositionKeyword = CSSValueTop;
        if (ident1 == CSSValueTop || ident1 == CSSValueBottom) {
            secondPositionKeyword = CSSValueLeft;
            swapNeeded = true;
        }
        value1 = createPrimitiveValuePair(parsedValue1, parsedValue2);
        value2 = createPrimitiveValuePair(cssValuePool().createIdentifierValue(secondPositionKeyword), cssValuePool().createValue(50, CSSPrimitiveValue::CSS_PERCENTAGE));
    } else {
        RefPtrWillBeRawPtr<CSSPrimitiveValue> firstPositionValue;
        RefPtrWillBeRawPtr<CSSPrimitiveValue> secondPositionValue;

        if (isFillPositionKeyword(ident2)) {
            // To match CSS grammar, we should only accept: [ center | left | right | bottom | top ] [ left | right | top | bottom ] [ <percentage> | <length> ].
            ASSERT(ident2 != CSSValueCenter);

            if (isFillPositionKeyword(ident3))
                return;

            secondPositionValue = value3;
            secondPositionKeyword = ident2;
            firstPositionValue = cssValuePool().createValue(0, CSSPrimitiveValue::CSS_PERCENTAGE);
        } else {
            // Per CSS, we should only accept: [ right | left | top | bottom ] [ <percentage> | <length> ] [ center | left | right | bottom | top ].
            if (!isFillPositionKeyword(ident3))
                return;

            firstPositionValue = parsedValue2;
            secondPositionKeyword = ident3;
            secondPositionValue = cssValuePool().createValue(0, CSSPrimitiveValue::CSS_PERCENTAGE);
        }

        if (isValueConflictingWithCurrentEdge(ident1, secondPositionKeyword))
            return;

        value1 = createPrimitiveValuePair(parsedValue1, firstPositionValue);
        value2 = createPrimitiveValuePair(cssValuePool().createIdentifierValue(secondPositionKeyword), secondPositionValue);
    }

    if (ident1 == CSSValueTop || ident1 == CSSValueBottom || swapNeeded)
        value1.swap(value2);

#ifndef NDEBUG
    CSSPrimitiveValue* first = toCSSPrimitiveValue(value1.get());
    CSSPrimitiveValue* second = toCSSPrimitiveValue(value2.get());
    ident1 = first->getPairValue()->first()->getValueID();
    ident2 = second->getPairValue()->first()->getValueID();
    ASSERT(ident1 == CSSValueLeft || ident1 == CSSValueRight);
    ASSERT(ident2 == CSSValueBottom || ident2 == CSSValueTop);
#endif
}

inline bool CSSPropertyParser::isPotentialPositionValue(CSSParserValue* value)
{
    return isFillPositionKeyword(value->id) || validUnit(value, FPercent | FLength, ReleaseParsedCalcValue);
}

void CSSPropertyParser::parseFillPosition(CSSParserValueList* valueList, RefPtrWillBeRawPtr<CSSValue>& value1, RefPtrWillBeRawPtr<CSSValue>& value2)
{
    unsigned numberOfValues = 0;
    for (unsigned i = valueList->currentIndex(); i < valueList->size(); ++i, ++numberOfValues) {
        CSSParserValue* current = valueList->valueAt(i);
        if (isComma(current) || !current || isForwardSlashOperator(current) || !isPotentialPositionValue(current))
            break;
    }

    if (numberOfValues > 4)
        return;

    // If we are parsing two values, we can safely call the CSS 2.1 parsing function and return.
    if (numberOfValues <= 2) {
        parse2ValuesFillPosition(valueList, value1, value2);
        return;
    }

    ASSERT(numberOfValues > 2 && numberOfValues <= 4);

    CSSParserValue* value = valueList->current();

    // <position> requires the first value to be a background keyword.
    if (!isFillPositionKeyword(value->id))
        return;

    // Parse the first value. We're just making sure that it is one of the valid keywords or a percentage/length.
    unsigned cumulativeFlags = 0;
    FillPositionFlag value1Flag = InvalidFillPosition;
    FillPositionFlag value2Flag = InvalidFillPosition;
    value1 = parseFillPositionComponent(valueList, cumulativeFlags, value1Flag, ResolveValuesAsKeyword);
    if (!value1)
        return;

    valueList->next();

    // In case we are parsing more than two values, relax the check inside of parseFillPositionComponent. top 20px is
    // a valid start for <position>.
    cumulativeFlags = AmbiguousFillPosition;
    value2 = parseFillPositionComponent(valueList, cumulativeFlags, value2Flag, ResolveValuesAsKeyword);
    if (value2)
        valueList->next();
    else {
        value1.clear();
        return;
    }

    RefPtrWillBeRawPtr<CSSPrimitiveValue> parsedValue1 = toCSSPrimitiveValue(value1.get());
    RefPtrWillBeRawPtr<CSSPrimitiveValue> parsedValue2 = toCSSPrimitiveValue(value2.get());

    value1.clear();
    value2.clear();

    // Per CSS3 syntax, <position> can't have 'center' as its second keyword as we have more arguments to follow.
    if (parsedValue2->getValueID() == CSSValueCenter)
        return;

    if (numberOfValues == 3)
        parse3ValuesFillPosition(valueList, value1, value2, parsedValue1.release(), parsedValue2.release());
    else
        parse4ValuesFillPosition(valueList, value1, value2, parsedValue1.release(), parsedValue2.release());
}

void CSSPropertyParser::parse2ValuesFillPosition(CSSParserValueList* valueList, RefPtrWillBeRawPtr<CSSValue>& value1, RefPtrWillBeRawPtr<CSSValue>& value2)
{
    CSSParserValue* value = valueList->current();

    // Parse the first value.  We're just making sure that it is one of the valid keywords or a percentage/length.
    unsigned cumulativeFlags = 0;
    FillPositionFlag value1Flag = InvalidFillPosition;
    FillPositionFlag value2Flag = InvalidFillPosition;
    value1 = parseFillPositionComponent(valueList, cumulativeFlags, value1Flag);
    if (!value1)
        return;

    // It only takes one value for background-position to be correctly parsed if it was specified in a shorthand (since we
    // can assume that any other values belong to the rest of the shorthand).  If we're not parsing a shorthand, though, the
    // value was explicitly specified for our property.
    value = valueList->next();

    // First check for the comma.  If so, we are finished parsing this value or value pair.
    if (isComma(value))
        value = 0;

    if (value) {
        value2 = parseFillPositionComponent(valueList, cumulativeFlags, value2Flag);
        if (value2)
            valueList->next();
        else {
            if (!inShorthand()) {
                value1.clear();
                return;
            }
        }
    }

    if (!value2)
        // Only one value was specified. If that value was not a keyword, then it sets the x position, and the y position
        // is simply 50%. This is our default.
        // For keywords, the keyword was either an x-keyword (left/right), a y-keyword (top/bottom), or an ambiguous keyword (center).
        // For left/right/center, the default of 50% in the y is still correct.
        value2 = cssValuePool().createValue(50, CSSPrimitiveValue::CSS_PERCENTAGE);

    if (value1Flag == YFillPosition || value2Flag == XFillPosition)
        value1.swap(value2);
}

void CSSPropertyParser::parseFillRepeat(RefPtrWillBeRawPtr<CSSValue>& value1, RefPtrWillBeRawPtr<CSSValue>& value2)
{
    CSSValueID id = m_valueList->current()->id;
    if (id == CSSValueRepeatX) {
        m_implicitShorthand = true;
        value1 = cssValuePool().createIdentifierValue(CSSValueRepeat);
        value2 = cssValuePool().createIdentifierValue(CSSValueNoRepeat);
        m_valueList->next();
        return;
    }
    if (id == CSSValueRepeatY) {
        m_implicitShorthand = true;
        value1 = cssValuePool().createIdentifierValue(CSSValueNoRepeat);
        value2 = cssValuePool().createIdentifierValue(CSSValueRepeat);
        m_valueList->next();
        return;
    }
    if (id == CSSValueRepeat || id == CSSValueNoRepeat || id == CSSValueRound || id == CSSValueSpace)
        value1 = cssValuePool().createIdentifierValue(id);
    else {
        value1 = nullptr;
        return;
    }

    CSSParserValue* value = m_valueList->next();

    // Parse the second value if one is available
    if (value && !isComma(value)) {
        id = value->id;
        if (id == CSSValueRepeat || id == CSSValueNoRepeat || id == CSSValueRound || id == CSSValueSpace) {
            value2 = cssValuePool().createIdentifierValue(id);
            m_valueList->next();
            return;
        }
    }

    // If only one value was specified, value2 is the same as value1.
    m_implicitShorthand = true;
    value2 = cssValuePool().createIdentifierValue(toCSSPrimitiveValue(value1.get())->getValueID());
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseFillSize(CSSPropertyID propId, bool& allowComma)
{
    allowComma = true;
    CSSParserValue* value = m_valueList->current();

    if (value->id == CSSValueContain || value->id == CSSValueCover)
        return cssValuePool().createIdentifierValue(value->id);

    RefPtrWillBeRawPtr<CSSPrimitiveValue> parsedValue1;

    if (value->id == CSSValueAuto)
        parsedValue1 = cssValuePool().createIdentifierValue(CSSValueAuto);
    else {
        if (!validUnit(value, FLength | FPercent))
            return nullptr;
        parsedValue1 = createPrimitiveNumericValue(value);
    }

    RefPtrWillBeRawPtr<CSSPrimitiveValue> parsedValue2;
    if ((value = m_valueList->next())) {
        if (value->unit == CSSParserValue::Operator && value->iValue == ',')
            allowComma = false;
        else if (value->id != CSSValueAuto) {
            if (!validUnit(value, FLength | FPercent)) {
                if (!inShorthand())
                    return nullptr;
                // We need to rewind the value list, so that when it is advanced we'll end up back at this value.
                m_valueList->previous();
            } else
                parsedValue2 = createPrimitiveNumericValue(value);
        }
    } else if (!parsedValue2 && propId == CSSPropertyWebkitBackgroundSize) {
        // For backwards compatibility we set the second value to the first if it is omitted.
        // We only need to do this for -webkit-background-size. It should be safe to let masks match
        // the real property.
        parsedValue2 = parsedValue1;
    }

    if (!parsedValue2)
        return parsedValue1;
    return createPrimitiveValuePair(parsedValue1.release(), parsedValue2.release());
}

bool CSSPropertyParser::parseFillProperty(CSSPropertyID propId, CSSPropertyID& propId1, CSSPropertyID& propId2,
    RefPtrWillBeRawPtr<CSSValue>& retValue1, RefPtrWillBeRawPtr<CSSValue>& retValue2)
{
    RefPtrWillBeRawPtr<CSSValueList> values;
    RefPtrWillBeRawPtr<CSSValueList> values2;
    CSSParserValue* val;
    RefPtrWillBeRawPtr<CSSValue> value;
    RefPtrWillBeRawPtr<CSSValue> value2;

    bool allowComma = false;

    retValue1 = retValue2 = nullptr;
    propId1 = propId;
    propId2 = propId;
    if (propId == CSSPropertyBackgroundPosition) {
        propId1 = CSSPropertyBackgroundPositionX;
        propId2 = CSSPropertyBackgroundPositionY;
    } else if (propId == CSSPropertyWebkitMaskPosition) {
        propId1 = CSSPropertyWebkitMaskPositionX;
        propId2 = CSSPropertyWebkitMaskPositionY;
    } else if (propId == CSSPropertyBackgroundRepeat) {
        propId1 = CSSPropertyBackgroundRepeatX;
        propId2 = CSSPropertyBackgroundRepeatY;
    } else if (propId == CSSPropertyWebkitMaskRepeat) {
        propId1 = CSSPropertyWebkitMaskRepeatX;
        propId2 = CSSPropertyWebkitMaskRepeatY;
    }

    while ((val = m_valueList->current())) {
        RefPtrWillBeRawPtr<CSSValue> currValue;
        RefPtrWillBeRawPtr<CSSValue> currValue2;

        if (allowComma) {
            if (!isComma(val))
                return false;
            m_valueList->next();
            allowComma = false;
        } else {
            allowComma = true;
            switch (propId) {
                case CSSPropertyBackgroundColor:
                    currValue = parseBackgroundColor();
                    if (currValue)
                        m_valueList->next();
                    break;
                case CSSPropertyBackgroundAttachment:
                    if (val->id == CSSValueScroll || val->id == CSSValueFixed || val->id == CSSValueLocal) {
                        currValue = cssValuePool().createIdentifierValue(val->id);
                        m_valueList->next();
                    }
                    break;
                case CSSPropertyBackgroundImage:
                case CSSPropertyWebkitMaskImage:
                    if (parseFillImage(m_valueList.get(), currValue))
                        m_valueList->next();
                    break;
                case CSSPropertyWebkitBackgroundClip:
                case CSSPropertyWebkitBackgroundOrigin:
                case CSSPropertyWebkitMaskClip:
                case CSSPropertyWebkitMaskOrigin:
                    // The first three values here are deprecated and do not apply to the version of the property that has
                    // the -webkit- prefix removed.
                    if (val->id == CSSValueBorder || val->id == CSSValuePadding || val->id == CSSValueContent ||
                        val->id == CSSValueBorderBox || val->id == CSSValuePaddingBox || val->id == CSSValueContentBox ||
                        ((propId == CSSPropertyWebkitBackgroundClip || propId == CSSPropertyWebkitMaskClip) &&
                         (val->id == CSSValueText || val->id == CSSValueWebkitText))) {
                        currValue = cssValuePool().createIdentifierValue(val->id);
                        m_valueList->next();
                    }
                    break;
                case CSSPropertyBackgroundClip:
                    if (parseBackgroundClip(val, currValue))
                        m_valueList->next();
                    break;
                case CSSPropertyBackgroundOrigin:
                    if (val->id == CSSValueBorderBox || val->id == CSSValuePaddingBox || val->id == CSSValueContentBox) {
                        currValue = cssValuePool().createIdentifierValue(val->id);
                        m_valueList->next();
                    }
                    break;
                case CSSPropertyBackgroundPosition:
                case CSSPropertyWebkitMaskPosition:
                    parseFillPosition(m_valueList.get(), currValue, currValue2);
                    // parseFillPosition advances the m_valueList pointer.
                    break;
                case CSSPropertyBackgroundPositionX:
                case CSSPropertyWebkitMaskPositionX: {
                    currValue = parseFillPositionX(m_valueList.get());
                    if (currValue)
                        m_valueList->next();
                    break;
                }
                case CSSPropertyBackgroundPositionY:
                case CSSPropertyWebkitMaskPositionY: {
                    currValue = parseFillPositionY(m_valueList.get());
                    if (currValue)
                        m_valueList->next();
                    break;
                }
                case CSSPropertyWebkitBackgroundComposite:
                case CSSPropertyWebkitMaskComposite:
                    if (val->id >= CSSValueClear && val->id <= CSSValuePlusLighter) {
                        currValue = cssValuePool().createIdentifierValue(val->id);
                        m_valueList->next();
                    }
                    break;
                case CSSPropertyBackgroundBlendMode:
                    if (val->id == CSSValueNormal || val->id == CSSValueMultiply
                        || val->id == CSSValueScreen || val->id == CSSValueOverlay || val->id == CSSValueDarken
                        || val->id == CSSValueLighten ||  val->id == CSSValueColorDodge || val->id == CSSValueColorBurn
                        || val->id == CSSValueHardLight || val->id == CSSValueSoftLight || val->id == CSSValueDifference
                        || val->id == CSSValueExclusion || val->id == CSSValueHue || val->id == CSSValueSaturation
                        || val->id == CSSValueColor || val->id == CSSValueLuminosity) {
                        currValue = cssValuePool().createIdentifierValue(val->id);
                        m_valueList->next();
                    }
                    break;
                case CSSPropertyBackgroundRepeat:
                case CSSPropertyWebkitMaskRepeat:
                    parseFillRepeat(currValue, currValue2);
                    // parseFillRepeat advances the m_valueList pointer
                    break;
                case CSSPropertyBackgroundSize:
                case CSSPropertyWebkitBackgroundSize:
                case CSSPropertyWebkitMaskSize: {
                    currValue = parseFillSize(propId, allowComma);
                    if (currValue)
                        m_valueList->next();
                    break;
                }
                case CSSPropertyMaskSourceType: {
                    if (RuntimeEnabledFeatures::cssMaskSourceTypeEnabled()) {
                        if (val->id == CSSValueAuto || val->id == CSSValueAlpha || val->id == CSSValueLuminance) {
                            currValue = cssValuePool().createIdentifierValue(val->id);
                            m_valueList->next();
                        } else {
                            currValue = nullptr;
                        }
                    }
                    break;
                }
                default:
                    break;
            }
            if (!currValue)
                return false;

            if (value && !values) {
                values = CSSValueList::createCommaSeparated();
                values->append(value.release());
            }

            if (value2 && !values2) {
                values2 = CSSValueList::createCommaSeparated();
                values2->append(value2.release());
            }

            if (values)
                values->append(currValue.release());
            else
                value = currValue.release();
            if (currValue2) {
                if (values2)
                    values2->append(currValue2.release());
                else
                    value2 = currValue2.release();
            }
        }

        // When parsing any fill shorthand property, we let it handle building up the lists for all
        // properties.
        if (inShorthand())
            break;
    }

    if (values && values->length()) {
        retValue1 = values.release();
        if (values2 && values2->length())
            retValue2 = values2.release();
        return true;
    }
    if (value) {
        retValue1 = value.release();
        retValue2 = value2.release();
        return true;
    }
    return false;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseAnimationDelay()
{
    CSSParserValue* value = m_valueList->current();
    if (validUnit(value, FTime))
        return createPrimitiveNumericValue(value);
    return nullptr;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseAnimationDirection()
{
    CSSParserValue* value = m_valueList->current();
    if (value->id == CSSValueNormal || value->id == CSSValueAlternate || value->id == CSSValueReverse || value->id == CSSValueAlternateReverse)
        return cssValuePool().createIdentifierValue(value->id);
    return nullptr;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseAnimationDuration()
{
    CSSParserValue* value = m_valueList->current();
    if (validUnit(value, FTime | FNonNeg))
        return createPrimitiveNumericValue(value);
    return nullptr;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseAnimationFillMode()
{
    CSSParserValue* value = m_valueList->current();
    if (value->id == CSSValueNone || value->id == CSSValueForwards || value->id == CSSValueBackwards || value->id == CSSValueBoth)
        return cssValuePool().createIdentifierValue(value->id);
    return nullptr;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseAnimationIterationCount()
{
    CSSParserValue* value = m_valueList->current();
    if (value->id == CSSValueInfinite)
        return cssValuePool().createIdentifierValue(value->id);
    if (validUnit(value, FNumber | FNonNeg))
        return createPrimitiveNumericValue(value);
    return nullptr;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseAnimationName()
{
    CSSParserValue* value = m_valueList->current();
    if (value->unit == CSSPrimitiveValue::CSS_STRING || value->unit == CSSPrimitiveValue::CSS_IDENT) {
        if (value->id == CSSValueNone || (value->unit == CSSPrimitiveValue::CSS_STRING && equalIgnoringCase(value, "none"))) {
            return cssValuePool().createIdentifierValue(CSSValueNone);
        } else {
            return createPrimitiveStringValue(value);
        }
    }
    return nullptr;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseAnimationPlayState()
{
    CSSParserValue* value = m_valueList->current();
    if (value->id == CSSValueRunning || value->id == CSSValuePaused)
        return cssValuePool().createIdentifierValue(value->id);
    return nullptr;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseAnimationProperty(AnimationParseContext& context)
{
    CSSParserValue* value = m_valueList->current();
    if (value->unit != CSSPrimitiveValue::CSS_IDENT)
        return nullptr;
    CSSPropertyID result = cssPropertyID(value->string);
    if (result)
        return cssValuePool().createIdentifierValue(result);
    if (equalIgnoringCase(value, "all")) {
        context.sawAnimationPropertyKeyword();
        return cssValuePool().createIdentifierValue(CSSValueAll);
    }
    if (equalIgnoringCase(value, "none")) {
        context.commitAnimationPropertyKeyword();
        context.sawAnimationPropertyKeyword();
        return cssValuePool().createIdentifierValue(CSSValueNone);
    }
    return nullptr;
}

bool CSSPropertyParser::parseTransformOriginShorthand(RefPtrWillBeRawPtr<CSSValue>& value1, RefPtrWillBeRawPtr<CSSValue>& value2, RefPtrWillBeRawPtr<CSSValue>& value3)
{
    parse2ValuesFillPosition(m_valueList.get(), value1, value2);

    // now get z
    if (m_valueList->current()) {
        if (validUnit(m_valueList->current(), FLength)) {
            value3 = createPrimitiveNumericValue(m_valueList->current());
            m_valueList->next();
            return true;
        }
        return false;
    }
    value3 = cssValuePool().createImplicitInitialValue();
    return true;
}

bool CSSPropertyParser::parseCubicBezierTimingFunctionValue(CSSParserValueList*& args, double& result)
{
    CSSParserValue* v = args->current();
    if (!validUnit(v, FNumber))
        return false;
    result = v->fValue;
    v = args->next();
    if (!v)
        // The last number in the function has no comma after it, so we're done.
        return true;
    if (!isComma(v))
        return false;
    args->next();
    return true;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseAnimationTimingFunction()
{
    CSSParserValue* value = m_valueList->current();
    if (value->id == CSSValueEase || value->id == CSSValueLinear || value->id == CSSValueEaseIn || value->id == CSSValueEaseOut
        || value->id == CSSValueEaseInOut || value->id == CSSValueStepStart || value->id == CSSValueStepEnd)
        return cssValuePool().createIdentifierValue(value->id);

    // We must be a function.
    if (value->unit != CSSParserValue::Function)
        return nullptr;

    CSSParserValueList* args = value->function->args.get();

    if (equalIgnoringCase(value->function->name, "steps(")) {
        // For steps, 1 or 2 params must be specified (comma-separated)
        if (!args || (args->size() != 1 && args->size() != 3))
            return nullptr;

        // There are two values.
        int numSteps;
        bool stepAtStart = false;

        CSSParserValue* v = args->current();
        if (!validUnit(v, FInteger))
            return nullptr;
        numSteps = clampToInteger(v->fValue);
        if (numSteps < 1)
            return nullptr;
        v = args->next();

        if (v) {
            // There is a comma so we need to parse the second value
            if (!isComma(v))
                return nullptr;
            v = args->next();
            if (v->id != CSSValueStart && v->id != CSSValueEnd)
                return nullptr;
            stepAtStart = v->id == CSSValueStart;
        }

        return CSSStepsTimingFunctionValue::create(numSteps, stepAtStart);
    }

    if (equalIgnoringCase(value->function->name, "cubic-bezier(")) {
        // For cubic bezier, 4 values must be specified.
        if (!args || args->size() != 7)
            return nullptr;

        // There are two points specified. The x values must be between 0 and 1 but the y values can exceed this range.
        double x1, y1, x2, y2;

        if (!parseCubicBezierTimingFunctionValue(args, x1))
            return nullptr;
        if (x1 < 0 || x1 > 1)
            return nullptr;
        if (!parseCubicBezierTimingFunctionValue(args, y1))
            return nullptr;
        if (!parseCubicBezierTimingFunctionValue(args, x2))
            return nullptr;
        if (x2 < 0 || x2 > 1)
            return nullptr;
        if (!parseCubicBezierTimingFunctionValue(args, y2))
            return nullptr;

        return CSSCubicBezierTimingFunctionValue::create(x1, y1, x2, y2);
    }

    return nullptr;
}

bool CSSPropertyParser::parseAnimationProperty(CSSPropertyID propId, RefPtrWillBeRawPtr<CSSValue>& result, AnimationParseContext& context)
{
    RefPtrWillBeRawPtr<CSSValueList> values;
    CSSParserValue* val;
    RefPtrWillBeRawPtr<CSSValue> value;
    bool allowComma = false;

    result = nullptr;

    while ((val = m_valueList->current())) {
        RefPtrWillBeRawPtr<CSSValue> currValue;
        if (allowComma) {
            if (!isComma(val))
                return false;
            m_valueList->next();
            allowComma = false;
        }
        else {
            switch (propId) {
                case CSSPropertyAnimationDelay:
                case CSSPropertyWebkitAnimationDelay:
                case CSSPropertyTransitionDelay:
                case CSSPropertyWebkitTransitionDelay:
                    currValue = parseAnimationDelay();
                    if (currValue)
                        m_valueList->next();
                    break;
                case CSSPropertyAnimationDirection:
                case CSSPropertyWebkitAnimationDirection:
                    currValue = parseAnimationDirection();
                    if (currValue)
                        m_valueList->next();
                    break;
                case CSSPropertyAnimationDuration:
                case CSSPropertyWebkitAnimationDuration:
                case CSSPropertyTransitionDuration:
                case CSSPropertyWebkitTransitionDuration:
                    currValue = parseAnimationDuration();
                    if (currValue)
                        m_valueList->next();
                    break;
                case CSSPropertyAnimationFillMode:
                case CSSPropertyWebkitAnimationFillMode:
                    currValue = parseAnimationFillMode();
                    if (currValue)
                        m_valueList->next();
                    break;
                case CSSPropertyAnimationIterationCount:
                case CSSPropertyWebkitAnimationIterationCount:
                    currValue = parseAnimationIterationCount();
                    if (currValue)
                        m_valueList->next();
                    break;
                case CSSPropertyAnimationName:
                case CSSPropertyWebkitAnimationName:
                    currValue = parseAnimationName();
                    if (currValue)
                        m_valueList->next();
                    break;
                case CSSPropertyAnimationPlayState:
                case CSSPropertyWebkitAnimationPlayState:
                    currValue = parseAnimationPlayState();
                    if (currValue)
                        m_valueList->next();
                    break;
                case CSSPropertyTransitionProperty:
                case CSSPropertyWebkitTransitionProperty:
                    currValue = parseAnimationProperty(context);
                    if (value && !context.animationPropertyKeywordAllowed())
                        return false;
                    if (currValue)
                        m_valueList->next();
                    break;
                case CSSPropertyAnimationTimingFunction:
                case CSSPropertyWebkitAnimationTimingFunction:
                case CSSPropertyTransitionTimingFunction:
                case CSSPropertyWebkitTransitionTimingFunction:
                    currValue = parseAnimationTimingFunction();
                    if (currValue)
                        m_valueList->next();
                    break;
                default:
                    ASSERT_NOT_REACHED();
                    return false;
            }

            if (!currValue)
                return false;

            if (value && !values) {
                values = CSSValueList::createCommaSeparated();
                values->append(value.release());
            }

            if (values)
                values->append(currValue.release());
            else
                value = currValue.release();

            allowComma = true;
        }

        // When parsing the 'transition' shorthand property, we let it handle building up the lists for all
        // properties.
        if (inShorthand())
            break;
    }

    if (values && values->length()) {
        result = values.release();
        return true;
    }
    if (value) {
        result = value.release();
        return true;
    }
    return false;
}

// The function parses [ <integer> || <string> ] in <grid-line> (which can be stand alone or with 'span').
bool CSSPropertyParser::parseIntegerOrStringFromGridPosition(RefPtrWillBeRawPtr<CSSPrimitiveValue>& numericValue, RefPtrWillBeRawPtr<CSSPrimitiveValue>& gridLineName)
{
    CSSParserValue* value = m_valueList->current();
    if (validUnit(value, FInteger) && value->fValue) {
        numericValue = createPrimitiveNumericValue(value);
        value = m_valueList->next();
        if (value && value->unit == CSSPrimitiveValue::CSS_STRING) {
            gridLineName = createPrimitiveStringValue(m_valueList->current());
            m_valueList->next();
        }
        return true;
    }

    if (value->unit == CSSPrimitiveValue::CSS_STRING) {
        gridLineName = createPrimitiveStringValue(m_valueList->current());
        value = m_valueList->next();
        if (value && validUnit(value, FInteger) && value->fValue) {
            numericValue = createPrimitiveNumericValue(value);
            m_valueList->next();
        }
        return true;
    }

    return false;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseGridPosition()
{
    ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());

    CSSParserValue* value = m_valueList->current();
    if (value->id == CSSValueAuto) {
        m_valueList->next();
        return cssValuePool().createIdentifierValue(CSSValueAuto);
    }

    if (value->id != CSSValueSpan && value->unit == CSSPrimitiveValue::CSS_IDENT) {
        m_valueList->next();
        return cssValuePool().createValue(value->string, CSSPrimitiveValue::CSS_STRING);
    }

    RefPtrWillBeRawPtr<CSSPrimitiveValue> numericValue;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> gridLineName;
    bool hasSeenSpanKeyword = false;

    if (parseIntegerOrStringFromGridPosition(numericValue, gridLineName)) {
        value = m_valueList->current();
        if (value && value->id == CSSValueSpan) {
            hasSeenSpanKeyword = true;
            m_valueList->next();
        }
    } else if (value->id == CSSValueSpan) {
        hasSeenSpanKeyword = true;
        if (m_valueList->next())
            parseIntegerOrStringFromGridPosition(numericValue, gridLineName);
    }

    // Check that we have consumed all the value list. For shorthands, the parser will pass
    // the whole value list (including the opposite position).
    if (m_valueList->current() && !isForwardSlashOperator(m_valueList->current()))
        return nullptr;

    // If we didn't parse anything, this is not a valid grid position.
    if (!hasSeenSpanKeyword && !gridLineName && !numericValue)
        return nullptr;

    // Negative numbers are not allowed for span (but are for <integer>).
    if (hasSeenSpanKeyword && numericValue && numericValue->getIntValue() < 0)
        return nullptr;

    RefPtrWillBeRawPtr<CSSValueList> values = CSSValueList::createSpaceSeparated();
    if (hasSeenSpanKeyword)
        values->append(cssValuePool().createIdentifierValue(CSSValueSpan));
    if (numericValue)
        values->append(numericValue.release());
    if (gridLineName)
        values->append(gridLineName.release());
    ASSERT(values->length());
    return values.release();
}

static PassRefPtrWillBeRawPtr<CSSValue> gridMissingGridPositionValue(CSSValue* value)
{
    if (value->isPrimitiveValue() && toCSSPrimitiveValue(value)->isString())
        return value;

    return cssValuePool().createIdentifierValue(CSSValueAuto);
}

bool CSSPropertyParser::parseGridItemPositionShorthand(CSSPropertyID shorthandId, bool important)
{
    ShorthandScope scope(this, shorthandId);
    const StylePropertyShorthand& shorthand = shorthandForProperty(shorthandId);
    ASSERT(shorthand.length() == 2);

    RefPtrWillBeRawPtr<CSSValue> startValue = parseGridPosition();
    if (!startValue)
        return false;

    RefPtrWillBeRawPtr<CSSValue> endValue;
    if (m_valueList->current()) {
        if (!isForwardSlashOperator(m_valueList->current()))
            return false;

        if (!m_valueList->next())
            return false;

        endValue = parseGridPosition();
        if (!endValue || m_valueList->current())
            return false;
    } else {
        endValue = gridMissingGridPositionValue(startValue.get());
    }

    addProperty(shorthand.properties()[0], startValue, important);
    addProperty(shorthand.properties()[1], endValue, important);
    return true;
}

bool CSSPropertyParser::parseGridAreaShorthand(bool important)
{
    ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());

    ShorthandScope scope(this, CSSPropertyGridArea);
    const StylePropertyShorthand& shorthand = gridAreaShorthand();
    ASSERT_UNUSED(shorthand, shorthand.length() == 4);

    RefPtrWillBeRawPtr<CSSValue> rowStartValue = parseGridPosition();
    if (!rowStartValue)
        return false;

    RefPtrWillBeRawPtr<CSSValue> columnStartValue;
    if (!parseSingleGridAreaLonghand(columnStartValue))
        return false;

    RefPtrWillBeRawPtr<CSSValue> rowEndValue;
    if (!parseSingleGridAreaLonghand(rowEndValue))
        return false;

    RefPtrWillBeRawPtr<CSSValue> columnEndValue;
    if (!parseSingleGridAreaLonghand(columnEndValue))
        return false;

    if (!columnStartValue)
        columnStartValue = gridMissingGridPositionValue(rowStartValue.get());

    if (!rowEndValue)
        rowEndValue = gridMissingGridPositionValue(rowStartValue.get());

    if (!columnEndValue)
        columnEndValue = gridMissingGridPositionValue(columnStartValue.get());

    addProperty(CSSPropertyGridRowStart, rowStartValue, important);
    addProperty(CSSPropertyGridColumnStart, columnStartValue, important);
    addProperty(CSSPropertyGridRowEnd, rowEndValue, important);
    addProperty(CSSPropertyGridColumnEnd, columnEndValue, important);
    return true;
}

bool CSSPropertyParser::parseSingleGridAreaLonghand(RefPtrWillBeRawPtr<CSSValue>& property)
{
    if (!m_valueList->current())
        return true;

    if (!isForwardSlashOperator(m_valueList->current()))
        return false;

    if (!m_valueList->next())
        return false;

    property = parseGridPosition();
    return true;
}

void CSSPropertyParser::parseGridLineNames(CSSParserValueList* parserValueList, CSSValueList& valueList)
{
    ASSERT(parserValueList->current() && parserValueList->current()->unit == CSSParserValue::ValueList);

    CSSParserValueList* identList = parserValueList->current()->valueList;
    if (!identList->size()) {
        parserValueList->next();
        return;
    }

    RefPtrWillBeRawPtr<CSSGridLineNamesValue> lineNames = CSSGridLineNamesValue::create();
    while (CSSParserValue* identValue = identList->current()) {
        ASSERT(identValue->unit == CSSPrimitiveValue::CSS_IDENT);
        RefPtrWillBeRawPtr<CSSPrimitiveValue> lineName = createPrimitiveStringValue(identValue);
        lineNames->append(lineName.release());
        identList->next();
    }
    valueList.append(lineNames.release());

    parserValueList->next();
}

bool CSSPropertyParser::parseGridTrackList(CSSPropertyID propId, bool important)
{
    ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());

    CSSParserValue* value = m_valueList->current();
    if (value->id == CSSValueNone) {
        if (m_valueList->next())
            return false;

        addProperty(propId, cssValuePool().createIdentifierValue(value->id), important);
        return true;
    }

    RefPtrWillBeRawPtr<CSSValueList> values = CSSValueList::createSpaceSeparated();
    // Handle leading  <ident>*.
    value = m_valueList->current();
    if (value && value->unit == CSSParserValue::ValueList)
        parseGridLineNames(m_valueList.get(), *values);

    bool seenTrackSizeOrRepeatFunction = false;
    while (CSSParserValue* currentValue = m_valueList->current()) {
        if (currentValue->unit == CSSParserValue::Function && equalIgnoringCase(currentValue->function->name, "repeat(")) {
            if (!parseGridTrackRepeatFunction(*values))
                return false;
            seenTrackSizeOrRepeatFunction = true;
        } else {
            RefPtrWillBeRawPtr<CSSValue> value = parseGridTrackSize(*m_valueList);
            if (!value)
                return false;
            values->append(value);
            seenTrackSizeOrRepeatFunction = true;
        }
        // This will handle the trailing <ident>* in the grammar.
        value = m_valueList->current();
        if (value && value->unit == CSSParserValue::ValueList)
            parseGridLineNames(m_valueList.get(), *values);
    }

    // We should have found a <track-size> or else it is not a valid <track-list>
    if (!seenTrackSizeOrRepeatFunction)
        return false;

    addProperty(propId, values.release(), important);
    return true;
}

bool CSSPropertyParser::parseGridTrackRepeatFunction(CSSValueList& list)
{
    CSSParserValueList* arguments = m_valueList->current()->function->args.get();
    if (!arguments || arguments->size() < 3 || !validUnit(arguments->valueAt(0), FPositiveInteger) || !isComma(arguments->valueAt(1)))
        return false;

    ASSERT_WITH_SECURITY_IMPLICATION(arguments->valueAt(0)->fValue > 0);
    size_t repetitions = arguments->valueAt(0)->fValue;
    RefPtrWillBeRawPtr<CSSValueList> repeatedValues = CSSValueList::createSpaceSeparated();
    arguments->next(); // Skip the repetition count.
    arguments->next(); // Skip the comma.

    // Handle leading <ident>*.
    CSSParserValue* currentValue = arguments->current();
    if (currentValue && currentValue->unit == CSSParserValue::ValueList)
        parseGridLineNames(arguments, *repeatedValues);

    while (arguments->current()) {
        RefPtrWillBeRawPtr<CSSValue> trackSize = parseGridTrackSize(*arguments);
        if (!trackSize)
            return false;

        repeatedValues->append(trackSize);

        // This takes care of any trailing <ident>* in the grammar.
        currentValue = arguments->current();
        if (currentValue && currentValue->unit == CSSParserValue::ValueList)
            parseGridLineNames(arguments, *repeatedValues);
    }

    for (size_t i = 0; i < repetitions; ++i) {
        for (size_t j = 0; j < repeatedValues->length(); ++j)
            list.append(repeatedValues->itemWithoutBoundsCheck(j));
    }

    // parseGridTrackSize iterated over the repeat arguments, move to the next value.
    m_valueList->next();
    return true;
}


PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseGridTrackSize(CSSParserValueList& inputList)
{
    ASSERT(RuntimeEnabledFeatures::cssGridLayoutEnabled());

    CSSParserValue* currentValue = inputList.current();
    inputList.next();

    if (currentValue->id == CSSValueAuto)
        return cssValuePool().createIdentifierValue(CSSValueAuto);

    if (currentValue->unit == CSSParserValue::Function && equalIgnoringCase(currentValue->function->name, "minmax(")) {
        // The spec defines the following grammar: minmax( <track-breadth> , <track-breadth> )
        CSSParserValueList* arguments = currentValue->function->args.get();
        if (!arguments || arguments->size() != 3 || !isComma(arguments->valueAt(1)))
            return nullptr;

        RefPtrWillBeRawPtr<CSSPrimitiveValue> minTrackBreadth = parseGridBreadth(arguments->valueAt(0));
        if (!minTrackBreadth)
            return nullptr;

        RefPtrWillBeRawPtr<CSSPrimitiveValue> maxTrackBreadth = parseGridBreadth(arguments->valueAt(2));
        if (!maxTrackBreadth)
            return nullptr;

        RefPtrWillBeRawPtr<CSSValueList> parsedArguments = CSSValueList::createCommaSeparated();
        parsedArguments->append(minTrackBreadth);
        parsedArguments->append(maxTrackBreadth);
        return CSSFunctionValue::create("minmax(", parsedArguments);
    }

    return parseGridBreadth(currentValue);
}

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> CSSPropertyParser::parseGridBreadth(CSSParserValue* currentValue)
{
    if (currentValue->id == CSSValueMinContent || currentValue->id == CSSValueMaxContent)
        return cssValuePool().createIdentifierValue(currentValue->id);

    if (currentValue->unit == CSSPrimitiveValue::CSS_FR) {
        double flexValue = currentValue->fValue;

        // Fractional unit is a non-negative dimension.
        if (flexValue <= 0)
            return nullptr;

        return cssValuePool().createValue(flexValue, CSSPrimitiveValue::CSS_FR);
    }

    if (!validUnit(currentValue, FNonNeg | FLength | FPercent))
        return nullptr;

    return createPrimitiveNumericValue(currentValue);
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseGridTemplateAreas()
{
    NamedGridAreaMap gridAreaMap;
    size_t rowCount = 0;
    size_t columnCount = 0;

    while (CSSParserValue* currentValue = m_valueList->current()) {
        if (currentValue->unit != CSSPrimitiveValue::CSS_STRING)
            return nullptr;

        String gridRowNames = currentValue->string;
        if (!gridRowNames.length())
            return nullptr;

        Vector<String> columnNames;
        gridRowNames.split(' ', columnNames);

        if (!columnCount) {
            columnCount = columnNames.size();
            ASSERT(columnCount);
        } else if (columnCount != columnNames.size()) {
            // The declaration is invalid is all the rows don't have the number of columns.
            return nullptr;
        }

        for (size_t currentCol = 0; currentCol < columnCount; ++currentCol) {
            const String& gridAreaName = columnNames[currentCol];

            // Unamed areas are always valid (we consider them to be 1x1).
            if (gridAreaName == ".")
                continue;

            // We handle several grid areas with the same name at once to simplify the validation code.
            size_t lookAheadCol;
            for (lookAheadCol = currentCol; lookAheadCol < (columnCount - 1); ++lookAheadCol) {
                if (columnNames[lookAheadCol + 1] != gridAreaName)
                    break;
            }

            NamedGridAreaMap::iterator gridAreaIt = gridAreaMap.find(gridAreaName);
            if (gridAreaIt == gridAreaMap.end()) {
                gridAreaMap.add(gridAreaName, GridCoordinate(GridSpan(rowCount, rowCount), GridSpan(currentCol, lookAheadCol)));
            } else {
                GridCoordinate& gridCoordinate = gridAreaIt->value;

                // The following checks test that the grid area is a single filled-in rectangle.
                // 1. The new row is adjacent to the previously parsed row.
                if (rowCount != gridCoordinate.rows.initialPositionIndex + 1)
                    return nullptr;

                // 2. The new area starts at the same position as the previously parsed area.
                if (currentCol != gridCoordinate.columns.initialPositionIndex)
                    return nullptr;

                // 3. The new area ends at the same position as the previously parsed area.
                if (lookAheadCol != gridCoordinate.columns.finalPositionIndex)
                    return nullptr;

                ++gridCoordinate.rows.finalPositionIndex;
            }
            currentCol = lookAheadCol;
        }

        ++rowCount;
        m_valueList->next();
    }

    if (!rowCount || !columnCount)
        return nullptr;

    return CSSGridTemplateAreasValue::create(gridAreaMap, rowCount, columnCount);
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseCounterContent(CSSParserValueList* args, bool counters)
{
    unsigned numArgs = args->size();
    if (counters && numArgs != 3 && numArgs != 5)
        return nullptr;
    if (!counters && numArgs != 1 && numArgs != 3)
        return nullptr;

    CSSParserValue* i = args->current();
    if (i->unit != CSSPrimitiveValue::CSS_IDENT)
        return nullptr;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> identifier = createPrimitiveStringValue(i);

    RefPtrWillBeRawPtr<CSSPrimitiveValue> separator;
    if (!counters)
        separator = cssValuePool().createValue(String(), CSSPrimitiveValue::CSS_STRING);
    else {
        i = args->next();
        if (i->unit != CSSParserValue::Operator || i->iValue != ',')
            return nullptr;

        i = args->next();
        if (i->unit != CSSPrimitiveValue::CSS_STRING)
            return nullptr;

        separator = createPrimitiveStringValue(i);
    }

    RefPtrWillBeRawPtr<CSSPrimitiveValue> listStyle;
    i = args->next();
    if (!i) // Make the list style default decimal
        listStyle = cssValuePool().createIdentifierValue(CSSValueDecimal);
    else {
        if (i->unit != CSSParserValue::Operator || i->iValue != ',')
            return nullptr;

        i = args->next();
        if (i->unit != CSSPrimitiveValue::CSS_IDENT)
            return nullptr;

        CSSValueID listStyleID = CSSValueInvalid;
        if (i->id == CSSValueNone || (i->id >= CSSValueDisc && i->id <= CSSValueKatakanaIroha))
            listStyleID = i->id;
        else
            return nullptr;

        listStyle = cssValuePool().createIdentifierValue(listStyleID);
    }

    return cssValuePool().createValue(Counter::create(identifier.release(), listStyle.release(), separator.release()));
}

bool CSSPropertyParser::parseClipShape(CSSPropertyID propId, bool important)
{
    CSSParserValue* value = m_valueList->current();
    CSSParserValueList* args = value->function->args.get();

    if (!equalIgnoringCase(value->function->name, "rect(") || !args)
        return false;

    // rect(t, r, b, l) || rect(t r b l)
    if (args->size() != 4 && args->size() != 7)
        return false;
    RefPtrWillBeRawPtr<Rect> rect = Rect::create();
    bool valid = true;
    int i = 0;
    CSSParserValue* a = args->current();
    while (a) {
        valid = a->id == CSSValueAuto || validUnit(a, FLength);
        if (!valid)
            break;
        RefPtrWillBeRawPtr<CSSPrimitiveValue> length = a->id == CSSValueAuto ?
            cssValuePool().createIdentifierValue(CSSValueAuto) :
            createPrimitiveNumericValue(a);
        if (i == 0)
            rect->setTop(length);
        else if (i == 1)
            rect->setRight(length);
        else if (i == 2)
            rect->setBottom(length);
        else
            rect->setLeft(length);
        a = args->next();
        if (a && args->size() == 7) {
            if (a->unit == CSSParserValue::Operator && a->iValue == ',') {
                a = args->next();
            } else {
                valid = false;
                break;
            }
        }
        i++;
    }
    if (valid) {
        addProperty(propId, cssValuePool().createValue(rect.release()), important);
        m_valueList->next();
        return true;
    }
    return false;
}

static void completeBorderRadii(RefPtrWillBeRawPtr<CSSPrimitiveValue> radii[4])
{
    if (radii[3])
        return;
    if (!radii[2]) {
        if (!radii[1])
            radii[1] = radii[0];
        radii[2] = radii[0];
    }
    radii[3] = radii[1];
}

// FIXME: This should be refactored with CSSParser::parseBorderRadius.
// CSSParser::parseBorderRadius contains support for some legacy radius construction.
PassRefPtrWillBeRawPtr<CSSBasicShape> CSSPropertyParser::parseInsetRoundedCorners(PassRefPtrWillBeRawPtr<CSSBasicShapeInset> shape, CSSParserValueList* args)
{
    CSSParserValue* argument = args->next();

    if (!argument)
        return nullptr;

    CSSParserValueList radiusArguments;
    while (argument) {
        radiusArguments.addValue(*argument);
        argument = args->next();
    }

    unsigned num = radiusArguments.size();
    if (!num || num > 9)
        return nullptr;

    // FIXME: Refactor completeBorderRadii and the array
    RefPtrWillBeRawPtr<CSSPrimitiveValue> radii[2][4];

    unsigned indexAfterSlash = 0;
    for (unsigned i = 0; i < num; ++i) {
        CSSParserValue* value = radiusArguments.valueAt(i);
        if (value->unit == CSSParserValue::Operator) {
            if (value->iValue != '/')
                return nullptr;

            if (!i || indexAfterSlash || i + 1 == num)
                return nullptr;

            indexAfterSlash = i + 1;
            completeBorderRadii(radii[0]);
            continue;
        }

        if (i - indexAfterSlash >= 4)
            return nullptr;

        if (!validUnit(value, FLength | FPercent | FNonNeg))
            return nullptr;

        RefPtrWillBeRawPtr<CSSPrimitiveValue> radius = createPrimitiveNumericValue(value);

        if (!indexAfterSlash)
            radii[0][i] = radius;
        else
            radii[1][i - indexAfterSlash] = radius.release();
    }

    if (!indexAfterSlash) {
        completeBorderRadii(radii[0]);
        for (unsigned i = 0; i < 4; ++i)
            radii[1][i] = radii[0][i];
    } else {
        completeBorderRadii(radii[1]);
    }
    shape->setTopLeftRadius(createPrimitiveValuePair(radii[0][0].release(), radii[1][0].release()));
    shape->setTopRightRadius(createPrimitiveValuePair(radii[0][1].release(), radii[1][1].release()));
    shape->setBottomRightRadius(createPrimitiveValuePair(radii[0][2].release(), radii[1][2].release()));
    shape->setBottomLeftRadius(createPrimitiveValuePair(radii[0][3].release(), radii[1][3].release()));

    return shape;
}

PassRefPtrWillBeRawPtr<CSSBasicShape> CSSPropertyParser::parseBasicShapeInset(CSSParserValueList* args)
{
    ASSERT(args);

    RefPtrWillBeRawPtr<CSSBasicShapeInset> shape = CSSBasicShapeInset::create();

    CSSParserValue* argument = args->current();
    WillBeHeapVector<RefPtrWillBeMember<CSSPrimitiveValue> > widthArguments;
    bool hasRoundedInset = false;

    while (argument) {
        if (argument->unit == CSSPrimitiveValue::CSS_IDENT && equalIgnoringCase(argument->string, "round")) {
            hasRoundedInset = true;
            break;
        }

        Units unitFlags = FLength | FPercent;
        if (!validUnit(argument, unitFlags) || widthArguments.size() > 4)
            return nullptr;

        widthArguments.append(createPrimitiveNumericValue(argument));
        argument = args->next();
    }

    switch (widthArguments.size()) {
    case 1: {
        shape->updateShapeSize1Value(widthArguments[0].get());
        break;
    }
    case 2: {
        shape->updateShapeSize2Values(widthArguments[0].get(), widthArguments[1].get());
        break;
        }
    case 3: {
        shape->updateShapeSize3Values(widthArguments[0].get(), widthArguments[1].get(), widthArguments[2].get());
        break;
    }
    case 4: {
        shape->updateShapeSize4Values(widthArguments[0].get(), widthArguments[1].get(), widthArguments[2].get(), widthArguments[3].get());
        break;
    }
    default:
        return nullptr;
    }

    if (hasRoundedInset)
        return parseInsetRoundedCorners(shape, args);
    return shape;
}

static bool isItemPositionKeyword(CSSValueID id)
{
    return id == CSSValueStart || id == CSSValueEnd || id == CSSValueCenter
        || id == CSSValueSelfStart || id == CSSValueSelfEnd || id == CSSValueFlexStart
        || id == CSSValueFlexEnd || id == CSSValueLeft || id == CSSValueRight;
}

bool CSSPropertyParser::parseItemPositionOverflowPosition(CSSPropertyID propId, bool important)
{
    // auto | baseline | stretch | [<item-position> && <overflow-position>? ]
    // <item-position> = center | start | end | self-start | self-end | flex-start | flex-end | left | right;
    // <overflow-position> = true | safe

    CSSParserValue* value = m_valueList->current();

    if (value->id == CSSValueAuto || value->id == CSSValueBaseline || value->id == CSSValueStretch) {
        if (m_valueList->next())
            return false;

        addProperty(propId, cssValuePool().createIdentifierValue(value->id), important);
        return true;
    }

    RefPtrWillBeRawPtr<CSSPrimitiveValue> position = nullptr;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> overflowAlignmentKeyword = nullptr;
    if (isItemPositionKeyword(value->id)) {
        position = cssValuePool().createIdentifierValue(value->id);
        value = m_valueList->next();
        if (value) {
            if (value->id == CSSValueTrue || value->id == CSSValueSafe)
                overflowAlignmentKeyword = cssValuePool().createIdentifierValue(value->id);
            else
                return false;
        }
    } else if (value->id == CSSValueTrue || value->id == CSSValueSafe) {
        overflowAlignmentKeyword = cssValuePool().createIdentifierValue(value->id);
        value = m_valueList->next();
        if (value) {
            if (isItemPositionKeyword(value->id))
                position = cssValuePool().createIdentifierValue(value->id);
            else
                return false;
        }
    } else {
        return false;
    }

    if (m_valueList->next())
        return false;

    ASSERT(position);
    if (overflowAlignmentKeyword)
        addProperty(propId, createPrimitiveValuePair(position, overflowAlignmentKeyword), important);
    else
        addProperty(propId, position.release(), important);

    return true;
}

PassRefPtrWillBeRawPtr<CSSBasicShape> CSSPropertyParser::parseBasicShapeRectangle(CSSParserValueList* args)
{
    ASSERT(args);

    // rect(x, y, width, height, [[rx], ry])
    if (args->size() != 7 && args->size() != 9 && args->size() != 11)
        return nullptr;

    RefPtrWillBeRawPtr<CSSBasicShapeRectangle> shape = CSSBasicShapeRectangle::create();

    unsigned argumentNumber = 0;
    CSSParserValue* argument = args->current();
    while (argument) {
        Units unitFlags = FLength | FPercent;
        if (argumentNumber > 1) {
            // Arguments width, height, rx, and ry cannot be negative.
            unitFlags = unitFlags | FNonNeg;
        }
        if (!validUnit(argument, unitFlags))
            return nullptr;

        RefPtrWillBeRawPtr<CSSPrimitiveValue> length = createPrimitiveNumericValue(argument);
        ASSERT(argumentNumber < 6);
        switch (argumentNumber) {
        case 0:
            shape->setX(length);
            break;
        case 1:
            shape->setY(length);
            break;
        case 2:
            shape->setWidth(length);
            break;
        case 3:
            shape->setHeight(length);
            break;
        case 4:
            shape->setRadiusX(length);
            break;
        case 5:
            shape->setRadiusY(length);
            break;
        }
        argument = args->next();
        if (argument) {
            if (!isComma(argument))
                return nullptr;

            argument = args->next();
        }
        argumentNumber++;
    }

    if (argumentNumber < 4)
        return nullptr;
    return shape;
}

PassRefPtrWillBeRawPtr<CSSBasicShape> CSSPropertyParser::parseBasicShapeInsetRectangle(CSSParserValueList* args)
{
    ASSERT(args);

    // inset-rectangle(top, right, bottom, left, [[rx], ry])
    if (args->size() != 7 && args->size() != 9 && args->size() != 11)
        return nullptr;

    RefPtrWillBeRawPtr<CSSBasicShapeInsetRectangle> shape = CSSBasicShapeInsetRectangle::create();

    unsigned argumentNumber = 0;
    CSSParserValue* argument = args->current();
    while (argument) {
        Units unitFlags = FLength | FPercent | FNonNeg;
        if (!validUnit(argument, unitFlags))
            return nullptr;

        RefPtrWillBeRawPtr<CSSPrimitiveValue> length = createPrimitiveNumericValue(argument);
        ASSERT(argumentNumber < 6);
        switch (argumentNumber) {
        case 0:
            shape->setTop(length);
            break;
        case 1:
            shape->setRight(length);
            break;
        case 2:
            shape->setBottom(length);
            break;
        case 3:
            shape->setLeft(length);
            break;
        case 4:
            shape->setRadiusX(length);
            break;
        case 5:
            shape->setRadiusY(length);
            break;
        }
        argument = args->next();
        if (argument) {
            if (!isComma(argument))
                return nullptr;

            argument = args->next();
        }
        argumentNumber++;
    }

    if (argumentNumber < 4)
        return nullptr;
    return shape;
}

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> CSSPropertyParser::parseShapeRadius(CSSParserValue* value)
{
    if (value->id == CSSValueClosestSide || value->id == CSSValueFarthestSide)
        return cssValuePool().createIdentifierValue(value->id);

    if (!validUnit(value, FLength | FPercent | FNonNeg))
        return nullptr;

    return createPrimitiveNumericValue(value);
}

PassRefPtrWillBeRawPtr<CSSBasicShape> CSSPropertyParser::parseBasicShapeCircle(CSSParserValueList* args)
{
    ASSERT(args);

    // circle(radius)
    // circle(radius at <position>
    // circle(at <position>)
    // where position defines centerX and centerY using a CSS <position> data type.
    RefPtrWillBeRawPtr<CSSBasicShapeCircle> shape = CSSBasicShapeCircle::create();

    for (CSSParserValue* argument = args->current(); argument; argument = args->next()) {
        // The call to parseFillPosition below should consume all of the
        // arguments except the first two. Thus, and index greater than one
        // indicates an invalid production.
        if (args->currentIndex() > 1)
            return nullptr;

        if (!args->currentIndex() && argument->id != CSSValueAt) {
            if (RefPtrWillBeRawPtr<CSSPrimitiveValue> radius = parseShapeRadius(argument)) {
                shape->setRadius(radius);
                continue;
            }

            return nullptr;
        }

        if (argument->id == CSSValueAt) {
            RefPtrWillBeRawPtr<CSSValue> centerX;
            RefPtrWillBeRawPtr<CSSValue> centerY;
            args->next(); // set list to start of position center
            parseFillPosition(args, centerX, centerY);
            if (centerX && centerY) {
                ASSERT(centerX->isPrimitiveValue());
                ASSERT(centerY->isPrimitiveValue());
                shape->setCenterX(toCSSPrimitiveValue(centerX.get()));
                shape->setCenterY(toCSSPrimitiveValue(centerY.get()));
            } else {
                return nullptr;
            }
        } else {
            return nullptr;
        }
    }

    return shape;
}

PassRefPtrWillBeRawPtr<CSSBasicShape> CSSPropertyParser::parseDeprecatedBasicShapeCircle(CSSParserValueList* args)
{
    ASSERT(args);

    // circle(centerX, centerY, radius)
    if (args->size() != 5)
        return nullptr;

    RefPtrWillBeRawPtr<CSSDeprecatedBasicShapeCircle> shape = CSSDeprecatedBasicShapeCircle::create();

    unsigned argumentNumber = 0;
    CSSParserValue* argument = args->current();
    while (argument) {
        Units unitFlags = FLength | FPercent;
        if (argumentNumber == 2) {
            // Argument radius cannot be negative.
            unitFlags = unitFlags | FNonNeg;
        }

        if (!validUnit(argument, unitFlags))
            return nullptr;

        RefPtrWillBeRawPtr<CSSPrimitiveValue> length = createPrimitiveNumericValue(argument);
        ASSERT(argumentNumber < 3);
        switch (argumentNumber) {
        case 0:
            shape->setCenterX(length);
            break;
        case 1:
            shape->setCenterY(length);
            break;
        case 2:
            shape->setRadius(length);
            break;
        }

        argument = args->next();
        if (argument) {
            if (!isComma(argument))
                return nullptr;
            argument = args->next();
        }
        argumentNumber++;
    }

    if (argumentNumber < 3)
        return nullptr;
    return shape;
}

PassRefPtrWillBeRawPtr<CSSBasicShape> CSSPropertyParser::parseBasicShapeEllipse(CSSParserValueList* args)
{
    ASSERT(args);

    // ellipse(radiusX)
    // ellipse(radiusX at <position>
    // ellipse(radiusX radiusY)
    // ellipse(radiusX radiusY at <position>
    // ellipse(at <position>)
    // where position defines centerX and centerY using a CSS <position> data type.
    RefPtrWillBeRawPtr<CSSBasicShapeEllipse> shape = CSSBasicShapeEllipse::create();

    for (CSSParserValue* argument = args->current(); argument; argument = args->next()) {
        // The call to parseFillPosition below should consume all of the
        // arguments except the first three. Thus, an index greater than two
        // indicates an invalid production.
        if (args->currentIndex() > 2)
            return nullptr;

        if (args->currentIndex() < 2 && argument->id != CSSValueAt) {
            if (RefPtrWillBeRawPtr<CSSPrimitiveValue> radius = parseShapeRadius(argument)) {
                if (!shape->radiusX())
                    shape->setRadiusX(radius);
                else
                    shape->setRadiusY(radius);
                continue;
            }

            return nullptr;
        }

        if (argument->id != CSSValueAt)
            return nullptr;
        RefPtrWillBeRawPtr<CSSValue> centerX;
        RefPtrWillBeRawPtr<CSSValue> centerY;
        args->next(); // set list to start of position center
        parseFillPosition(args, centerX, centerY);
        if (!centerX || !centerY)
            return nullptr;

        ASSERT(centerX->isPrimitiveValue());
        ASSERT(centerY->isPrimitiveValue());
        shape->setCenterX(toCSSPrimitiveValue(centerX.get()));
        shape->setCenterY(toCSSPrimitiveValue(centerY.get()));
    }

    return shape;
}

PassRefPtrWillBeRawPtr<CSSBasicShape> CSSPropertyParser::parseDeprecatedBasicShapeEllipse(CSSParserValueList* args)
{
    ASSERT(args);

    // ellipse(centerX, centerY, radiusX, radiusY)
    if (args->size() != 7)
        return nullptr;

    RefPtrWillBeRawPtr<CSSDeprecatedBasicShapeEllipse> shape = CSSDeprecatedBasicShapeEllipse::create();
    unsigned argumentNumber = 0;
    CSSParserValue* argument = args->current();
    while (argument) {
        Units unitFlags = FLength | FPercent;
        if (argumentNumber > 1) {
            // Arguments radiusX and radiusY cannot be negative.
            unitFlags = unitFlags | FNonNeg;
        }
        if (!validUnit(argument, unitFlags))
            return nullptr;

        RefPtrWillBeRawPtr<CSSPrimitiveValue> length = createPrimitiveNumericValue(argument);
        ASSERT(argumentNumber < 4);
        switch (argumentNumber) {
        case 0:
            shape->setCenterX(length);
            break;
        case 1:
            shape->setCenterY(length);
            break;
        case 2:
            shape->setRadiusX(length);
            break;
        case 3:
            shape->setRadiusY(length);
            break;
        }

        argument = args->next();
        if (argument) {
            if (!isComma(argument))
                return nullptr;
            argument = args->next();
        }
        argumentNumber++;
    }

    if (argumentNumber < 4)
        return nullptr;
    return shape;
}

PassRefPtrWillBeRawPtr<CSSBasicShape> CSSPropertyParser::parseBasicShapePolygon(CSSParserValueList* args)
{
    ASSERT(args);

    unsigned size = args->size();
    if (!size)
        return nullptr;

    RefPtrWillBeRawPtr<CSSBasicShapePolygon> shape = CSSBasicShapePolygon::create();

    CSSParserValue* argument = args->current();
    if (argument->id == CSSValueEvenodd || argument->id == CSSValueNonzero) {
        shape->setWindRule(argument->id == CSSValueEvenodd ? RULE_EVENODD : RULE_NONZERO);

        if (!isComma(args->next()))
            return nullptr;

        argument = args->next();
        size -= 2;
    }

    // <length> <length>, ... <length> <length> -> each pair has 3 elements except the last one
    if (!size || (size % 3) - 2)
        return nullptr;

    CSSParserValue* argumentX = argument;
    while (argumentX) {
        if (!validUnit(argumentX, FLength | FPercent))
            return nullptr;

        CSSParserValue* argumentY = args->next();
        if (!argumentY || !validUnit(argumentY, FLength | FPercent))
            return nullptr;

        RefPtrWillBeRawPtr<CSSPrimitiveValue> xLength = createPrimitiveNumericValue(argumentX);
        RefPtrWillBeRawPtr<CSSPrimitiveValue> yLength = createPrimitiveNumericValue(argumentY);

        shape->appendPoint(xLength.release(), yLength.release());

        CSSParserValue* commaOrNull = args->next();
        if (!commaOrNull)
            argumentX = 0;
        else if (!isComma(commaOrNull))
            return nullptr;
        else
            argumentX = args->next();
    }

    return shape;
}

static bool isBoxValue(CSSValueID valueId)
{
    switch (valueId) {
    case CSSValueContentBox:
    case CSSValuePaddingBox:
    case CSSValueBorderBox:
    case CSSValueMarginBox:
        return true;
    default:
        break;
    }

    return false;
}

// FIXME This function is temporary to allow for an orderly transition between
// the new CSS Shapes circle and ellipse syntax. It will be removed when the
// old syntax is removed.
static bool isDeprecatedBasicShape(CSSParserValueList* args)
{
    for (unsigned i = args->currentIndex(); i < args->size(); ++i) {
        CSSParserValue* value = args->valueAt(i);
        if (isComma(value))
            return true;
    }

    return false;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseShapeProperty(CSSPropertyID propId)
{
    if (!RuntimeEnabledFeatures::cssShapesEnabled())
        return nullptr;

    CSSParserValue* value = m_valueList->current();
    CSSValueID valueId = value->id;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> boxValue;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> shapeValue;

    if (valueId == CSSValueNone
        || (valueId == CSSValueOutsideShape && propId == CSSPropertyShapeInside)) {
        RefPtrWillBeRawPtr<CSSPrimitiveValue> keywordValue = parseValidPrimitive(valueId, value);
        m_valueList->next();
        return keywordValue.release();
    }

    RefPtrWillBeRawPtr<CSSValue> imageValue;
    if (valueId != CSSValueNone && parseFillImage(m_valueList.get(), imageValue)) {
        m_valueList->next();
        return imageValue.release();
    }

    if (value->unit == CSSParserValue::Function) {
        shapeValue = parseBasicShape();
        if (!shapeValue)
            return nullptr;
    } else if (isBoxValue(valueId)) {
        boxValue = parseValidPrimitive(valueId, value);
        m_valueList->next();
    } else {
        return nullptr;
    }

    ASSERT(shapeValue || boxValue);
    value = m_valueList->current();

    if (value) {
        valueId = value->id;
        if (boxValue && value->unit == CSSParserValue::Function) {
            shapeValue = parseBasicShape();
            if (!shapeValue)
                return nullptr;
        } else if (shapeValue && isBoxValue(valueId)) {
            boxValue = parseValidPrimitive(valueId, value);
            m_valueList->next();
        } else {
            return nullptr;
        }

        ASSERT(shapeValue && boxValue);
        shapeValue->getShapeValue()->setLayoutBox(boxValue.release());
    }

    if (shapeValue)
        return shapeValue.release();
    return boxValue.release();
}

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> CSSPropertyParser::parseBasicShape()
{
    CSSParserValue* value = m_valueList->current();
    ASSERT(value->unit == CSSParserValue::Function);
    CSSParserValueList* args = value->function->args.get();

    if (!args)
        return nullptr;

    RefPtrWillBeRawPtr<CSSBasicShape> shape;
    if (equalIgnoringCase(value->function->name, "rectangle("))
        shape = parseBasicShapeRectangle(args);
    else if (equalIgnoringCase(value->function->name, "circle("))
        if (isDeprecatedBasicShape(args))
            shape = parseDeprecatedBasicShapeCircle(args);
        else
            shape = parseBasicShapeCircle(args);
    else if (equalIgnoringCase(value->function->name, "ellipse("))
        if (isDeprecatedBasicShape(args))
            shape = parseDeprecatedBasicShapeEllipse(args);
        else
            shape = parseBasicShapeEllipse(args);
    else if (equalIgnoringCase(value->function->name, "polygon("))
        shape = parseBasicShapePolygon(args);
    else if (equalIgnoringCase(value->function->name, "inset-rectangle("))
        shape = parseBasicShapeInsetRectangle(args);
    else if (equalIgnoringCase(value->function->name, "inset("))
        shape = parseBasicShapeInset(args);

    if (!shape)
        return nullptr;

    m_valueList->next();
    return cssValuePool().createValue(shape.release());
}

// [ 'font-style' || 'font-variant' || 'font-weight' ]? 'font-size' [ / 'line-height' ]? 'font-family'
bool CSSPropertyParser::parseFont(bool important)
{
    // Let's check if there is an inherit or initial somewhere in the shorthand.
    for (unsigned i = 0; i < m_valueList->size(); ++i) {
        if (m_valueList->valueAt(i)->id == CSSValueInherit || m_valueList->valueAt(i)->id == CSSValueInitial)
            return false;
    }

    ShorthandScope scope(this, CSSPropertyFont);
    // Optional font-style, font-variant and font-weight.
    bool fontStyleParsed = false;
    bool fontVariantParsed = false;
    bool fontWeightParsed = false;
    CSSParserValue* value;
    while ((value = m_valueList->current())) {
        if (!fontStyleParsed && isValidKeywordPropertyAndValue(CSSPropertyFontStyle, value->id, m_context)) {
            addProperty(CSSPropertyFontStyle, cssValuePool().createIdentifierValue(value->id), important);
            fontStyleParsed = true;
        } else if (!fontVariantParsed && (value->id == CSSValueNormal || value->id == CSSValueSmallCaps)) {
            // Font variant in the shorthand is particular, it only accepts normal or small-caps.
            addProperty(CSSPropertyFontVariant, cssValuePool().createIdentifierValue(value->id), important);
            fontVariantParsed = true;
        } else if (!fontWeightParsed && parseFontWeight(important))
            fontWeightParsed = true;
        else
            break;
        m_valueList->next();
    }

    if (!value)
        return false;

    if (!fontStyleParsed)
        addProperty(CSSPropertyFontStyle, cssValuePool().createIdentifierValue(CSSValueNormal), important, true);
    if (!fontVariantParsed)
        addProperty(CSSPropertyFontVariant, cssValuePool().createIdentifierValue(CSSValueNormal), important, true);
    if (!fontWeightParsed)
        addProperty(CSSPropertyFontWeight, cssValuePool().createIdentifierValue(CSSValueNormal), important, true);

    // Now a font size _must_ come.
    // <absolute-size> | <relative-size> | <length> | <percentage> | inherit
    if (!parseFontSize(important))
        return false;

    value = m_valueList->current();
    if (!value)
        return false;

    if (isForwardSlashOperator(value)) {
        // The line-height property.
        value = m_valueList->next();
        if (!value)
            return false;
        if (!parseLineHeight(important))
            return false;
    } else
        addProperty(CSSPropertyLineHeight, cssValuePool().createIdentifierValue(CSSValueNormal), important, true);

    // Font family must come now.
    RefPtrWillBeRawPtr<CSSValue> parsedFamilyValue = parseFontFamily();
    if (!parsedFamilyValue)
        return false;

    addProperty(CSSPropertyFontFamily, parsedFamilyValue.release(), important);

    // FIXME: http://www.w3.org/TR/2011/WD-css3-fonts-20110324/#font-prop requires that
    // "font-stretch", "font-size-adjust", and "font-kerning" be reset to their initial values
    // but we don't seem to support them at the moment. They should also be added here once implemented.
    if (m_valueList->current())
        return false;

    return true;
}

class FontFamilyValueBuilder {
    DISALLOW_ALLOCATION();
public:
    FontFamilyValueBuilder(CSSValueList* list)
        : m_list(list)
    {
    }

    void add(const CSSParserString& string)
    {
        if (!m_builder.isEmpty())
            m_builder.append(' ');

        if (string.is8Bit()) {
            m_builder.append(string.characters8(), string.length());
            return;
        }

        m_builder.append(string.characters16(), string.length());
    }

    void commit()
    {
        if (m_builder.isEmpty())
            return;
        m_list->append(cssValuePool().createFontFamilyValue(m_builder.toString()));
        m_builder.clear();
    }

private:
    StringBuilder m_builder;
    CSSValueList* m_list;
};

PassRefPtrWillBeRawPtr<CSSValueList> CSSPropertyParser::parseFontFamily()
{
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    CSSParserValue* value = m_valueList->current();

    FontFamilyValueBuilder familyBuilder(list.get());
    bool inFamily = false;

    while (value) {
        CSSParserValue* nextValue = m_valueList->next();
        bool nextValBreaksFont = !nextValue ||
                                 (nextValue->unit == CSSParserValue::Operator && nextValue->iValue == ',');
        bool nextValIsFontName = nextValue &&
            ((nextValue->id >= CSSValueSerif && nextValue->id <= CSSValueWebkitBody) ||
            (nextValue->unit == CSSPrimitiveValue::CSS_STRING || nextValue->unit == CSSPrimitiveValue::CSS_IDENT));

        bool valueIsKeyword = value->id == CSSValueInitial || value->id == CSSValueInherit || value->id == CSSValueDefault;
        if (valueIsKeyword && !inFamily) {
            if (nextValBreaksFont)
                value = m_valueList->next();
            else if (nextValIsFontName)
                value = nextValue;
            continue;
        }

        if (value->id >= CSSValueSerif && value->id <= CSSValueWebkitBody) {
            if (inFamily)
                familyBuilder.add(value->string);
            else if (nextValBreaksFont || !nextValIsFontName)
                list->append(cssValuePool().createIdentifierValue(value->id));
            else {
                familyBuilder.commit();
                familyBuilder.add(value->string);
                inFamily = true;
            }
        } else if (value->unit == CSSPrimitiveValue::CSS_STRING) {
            // Strings never share in a family name.
            inFamily = false;
            familyBuilder.commit();
            list->append(cssValuePool().createFontFamilyValue(value->string));
        } else if (value->unit == CSSPrimitiveValue::CSS_IDENT) {
            if (inFamily)
                familyBuilder.add(value->string);
            else if (nextValBreaksFont || !nextValIsFontName)
                list->append(cssValuePool().createFontFamilyValue(value->string));
            else {
                familyBuilder.commit();
                familyBuilder.add(value->string);
                inFamily = true;
            }
        } else {
            break;
        }

        if (!nextValue)
            break;

        if (nextValBreaksFont) {
            value = m_valueList->next();
            familyBuilder.commit();
            inFamily = false;
        }
        else if (nextValIsFontName)
            value = nextValue;
        else
            break;
    }
    familyBuilder.commit();

    if (!list->length())
        list = nullptr;
    return list.release();
}

bool CSSPropertyParser::parseLineHeight(bool important)
{
    CSSParserValue* value = m_valueList->current();
    CSSValueID id = value->id;
    bool validPrimitive = false;
    // normal | <number> | <length> | <percentage> | inherit
    if (id == CSSValueNormal)
        validPrimitive = true;
    else
        validPrimitive = (!id && validUnit(value, FNumber | FLength | FPercent | FNonNeg));
    if (validPrimitive && (!m_valueList->next() || inShorthand()))
        addProperty(CSSPropertyLineHeight, parseValidPrimitive(id, value), important);
    return validPrimitive;
}

bool CSSPropertyParser::parseFontSize(bool important)
{
    CSSParserValue* value = m_valueList->current();
    CSSValueID id = value->id;
    bool validPrimitive = false;
    // <absolute-size> | <relative-size> | <length> | <percentage> | inherit
    if (id >= CSSValueXxSmall && id <= CSSValueLarger)
        validPrimitive = true;
    else
        validPrimitive = validUnit(value, FLength | FPercent | FNonNeg);
    if (validPrimitive && (!m_valueList->next() || inShorthand()))
        addProperty(CSSPropertyFontSize, parseValidPrimitive(id, value), important);
    return validPrimitive;
}

bool CSSPropertyParser::parseFontVariant(bool important)
{
    RefPtrWillBeRawPtr<CSSValueList> values;
    if (m_valueList->size() > 1)
        values = CSSValueList::createCommaSeparated();
    CSSParserValue* val;
    bool expectComma = false;
    while ((val = m_valueList->current())) {
        RefPtrWillBeRawPtr<CSSPrimitiveValue> parsedValue;
        if (!expectComma) {
            expectComma = true;
            if (val->id == CSSValueNormal || val->id == CSSValueSmallCaps)
                parsedValue = cssValuePool().createIdentifierValue(val->id);
            else if (val->id == CSSValueAll && !values) {
                // 'all' is only allowed in @font-face and with no other values. Make a value list to
                // indicate that we are in the @font-face case.
                values = CSSValueList::createCommaSeparated();
                parsedValue = cssValuePool().createIdentifierValue(val->id);
            }
        } else if (val->unit == CSSParserValue::Operator && val->iValue == ',') {
            expectComma = false;
            m_valueList->next();
            continue;
        }

        if (!parsedValue)
            return false;

        m_valueList->next();

        if (values)
            values->append(parsedValue.release());
        else {
            addProperty(CSSPropertyFontVariant, parsedValue.release(), important);
            return true;
        }
    }

    if (values && values->length()) {
        m_hasFontFaceOnlyValues = true;
        addProperty(CSSPropertyFontVariant, values.release(), important);
        return true;
    }

    return false;
}

bool CSSPropertyParser::parseFontWeight(bool important)
{
    CSSParserValue* value = m_valueList->current();
    if ((value->id >= CSSValueNormal) && (value->id <= CSSValue900)) {
        addProperty(CSSPropertyFontWeight, cssValuePool().createIdentifierValue(value->id), important);
        return true;
    }
    if (validUnit(value, FInteger | FNonNeg, HTMLQuirksMode)) {
        int weight = static_cast<int>(value->fValue);
        if (!(weight % 100) && weight >= 100 && weight <= 900) {
            addProperty(CSSPropertyFontWeight, cssValuePool().createIdentifierValue(static_cast<CSSValueID>(CSSValue100 + weight / 100 - 1)), important);
            return true;
        }
    }
    return false;
}

bool CSSPropertyParser::parseFontFaceSrcURI(CSSValueList* valueList)
{
    RefPtrWillBeRawPtr<CSSFontFaceSrcValue> uriValue(CSSFontFaceSrcValue::create(completeURL(m_valueList->current()->string)));

    CSSParserValue* value = m_valueList->next();
    if (!value) {
        valueList->append(uriValue.release());
        return true;
    }
    if (value->unit == CSSParserValue::Operator && value->iValue == ',') {
        m_valueList->next();
        valueList->append(uriValue.release());
        return true;
    }

    if (value->unit != CSSParserValue::Function || !equalIgnoringCase(value->function->name, "format("))
        return false;

    // FIXME: http://www.w3.org/TR/2011/WD-css3-fonts-20111004/ says that format() contains a comma-separated list of strings,
    // but CSSFontFaceSrcValue stores only one format. Allowing one format for now.
    CSSParserValueList* args = value->function->args.get();
    if (!args || args->size() != 1 || (args->current()->unit != CSSPrimitiveValue::CSS_STRING && args->current()->unit != CSSPrimitiveValue::CSS_IDENT))
        return false;
    uriValue->setFormat(args->current()->string);
    valueList->append(uriValue.release());
    value = m_valueList->next();
    if (value && value->unit == CSSParserValue::Operator && value->iValue == ',')
        m_valueList->next();
    return true;
}

bool CSSPropertyParser::parseFontFaceSrcLocal(CSSValueList* valueList)
{
    CSSParserValueList* args = m_valueList->current()->function->args.get();
    if (!args || !args->size())
        return false;

    if (args->size() == 1 && args->current()->unit == CSSPrimitiveValue::CSS_STRING)
        valueList->append(CSSFontFaceSrcValue::createLocal(args->current()->string));
    else if (args->current()->unit == CSSPrimitiveValue::CSS_IDENT) {
        StringBuilder builder;
        for (CSSParserValue* localValue = args->current(); localValue; localValue = args->next()) {
            if (localValue->unit != CSSPrimitiveValue::CSS_IDENT)
                return false;
            if (!builder.isEmpty())
                builder.append(' ');
            builder.append(localValue->string);
        }
        valueList->append(CSSFontFaceSrcValue::createLocal(builder.toString()));
    } else
        return false;

    if (CSSParserValue* value = m_valueList->next()) {
        if (value->unit == CSSParserValue::Operator && value->iValue == ',')
            m_valueList->next();
    }
    return true;
}

bool CSSPropertyParser::parseFontFaceSrc()
{
    RefPtrWillBeRawPtr<CSSValueList> values(CSSValueList::createCommaSeparated());

    while (CSSParserValue* value = m_valueList->current()) {
        if (value->unit == CSSPrimitiveValue::CSS_URI) {
            if (!parseFontFaceSrcURI(values.get()))
                return false;
        } else if (value->unit == CSSParserValue::Function && equalIgnoringCase(value->function->name, "local(")) {
            if (!parseFontFaceSrcLocal(values.get()))
                return false;
        } else
            return false;
    }
    if (!values->length())
        return false;

    addProperty(CSSPropertySrc, values.release(), m_important);
    m_valueList->next();
    return true;
}

bool CSSPropertyParser::parseFontFaceUnicodeRange()
{
    RefPtrWillBeRawPtr<CSSValueList> values = CSSValueList::createCommaSeparated();
    bool failed = false;
    bool operatorExpected = false;
    for (; m_valueList->current(); m_valueList->next(), operatorExpected = !operatorExpected) {
        if (operatorExpected) {
            if (m_valueList->current()->unit == CSSParserValue::Operator && m_valueList->current()->iValue == ',')
                continue;
            failed = true;
            break;
        }
        if (m_valueList->current()->unit != CSSPrimitiveValue::CSS_UNICODE_RANGE) {
            failed = true;
            break;
        }

        String rangeString = m_valueList->current()->string;
        UChar32 from = 0;
        UChar32 to = 0;
        unsigned length = rangeString.length();

        if (length < 3) {
            failed = true;
            break;
        }

        unsigned i = 2;
        while (i < length) {
            UChar c = rangeString[i];
            if (c == '-' || c == '?')
                break;
            from *= 16;
            if (c >= '0' && c <= '9')
                from += c - '0';
            else if (c >= 'A' && c <= 'F')
                from += 10 + c - 'A';
            else if (c >= 'a' && c <= 'f')
                from += 10 + c - 'a';
            else {
                failed = true;
                break;
            }
            i++;
        }
        if (failed)
            break;

        if (i == length)
            to = from;
        else if (rangeString[i] == '?') {
            unsigned span = 1;
            while (i < length && rangeString[i] == '?') {
                span *= 16;
                from *= 16;
                i++;
            }
            if (i < length)
                failed = true;
            to = from + span - 1;
        } else {
            if (length < i + 2) {
                failed = true;
                break;
            }
            i++;
            while (i < length) {
                UChar c = rangeString[i];
                to *= 16;
                if (c >= '0' && c <= '9')
                    to += c - '0';
                else if (c >= 'A' && c <= 'F')
                    to += 10 + c - 'A';
                else if (c >= 'a' && c <= 'f')
                    to += 10 + c - 'a';
                else {
                    failed = true;
                    break;
                }
                i++;
            }
            if (failed)
                break;
        }
        if (from <= to)
            values->append(CSSUnicodeRangeValue::create(from, to));
    }
    if (failed || !values->length())
        return false;
    addProperty(CSSPropertyUnicodeRange, values.release(), m_important);
    return true;
}

// Returns the number of characters which form a valid double
// and are terminated by the given terminator character
template <typename CharacterType>
static int checkForValidDouble(const CharacterType* string, const CharacterType* end, const char terminator)
{
    int length = end - string;
    if (length < 1)
        return 0;

    bool decimalMarkSeen = false;
    int processedLength = 0;

    for (int i = 0; i < length; ++i) {
        if (string[i] == terminator) {
            processedLength = i;
            break;
        }
        if (!isASCIIDigit(string[i])) {
            if (!decimalMarkSeen && string[i] == '.')
                decimalMarkSeen = true;
            else
                return 0;
        }
    }

    if (decimalMarkSeen && processedLength == 1)
        return 0;

    return processedLength;
}

// Returns the number of characters consumed for parsing a valid double
// terminated by the given terminator character
template <typename CharacterType>
static int parseDouble(const CharacterType* string, const CharacterType* end, const char terminator, double& value)
{
    int length = checkForValidDouble(string, end, terminator);
    if (!length)
        return 0;

    int position = 0;
    double localValue = 0;

    // The consumed characters here are guaranteed to be
    // ASCII digits with or without a decimal mark
    for (; position < length; ++position) {
        if (string[position] == '.')
            break;
        localValue = localValue * 10 + string[position] - '0';
    }

    if (++position == length) {
        value = localValue;
        return length;
    }

    double fraction = 0;
    double scale = 1;

    while (position < length && scale < MAX_SCALE) {
        fraction = fraction * 10 + string[position++] - '0';
        scale *= 10;
    }

    value = localValue + fraction / scale;
    return length;
}

template <typename CharacterType>
static bool parseColorIntOrPercentage(const CharacterType*& string, const CharacterType* end, const char terminator, CSSPrimitiveValue::UnitTypes& expect, int& value)
{
    const CharacterType* current = string;
    double localValue = 0;
    bool negative = false;
    while (current != end && isHTMLSpace<CharacterType>(*current))
        current++;
    if (current != end && *current == '-') {
        negative = true;
        current++;
    }
    if (current == end || !isASCIIDigit(*current))
        return false;
    while (current != end && isASCIIDigit(*current)) {
        double newValue = localValue * 10 + *current++ - '0';
        if (newValue >= 255) {
            // Clamp values at 255.
            localValue = 255;
            while (current != end && isASCIIDigit(*current))
                ++current;
            break;
        }
        localValue = newValue;
    }

    if (current == end)
        return false;

    if (expect == CSSPrimitiveValue::CSS_NUMBER && (*current == '.' || *current == '%'))
        return false;

    if (*current == '.') {
        // We already parsed the integral part, try to parse
        // the fraction part of the percentage value.
        double percentage = 0;
        int numCharactersParsed = parseDouble(current, end, '%', percentage);
        if (!numCharactersParsed)
            return false;
        current += numCharactersParsed;
        if (*current != '%')
            return false;
        localValue += percentage;
    }

    if (expect == CSSPrimitiveValue::CSS_PERCENTAGE && *current != '%')
        return false;

    if (*current == '%') {
        expect = CSSPrimitiveValue::CSS_PERCENTAGE;
        localValue = localValue / 100.0 * 256.0;
        // Clamp values at 255 for percentages over 100%
        if (localValue > 255)
            localValue = 255;
        current++;
    } else
        expect = CSSPrimitiveValue::CSS_NUMBER;

    while (current != end && isHTMLSpace<CharacterType>(*current))
        current++;
    if (current == end || *current++ != terminator)
        return false;
    // Clamp negative values at zero.
    value = negative ? 0 : static_cast<int>(localValue);
    string = current;
    return true;
}

template <typename CharacterType>
static inline bool isTenthAlpha(const CharacterType* string, const int length)
{
    // "0.X"
    if (length == 3 && string[0] == '0' && string[1] == '.' && isASCIIDigit(string[2]))
        return true;

    // ".X"
    if (length == 2 && string[0] == '.' && isASCIIDigit(string[1]))
        return true;

    return false;
}

template <typename CharacterType>
static inline bool parseAlphaValue(const CharacterType*& string, const CharacterType* end, const char terminator, int& value)
{
    while (string != end && isHTMLSpace<CharacterType>(*string))
        string++;

    bool negative = false;

    if (string != end && *string == '-') {
        negative = true;
        string++;
    }

    value = 0;

    int length = end - string;
    if (length < 2)
        return false;

    if (string[length - 1] != terminator || !isASCIIDigit(string[length - 2]))
        return false;

    if (string[0] != '0' && string[0] != '1' && string[0] != '.') {
        if (checkForValidDouble(string, end, terminator)) {
            value = negative ? 0 : 255;
            string = end;
            return true;
        }
        return false;
    }

    if (length == 2 && string[0] != '.') {
        value = !negative && string[0] == '1' ? 255 : 0;
        string = end;
        return true;
    }

    if (isTenthAlpha(string, length - 1)) {
        static const int tenthAlphaValues[] = { 0, 25, 51, 76, 102, 127, 153, 179, 204, 230 };
        value = negative ? 0 : tenthAlphaValues[string[length - 2] - '0'];
        string = end;
        return true;
    }

    double alpha = 0;
    if (!parseDouble(string, end, terminator, alpha))
        return false;
    value = negative ? 0 : static_cast<int>(alpha * nextafter(256.0, 0.0));
    string = end;
    return true;
}

template <typename CharacterType>
static inline bool mightBeRGBA(const CharacterType* characters, unsigned length)
{
    if (length < 5)
        return false;
    return characters[4] == '('
        && isASCIIAlphaCaselessEqual(characters[0], 'r')
        && isASCIIAlphaCaselessEqual(characters[1], 'g')
        && isASCIIAlphaCaselessEqual(characters[2], 'b')
        && isASCIIAlphaCaselessEqual(characters[3], 'a');
}

template <typename CharacterType>
static inline bool mightBeRGB(const CharacterType* characters, unsigned length)
{
    if (length < 4)
        return false;
    return characters[3] == '('
        && isASCIIAlphaCaselessEqual(characters[0], 'r')
        && isASCIIAlphaCaselessEqual(characters[1], 'g')
        && isASCIIAlphaCaselessEqual(characters[2], 'b');
}

template <typename CharacterType>
static inline bool fastParseColorInternal(RGBA32& rgb, const CharacterType* characters, unsigned length , bool strict)
{
    CSSPrimitiveValue::UnitTypes expect = CSSPrimitiveValue::CSS_UNKNOWN;

    if (!strict && length >= 3) {
        if (characters[0] == '#') {
            if (Color::parseHexColor(characters + 1, length - 1, rgb))
                return true;
        } else {
            if (Color::parseHexColor(characters, length, rgb))
                return true;
        }
    }

    // Try rgba() syntax.
    if (mightBeRGBA(characters, length)) {
        const CharacterType* current = characters + 5;
        const CharacterType* end = characters + length;
        int red;
        int green;
        int blue;
        int alpha;

        if (!parseColorIntOrPercentage(current, end, ',', expect, red))
            return false;
        if (!parseColorIntOrPercentage(current, end, ',', expect, green))
            return false;
        if (!parseColorIntOrPercentage(current, end, ',', expect, blue))
            return false;
        if (!parseAlphaValue(current, end, ')', alpha))
            return false;
        if (current != end)
            return false;
        rgb = makeRGBA(red, green, blue, alpha);
        return true;
    }

    // Try rgb() syntax.
    if (mightBeRGB(characters, length)) {
        const CharacterType* current = characters + 4;
        const CharacterType* end = characters + length;
        int red;
        int green;
        int blue;
        if (!parseColorIntOrPercentage(current, end, ',', expect, red))
            return false;
        if (!parseColorIntOrPercentage(current, end, ',', expect, green))
            return false;
        if (!parseColorIntOrPercentage(current, end, ')', expect, blue))
            return false;
        if (current != end)
            return false;
        rgb = makeRGB(red, green, blue);
        return true;
    }

    return false;
}

template<typename StringType>
bool CSSPropertyParser::fastParseColor(RGBA32& rgb, const StringType& name, bool strict)
{
    unsigned length = name.length();
    bool parseResult;

    if (!length)
        return false;

    if (name.is8Bit())
        parseResult = fastParseColorInternal(rgb, name.characters8(), length, strict);
    else
        parseResult = fastParseColorInternal(rgb, name.characters16(), length, strict);

    if (parseResult)
        return true;

    // Try named colors.
    Color tc;
    if (!tc.setNamedColor(name))
        return false;
    rgb = tc.rgb();
    return true;
}

inline double CSSPropertyParser::parsedDouble(CSSParserValue *v, ReleaseParsedCalcValueCondition releaseCalc)
{
    const double result = m_parsedCalculation ? m_parsedCalculation->doubleValue() : v->fValue;
    if (releaseCalc == ReleaseParsedCalcValue)
        m_parsedCalculation.release();
    return result;
}

bool CSSPropertyParser::isCalculation(CSSParserValue* value)
{
    return (value->unit == CSSParserValue::Function)
        && (equalIgnoringCase(value->function->name, "calc(")
            || equalIgnoringCase(value->function->name, "-webkit-calc(")
            || equalIgnoringCase(value->function->name, "-webkit-min(")
            || equalIgnoringCase(value->function->name, "-webkit-max("));
}

inline int CSSPropertyParser::colorIntFromValue(CSSParserValue* v)
{
    bool isPercent;

    if (m_parsedCalculation)
        isPercent = m_parsedCalculation->category() == CalcPercent;
    else
        isPercent = v->unit == CSSPrimitiveValue::CSS_PERCENTAGE;

    const double value = parsedDouble(v, ReleaseParsedCalcValue);

    if (value <= 0.0)
        return 0;

    if (isPercent) {
        if (value >= 100.0)
            return 255;
        return static_cast<int>(value * 256.0 / 100.0);
    }

    if (value >= 255.0)
        return 255;

    return static_cast<int>(value);
}

bool CSSPropertyParser::parseColorParameters(CSSParserValue* value, int* colorArray, bool parseAlpha)
{
    CSSParserValueList* args = value->function->args.get();
    CSSParserValue* v = args->current();
    Units unitType = FUnknown;
    // Get the first value and its type
    if (validUnit(v, FInteger, HTMLStandardMode))
        unitType = FInteger;
    else if (validUnit(v, FPercent, HTMLStandardMode))
        unitType = FPercent;
    else
        return false;

    colorArray[0] = colorIntFromValue(v);
    for (int i = 1; i < 3; i++) {
        v = args->next();
        if (v->unit != CSSParserValue::Operator && v->iValue != ',')
            return false;
        v = args->next();
        if (!validUnit(v, unitType, HTMLStandardMode))
            return false;
        colorArray[i] = colorIntFromValue(v);
    }
    if (parseAlpha) {
        v = args->next();
        if (v->unit != CSSParserValue::Operator && v->iValue != ',')
            return false;
        v = args->next();
        if (!validUnit(v, FNumber, HTMLStandardMode))
            return false;
        const double value = parsedDouble(v, ReleaseParsedCalcValue);
        // Convert the floating pointer number of alpha to an integer in the range [0, 256),
        // with an equal distribution across all 256 values.
        colorArray[3] = static_cast<int>(max(0.0, min(1.0, value)) * nextafter(256.0, 0.0));
    }
    return true;
}

// The CSS3 specification defines the format of a HSL color as
// hsl(<number>, <percent>, <percent>)
// and with alpha, the format is
// hsla(<number>, <percent>, <percent>, <number>)
// The first value, HUE, is in an angle with a value between 0 and 360
bool CSSPropertyParser::parseHSLParameters(CSSParserValue* value, double* colorArray, bool parseAlpha)
{
    CSSParserValueList* args = value->function->args.get();
    CSSParserValue* v = args->current();
    // Get the first value
    if (!validUnit(v, FNumber, HTMLStandardMode))
        return false;
    // normalize the Hue value and change it to be between 0 and 1.0
    colorArray[0] = (((static_cast<int>(parsedDouble(v, ReleaseParsedCalcValue)) % 360) + 360) % 360) / 360.0;
    for (int i = 1; i < 3; i++) {
        v = args->next();
        if (v->unit != CSSParserValue::Operator && v->iValue != ',')
            return false;
        v = args->next();
        if (!validUnit(v, FPercent, HTMLStandardMode))
            return false;
        colorArray[i] = max(0.0, min(100.0, parsedDouble(v, ReleaseParsedCalcValue))) / 100.0; // needs to be value between 0 and 1.0
    }
    if (parseAlpha) {
        v = args->next();
        if (v->unit != CSSParserValue::Operator && v->iValue != ',')
            return false;
        v = args->next();
        if (!validUnit(v, FNumber, HTMLStandardMode))
            return false;
        colorArray[3] = max(0.0, min(1.0, parsedDouble(v, ReleaseParsedCalcValue)));
    }
    return true;
}

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> CSSPropertyParser::parseColor(CSSParserValue* value)
{
    RGBA32 c = Color::transparent;
    if (!parseColorFromValue(value ? value : m_valueList->current(), c))
        return nullptr;
    return cssValuePool().createColorValue(c);
}

bool CSSPropertyParser::parseColorFromValue(CSSParserValue* value, RGBA32& c)
{
    if (inQuirksMode() && value->unit == CSSPrimitiveValue::CSS_NUMBER
        && value->fValue >= 0. && value->fValue < 1000000.) {
        String str = String::format("%06d", static_cast<int>((value->fValue+.5)));
        // FIXME: This should be strict parsing for SVG as well.
        if (!fastParseColor(c, str, !inQuirksMode()))
            return false;
    } else if (value->unit == CSSPrimitiveValue::CSS_PARSER_HEXCOLOR ||
                value->unit == CSSPrimitiveValue::CSS_IDENT ||
                (inQuirksMode() && value->unit == CSSPrimitiveValue::CSS_DIMENSION)) {
        if (!fastParseColor(c, value->string, !inQuirksMode() && value->unit == CSSPrimitiveValue::CSS_IDENT))
            return false;
    } else if (value->unit == CSSParserValue::Function &&
                value->function->args != 0 &&
                value->function->args->size() == 5 /* rgb + two commas */ &&
                equalIgnoringCase(value->function->name, "rgb(")) {
        int colorValues[3];
        if (!parseColorParameters(value, colorValues, false))
            return false;
        c = makeRGB(colorValues[0], colorValues[1], colorValues[2]);
    } else {
        if (value->unit == CSSParserValue::Function &&
                value->function->args != 0 &&
                value->function->args->size() == 7 /* rgba + three commas */ &&
                equalIgnoringCase(value->function->name, "rgba(")) {
            int colorValues[4];
            if (!parseColorParameters(value, colorValues, true))
                return false;
            c = makeRGBA(colorValues[0], colorValues[1], colorValues[2], colorValues[3]);
        } else if (value->unit == CSSParserValue::Function &&
                    value->function->args != 0 &&
                    value->function->args->size() == 5 /* hsl + two commas */ &&
                    equalIgnoringCase(value->function->name, "hsl(")) {
            double colorValues[3];
            if (!parseHSLParameters(value, colorValues, false))
                return false;
            c = makeRGBAFromHSLA(colorValues[0], colorValues[1], colorValues[2], 1.0);
        } else if (value->unit == CSSParserValue::Function &&
                    value->function->args != 0 &&
                    value->function->args->size() == 7 /* hsla + three commas */ &&
                    equalIgnoringCase(value->function->name, "hsla(")) {
            double colorValues[4];
            if (!parseHSLParameters(value, colorValues, true))
                return false;
            c = makeRGBAFromHSLA(colorValues[0], colorValues[1], colorValues[2], colorValues[3]);
        } else
            return false;
    }

    return true;
}

// This class tracks parsing state for shadow values.  If it goes out of scope (e.g., due to an early return)
// without the allowBreak bit being set, then it will clean up all of the objects and destroy them.
class ShadowParseContext {
    DISALLOW_ALLOCATION();
public:
    ShadowParseContext(CSSPropertyID prop, CSSPropertyParser* parser)
        : property(prop)
        , m_parser(parser)
        , allowX(true)
        , allowY(false)
        , allowBlur(false)
        , allowSpread(false)
        , allowColor(true)
        , allowStyle(prop == CSSPropertyWebkitBoxShadow || prop == CSSPropertyBoxShadow)
        , allowBreak(true)
    {
    }

    bool allowLength() { return allowX || allowY || allowBlur || allowSpread; }

    void commitValue()
    {
        // Handle the ,, case gracefully by doing nothing.
        if (x || y || blur || spread || color || style) {
            if (!values)
                values = CSSValueList::createCommaSeparated();

            // Construct the current shadow value and add it to the list.
            values->append(CSSShadowValue::create(x.release(), y.release(), blur.release(), spread.release(), style.release(), color.release()));
        }

        // Now reset for the next shadow value.
        x = nullptr;
        y = nullptr;
        blur = nullptr;
        spread = nullptr;
        style = nullptr;
        color = nullptr;

        allowX = true;
        allowColor = true;
        allowBreak = true;
        allowY = false;
        allowBlur = false;
        allowSpread = false;
        allowStyle = property == CSSPropertyWebkitBoxShadow || property == CSSPropertyBoxShadow;
    }

    void commitLength(CSSParserValue* v)
    {
        RefPtrWillBeRawPtr<CSSPrimitiveValue> val = m_parser->createPrimitiveNumericValue(v);

        if (allowX) {
            x = val.release();
            allowX = false;
            allowY = true;
            allowColor = false;
            allowStyle = false;
            allowBreak = false;
        } else if (allowY) {
            y = val.release();
            allowY = false;
            allowBlur = true;
            allowColor = true;
            allowStyle = property == CSSPropertyWebkitBoxShadow || property == CSSPropertyBoxShadow;
            allowBreak = true;
        } else if (allowBlur) {
            blur = val.release();
            allowBlur = false;
            allowSpread = property == CSSPropertyWebkitBoxShadow || property == CSSPropertyBoxShadow;
        } else if (allowSpread) {
            spread = val.release();
            allowSpread = false;
        }
    }

    void commitColor(PassRefPtrWillBeRawPtr<CSSPrimitiveValue> val)
    {
        color = val;
        allowColor = false;
        if (allowX) {
            allowStyle = false;
            allowBreak = false;
        } else {
            allowBlur = false;
            allowSpread = false;
            allowStyle = property == CSSPropertyWebkitBoxShadow || property == CSSPropertyBoxShadow;
        }
    }

    void commitStyle(CSSParserValue* v)
    {
        style = cssValuePool().createIdentifierValue(v->id);
        allowStyle = false;
        if (allowX)
            allowBreak = false;
        else {
            allowBlur = false;
            allowSpread = false;
            allowColor = false;
        }
    }

    CSSPropertyID property;
    CSSPropertyParser* m_parser;

    RefPtrWillBeRawPtr<CSSValueList> values;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> x;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> y;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> blur;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> spread;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> style;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> color;

    bool allowX;
    bool allowY;
    bool allowBlur;
    bool allowSpread;
    bool allowColor;
    bool allowStyle; // inset or not.
    bool allowBreak;
};

PassRefPtrWillBeRawPtr<CSSValueList> CSSPropertyParser::parseShadow(CSSParserValueList* valueList, CSSPropertyID propId)
{
    ShadowParseContext context(propId, this);
    CSSParserValue* val;
    while ((val = valueList->current())) {
        // Check for a comma break first.
        if (val->unit == CSSParserValue::Operator) {
            if (val->iValue != ',' || !context.allowBreak) {
                // Other operators aren't legal or we aren't done with the current shadow
                // value.  Treat as invalid.
                return nullptr;
            }
            // The value is good.  Commit it.
            context.commitValue();
        } else if (validUnit(val, FLength, HTMLStandardMode)) {
            // We required a length and didn't get one. Invalid.
            if (!context.allowLength())
                return nullptr;

            // Blur radius must be non-negative.
            if (context.allowBlur && !validUnit(val, FLength | FNonNeg, HTMLStandardMode))
                return nullptr;

            // A length is allowed here.  Construct the value and add it.
            context.commitLength(val);
        } else if (val->id == CSSValueInset) {
            if (!context.allowStyle)
                return nullptr;

            context.commitStyle(val);
        } else {
            // The only other type of value that's ok is a color value.
            RefPtrWillBeRawPtr<CSSPrimitiveValue> parsedColor;
            bool isColor = ((val->id >= CSSValueAqua && val->id <= CSSValueWindowtext) || val->id == CSSValueMenu
                            || (val->id >= CSSValueWebkitFocusRingColor && val->id <= CSSValueWebkitText && inQuirksMode())
                            || val->id == CSSValueCurrentcolor);
            if (isColor) {
                if (!context.allowColor)
                    return nullptr;
                parsedColor = cssValuePool().createIdentifierValue(val->id);
            }

            if (!parsedColor)
                // It's not built-in. Try to parse it as a color.
                parsedColor = parseColor(val);

            if (!parsedColor || !context.allowColor)
                return nullptr; // This value is not a color or length and is invalid or
                          // it is a color, but a color isn't allowed at this point.

            context.commitColor(parsedColor.release());
        }

        valueList->next();
    }

    if (context.allowBreak) {
        context.commitValue();
        if (context.values && context.values->length())
            return context.values.release();
    }

    return nullptr;
}

bool CSSPropertyParser::parseReflect(CSSPropertyID propId, bool important)
{
    // box-reflect: <direction> <offset> <mask>

    // Direction comes first.
    CSSParserValue* val = m_valueList->current();
    RefPtrWillBeRawPtr<CSSPrimitiveValue> direction;
    switch (val->id) {
    case CSSValueAbove:
    case CSSValueBelow:
    case CSSValueLeft:
    case CSSValueRight:
        direction = cssValuePool().createIdentifierValue(val->id);
        break;
    default:
        return false;
    }

    // The offset comes next.
    val = m_valueList->next();
    RefPtrWillBeRawPtr<CSSPrimitiveValue> offset;
    if (!val)
        offset = cssValuePool().createValue(0, CSSPrimitiveValue::CSS_PX);
    else {
        if (!validUnit(val, FLength | FPercent))
            return false;
        offset = createPrimitiveNumericValue(val);
    }

    // Now for the mask.
    RefPtrWillBeRawPtr<CSSValue> mask;
    val = m_valueList->next();
    if (val) {
        mask = parseBorderImage(propId);
        if (!mask)
            return false;
    }

    RefPtrWillBeRawPtr<CSSReflectValue> reflectValue = CSSReflectValue::create(direction.release(), offset.release(), mask.release());
    addProperty(propId, reflectValue.release(), important);
    m_valueList->next();
    return true;
}

bool CSSPropertyParser::parseFlex(CSSParserValueList* args, bool important)
{
    if (!args || !args->size() || args->size() > 3)
        return false;
    static const double unsetValue = -1;
    double flexGrow = unsetValue;
    double flexShrink = unsetValue;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> flexBasis;

    while (CSSParserValue* arg = args->current()) {
        if (validUnit(arg, FNumber | FNonNeg)) {
            if (flexGrow == unsetValue)
                flexGrow = arg->fValue;
            else if (flexShrink == unsetValue)
                flexShrink = arg->fValue;
            else if (!arg->fValue) {
                // flex only allows a basis of 0 (sans units) if flex-grow and flex-shrink values have already been set.
                flexBasis = cssValuePool().createValue(0, CSSPrimitiveValue::CSS_PX);
            } else {
                // We only allow 3 numbers without units if the last value is 0. E.g., flex:1 1 1 is invalid.
                return false;
            }
        } else if (!flexBasis && (arg->id == CSSValueAuto || validUnit(arg, FLength | FPercent | FNonNeg)))
            flexBasis = parseValidPrimitive(arg->id, arg);
        else {
            // Not a valid arg for flex.
            return false;
        }
        args->next();
    }

    if (flexGrow == unsetValue)
        flexGrow = 1;
    if (flexShrink == unsetValue)
        flexShrink = 1;
    if (!flexBasis)
        flexBasis = cssValuePool().createValue(0, CSSPrimitiveValue::CSS_PX);

    addProperty(CSSPropertyFlexGrow, cssValuePool().createValue(clampToFloat(flexGrow), CSSPrimitiveValue::CSS_NUMBER), important);
    addProperty(CSSPropertyFlexShrink, cssValuePool().createValue(clampToFloat(flexShrink), CSSPrimitiveValue::CSS_NUMBER), important);
    addProperty(CSSPropertyFlexBasis, flexBasis, important);
    return true;
}

bool CSSPropertyParser::parseObjectPosition(bool important)
{
    RefPtrWillBeRawPtr<CSSValue> xValue;
    RefPtrWillBeRawPtr<CSSValue> yValue;
    parseFillPosition(m_valueList.get(), xValue, yValue);
    if (!xValue || !yValue)
        return false;
    addProperty(
        CSSPropertyObjectPosition,
        createPrimitiveValuePair(toCSSPrimitiveValue(xValue.get()), toCSSPrimitiveValue(yValue.get()), Pair::KeepIdenticalValues),
        important);
    return true;
}

class BorderImageParseContext {
    DISALLOW_ALLOCATION();
public:
    BorderImageParseContext()
    : m_canAdvance(false)
    , m_allowCommit(true)
    , m_allowImage(true)
    , m_allowImageSlice(true)
    , m_allowRepeat(true)
    , m_allowForwardSlashOperator(false)
    , m_requireWidth(false)
    , m_requireOutset(false)
    {}

    bool canAdvance() const { return m_canAdvance; }
    void setCanAdvance(bool canAdvance) { m_canAdvance = canAdvance; }

    bool allowCommit() const { return m_allowCommit; }
    bool allowImage() const { return m_allowImage; }
    bool allowImageSlice() const { return m_allowImageSlice; }
    bool allowRepeat() const { return m_allowRepeat; }
    bool allowForwardSlashOperator() const { return m_allowForwardSlashOperator; }

    bool requireWidth() const { return m_requireWidth; }
    bool requireOutset() const { return m_requireOutset; }

    void commitImage(PassRefPtrWillBeRawPtr<CSSValue> image)
    {
        m_image = image;
        m_canAdvance = true;
        m_allowCommit = true;
        m_allowImage = m_allowForwardSlashOperator = m_requireWidth = m_requireOutset = false;
        m_allowImageSlice = !m_imageSlice;
        m_allowRepeat = !m_repeat;
    }
    void commitImageSlice(PassRefPtrWillBeRawPtr<CSSBorderImageSliceValue> slice)
    {
        m_imageSlice = slice;
        m_canAdvance = true;
        m_allowCommit = m_allowForwardSlashOperator = true;
        m_allowImageSlice = m_requireWidth = m_requireOutset = false;
        m_allowImage = !m_image;
        m_allowRepeat = !m_repeat;
    }
    void commitForwardSlashOperator()
    {
        m_canAdvance = true;
        m_allowCommit = m_allowImage = m_allowImageSlice = m_allowRepeat = m_allowForwardSlashOperator = false;
        if (!m_borderSlice) {
            m_requireWidth = true;
            m_requireOutset = false;
        } else {
            m_requireOutset = true;
            m_requireWidth = false;
        }
    }
    void commitBorderWidth(PassRefPtrWillBeRawPtr<CSSPrimitiveValue> slice)
    {
        m_borderSlice = slice;
        m_canAdvance = true;
        m_allowCommit = m_allowForwardSlashOperator = true;
        m_allowImageSlice = m_requireWidth = m_requireOutset = false;
        m_allowImage = !m_image;
        m_allowRepeat = !m_repeat;
    }
    void commitBorderOutset(PassRefPtrWillBeRawPtr<CSSPrimitiveValue> outset)
    {
        m_outset = outset;
        m_canAdvance = true;
        m_allowCommit = true;
        m_allowImageSlice = m_allowForwardSlashOperator = m_requireWidth = m_requireOutset = false;
        m_allowImage = !m_image;
        m_allowRepeat = !m_repeat;
    }
    void commitRepeat(PassRefPtrWillBeRawPtr<CSSValue> repeat)
    {
        m_repeat = repeat;
        m_canAdvance = true;
        m_allowCommit = true;
        m_allowRepeat = m_allowForwardSlashOperator = m_requireWidth = m_requireOutset = false;
        m_allowImageSlice = !m_imageSlice;
        m_allowImage = !m_image;
    }

    PassRefPtrWillBeRawPtr<CSSValue> commitCSSValue()
    {
        return createBorderImageValue(m_image, m_imageSlice, m_borderSlice, m_outset, m_repeat);
    }

    void commitMaskBoxImage(CSSPropertyParser* parser, bool important)
    {
        commitBorderImageProperty(CSSPropertyWebkitMaskBoxImageSource, parser, m_image, important);
        commitBorderImageProperty(CSSPropertyWebkitMaskBoxImageSlice, parser, m_imageSlice, important);
        commitBorderImageProperty(CSSPropertyWebkitMaskBoxImageWidth, parser, m_borderSlice, important);
        commitBorderImageProperty(CSSPropertyWebkitMaskBoxImageOutset, parser, m_outset, important);
        commitBorderImageProperty(CSSPropertyWebkitMaskBoxImageRepeat, parser, m_repeat, important);
    }

    void commitBorderImage(CSSPropertyParser* parser, bool important)
    {
        commitBorderImageProperty(CSSPropertyBorderImageSource, parser, m_image, important);
        commitBorderImageProperty(CSSPropertyBorderImageSlice, parser, m_imageSlice, important);
        commitBorderImageProperty(CSSPropertyBorderImageWidth, parser, m_borderSlice, important);
        commitBorderImageProperty(CSSPropertyBorderImageOutset, parser, m_outset, important);
        commitBorderImageProperty(CSSPropertyBorderImageRepeat, parser, m_repeat, important);
    }

    void commitBorderImageProperty(CSSPropertyID propId, CSSPropertyParser* parser, PassRefPtrWillBeRawPtr<CSSValue> value, bool important)
    {
        if (value)
            parser->addProperty(propId, value, important);
        else
            parser->addProperty(propId, cssValuePool().createImplicitInitialValue(), important, true);
    }

    static bool buildFromParser(CSSPropertyParser&, CSSPropertyID, BorderImageParseContext&);

    bool m_canAdvance;

    bool m_allowCommit;
    bool m_allowImage;
    bool m_allowImageSlice;
    bool m_allowRepeat;
    bool m_allowForwardSlashOperator;

    bool m_requireWidth;
    bool m_requireOutset;

    RefPtrWillBeRawPtr<CSSValue> m_image;
    RefPtrWillBeRawPtr<CSSBorderImageSliceValue> m_imageSlice;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> m_borderSlice;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> m_outset;

    RefPtrWillBeRawPtr<CSSValue> m_repeat;
};

bool BorderImageParseContext::buildFromParser(CSSPropertyParser& parser, CSSPropertyID propId, BorderImageParseContext& context)
{
    CSSPropertyParser::ShorthandScope scope(&parser, propId);
    while (CSSParserValue* val = parser.m_valueList->current()) {
        context.setCanAdvance(false);

        if (!context.canAdvance() && context.allowForwardSlashOperator() && isForwardSlashOperator(val))
            context.commitForwardSlashOperator();

        if (!context.canAdvance() && context.allowImage()) {
            if (val->unit == CSSPrimitiveValue::CSS_URI) {
                context.commitImage(CSSImageValue::create(parser.m_context.completeURL(val->string)));
            } else if (isGeneratedImageValue(val)) {
                RefPtrWillBeRawPtr<CSSValue> value;
                if (parser.parseGeneratedImage(parser.m_valueList.get(), value))
                    context.commitImage(value.release());
                else
                    return false;
            } else if (val->unit == CSSParserValue::Function && equalIgnoringCase(val->function->name, "-webkit-image-set(")) {
                RefPtrWillBeRawPtr<CSSValue> value = parser.parseImageSet(parser.m_valueList.get());
                if (value)
                    context.commitImage(value.release());
                else
                    return false;
            } else if (val->id == CSSValueNone)
                context.commitImage(cssValuePool().createIdentifierValue(CSSValueNone));
        }

        if (!context.canAdvance() && context.allowImageSlice()) {
            RefPtrWillBeRawPtr<CSSBorderImageSliceValue> imageSlice;
            if (parser.parseBorderImageSlice(propId, imageSlice))
                context.commitImageSlice(imageSlice.release());
        }

        if (!context.canAdvance() && context.allowRepeat()) {
            RefPtrWillBeRawPtr<CSSValue> repeat;
            if (parser.parseBorderImageRepeat(repeat))
                context.commitRepeat(repeat.release());
        }

        if (!context.canAdvance() && context.requireWidth()) {
            RefPtrWillBeRawPtr<CSSPrimitiveValue> borderSlice;
            if (parser.parseBorderImageWidth(borderSlice))
                context.commitBorderWidth(borderSlice.release());
        }

        if (!context.canAdvance() && context.requireOutset()) {
            RefPtrWillBeRawPtr<CSSPrimitiveValue> borderOutset;
            if (parser.parseBorderImageOutset(borderOutset))
                context.commitBorderOutset(borderOutset.release());
        }

        if (!context.canAdvance())
            return false;

        parser.m_valueList->next();
    }

    return context.allowCommit();
}

bool CSSPropertyParser::parseBorderImageShorthand(CSSPropertyID propId, bool important)
{
    BorderImageParseContext context;
    if (BorderImageParseContext::buildFromParser(*this, propId, context)) {
        switch (propId) {
        case CSSPropertyWebkitMaskBoxImage:
            context.commitMaskBoxImage(this, important);
            return true;
        case CSSPropertyBorderImage:
            context.commitBorderImage(this, important);
            return true;
        default:
            ASSERT_NOT_REACHED();
            return false;
        }
    }
    return false;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseBorderImage(CSSPropertyID propId)
{
    BorderImageParseContext context;
    if (BorderImageParseContext::buildFromParser(*this, propId, context)) {
        return context.commitCSSValue();
    }
    return nullptr;
}

static bool isBorderImageRepeatKeyword(int id)
{
    return id == CSSValueStretch || id == CSSValueRepeat || id == CSSValueSpace || id == CSSValueRound;
}

bool CSSPropertyParser::parseBorderImageRepeat(RefPtrWillBeRawPtr<CSSValue>& result)
{
    RefPtrWillBeRawPtr<CSSPrimitiveValue> firstValue;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> secondValue;
    CSSParserValue* val = m_valueList->current();
    if (!val)
        return false;
    if (isBorderImageRepeatKeyword(val->id))
        firstValue = cssValuePool().createIdentifierValue(val->id);
    else
        return false;

    val = m_valueList->next();
    if (val) {
        if (isBorderImageRepeatKeyword(val->id))
            secondValue = cssValuePool().createIdentifierValue(val->id);
        else if (!inShorthand()) {
            // If we're not parsing a shorthand then we are invalid.
            return false;
        } else {
            // We need to rewind the value list, so that when its advanced we'll
            // end up back at this value.
            m_valueList->previous();
            secondValue = firstValue;
        }
    } else
        secondValue = firstValue;

    result = createPrimitiveValuePair(firstValue, secondValue);
    return true;
}

class BorderImageSliceParseContext {
    DISALLOW_ALLOCATION();
public:
    BorderImageSliceParseContext(CSSPropertyParser* parser)
    : m_parser(parser)
    , m_allowNumber(true)
    , m_allowFill(true)
    , m_allowFinalCommit(false)
    , m_fill(false)
    { }

    bool allowNumber() const { return m_allowNumber; }
    bool allowFill() const { return m_allowFill; }
    bool allowFinalCommit() const { return m_allowFinalCommit; }
    CSSPrimitiveValue* top() const { return m_top.get(); }

    void commitNumber(CSSParserValue* v)
    {
        RefPtrWillBeRawPtr<CSSPrimitiveValue> val = m_parser->createPrimitiveNumericValue(v);
        if (!m_top)
            m_top = val;
        else if (!m_right)
            m_right = val;
        else if (!m_bottom)
            m_bottom = val;
        else {
            ASSERT(!m_left);
            m_left = val;
        }

        m_allowNumber = !m_left;
        m_allowFinalCommit = true;
    }

    void commitFill() { m_fill = true; m_allowFill = false; m_allowNumber = !m_top; }

    PassRefPtrWillBeRawPtr<CSSBorderImageSliceValue> commitBorderImageSlice()
    {
        // We need to clone and repeat values for any omissions.
        ASSERT(m_top);
        if (!m_right) {
            m_right = m_top;
            m_bottom = m_top;
            m_left = m_top;
        }
        if (!m_bottom) {
            m_bottom = m_top;
            m_left = m_right;
        }
        if (!m_left)
            m_left = m_right;

        // Now build a rect value to hold all four of our primitive values.
        RefPtrWillBeRawPtr<Quad> quad = Quad::create();
        quad->setTop(m_top);
        quad->setRight(m_right);
        quad->setBottom(m_bottom);
        quad->setLeft(m_left);

        // Make our new border image value now.
        return CSSBorderImageSliceValue::create(cssValuePool().createValue(quad.release()), m_fill);
    }

private:
    CSSPropertyParser* m_parser;

    bool m_allowNumber;
    bool m_allowFill;
    bool m_allowFinalCommit;

    RefPtrWillBeRawPtr<CSSPrimitiveValue> m_top;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> m_right;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> m_bottom;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> m_left;

    bool m_fill;
};

bool CSSPropertyParser::parseBorderImageSlice(CSSPropertyID propId, RefPtrWillBeRawPtr<CSSBorderImageSliceValue>& result)
{
    BorderImageSliceParseContext context(this);
    CSSParserValue* val;
    while ((val = m_valueList->current())) {
        // FIXME calc() http://webkit.org/b/16662 : calc is parsed but values are not created yet.
        if (context.allowNumber() && !isCalculation(val) && validUnit(val, FInteger | FNonNeg | FPercent, HTMLStandardMode)) {
            context.commitNumber(val);
        } else if (context.allowFill() && val->id == CSSValueFill)
            context.commitFill();
        else if (!inShorthand()) {
            // If we're not parsing a shorthand then we are invalid.
            return false;
        } else {
            if (context.allowFinalCommit()) {
                // We're going to successfully parse, but we don't want to consume this token.
                m_valueList->previous();
            }
            break;
        }
        m_valueList->next();
    }

    if (context.allowFinalCommit()) {
        // FIXME: For backwards compatibility, -webkit-border-image, -webkit-mask-box-image and -webkit-box-reflect have to do a fill by default.
        // FIXME: What do we do with -webkit-box-reflect and -webkit-mask-box-image? Probably just have to leave them filling...
        if (propId == CSSPropertyWebkitBorderImage || propId == CSSPropertyWebkitMaskBoxImage || propId == CSSPropertyWebkitBoxReflect)
            context.commitFill();

        // Need to fully commit as a single value.
        result = context.commitBorderImageSlice();
        return true;
    }

    return false;
}

class BorderImageQuadParseContext {
public:
    BorderImageQuadParseContext(CSSPropertyParser* parser)
    : m_parser(parser)
    , m_allowNumber(true)
    , m_allowFinalCommit(false)
    { }

    bool allowNumber() const { return m_allowNumber; }
    bool allowFinalCommit() const { return m_allowFinalCommit; }
    CSSPrimitiveValue* top() const { return m_top.get(); }

    void commitNumber(CSSParserValue* v)
    {
        RefPtrWillBeRawPtr<CSSPrimitiveValue> val;
        if (v->id == CSSValueAuto)
            val = cssValuePool().createIdentifierValue(v->id);
        else
            val = m_parser->createPrimitiveNumericValue(v);

        if (!m_top)
            m_top = val;
        else if (!m_right)
            m_right = val;
        else if (!m_bottom)
            m_bottom = val;
        else {
            ASSERT(!m_left);
            m_left = val;
        }

        m_allowNumber = !m_left;
        m_allowFinalCommit = true;
    }

    void setAllowFinalCommit() { m_allowFinalCommit = true; }
    void setTop(PassRefPtrWillBeRawPtr<CSSPrimitiveValue> val) { m_top = val; }

    PassRefPtrWillBeRawPtr<CSSPrimitiveValue> commitBorderImageQuad()
    {
        // We need to clone and repeat values for any omissions.
        ASSERT(m_top);
        if (!m_right) {
            m_right = m_top;
            m_bottom = m_top;
            m_left = m_top;
        }
        if (!m_bottom) {
            m_bottom = m_top;
            m_left = m_right;
        }
        if (!m_left)
            m_left = m_right;

        // Now build a quad value to hold all four of our primitive values.
        RefPtrWillBeRawPtr<Quad> quad = Quad::create();
        quad->setTop(m_top);
        quad->setRight(m_right);
        quad->setBottom(m_bottom);
        quad->setLeft(m_left);

        // Make our new value now.
        return cssValuePool().createValue(quad.release());
    }

private:
    CSSPropertyParser* m_parser;

    bool m_allowNumber;
    bool m_allowFinalCommit;

    RefPtrWillBeRawPtr<CSSPrimitiveValue> m_top;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> m_right;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> m_bottom;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> m_left;
};

bool CSSPropertyParser::parseBorderImageQuad(Units validUnits, RefPtrWillBeRawPtr<CSSPrimitiveValue>& result)
{
    BorderImageQuadParseContext context(this);
    CSSParserValue* val;
    while ((val = m_valueList->current())) {
        if (context.allowNumber() && (validUnit(val, validUnits, HTMLStandardMode) || val->id == CSSValueAuto)) {
            context.commitNumber(val);
        } else if (!inShorthand()) {
            // If we're not parsing a shorthand then we are invalid.
            return false;
        } else {
            if (context.allowFinalCommit())
                m_valueList->previous(); // The shorthand loop will advance back to this point.
            break;
        }
        m_valueList->next();
    }

    if (context.allowFinalCommit()) {
        // Need to fully commit as a single value.
        result = context.commitBorderImageQuad();
        return true;
    }
    return false;
}

bool CSSPropertyParser::parseBorderImageWidth(RefPtrWillBeRawPtr<CSSPrimitiveValue>& result)
{
    return parseBorderImageQuad(FLength | FNumber | FNonNeg | FPercent, result);
}

bool CSSPropertyParser::parseBorderImageOutset(RefPtrWillBeRawPtr<CSSPrimitiveValue>& result)
{
    return parseBorderImageQuad(FLength | FNumber | FNonNeg, result);
}

bool CSSPropertyParser::parseBorderRadius(CSSPropertyID propId, bool important)
{
    unsigned num = m_valueList->size();
    if (num > 9)
        return false;

    ShorthandScope scope(this, propId);
    RefPtrWillBeRawPtr<CSSPrimitiveValue> radii[2][4];

    unsigned indexAfterSlash = 0;
    for (unsigned i = 0; i < num; ++i) {
        CSSParserValue* value = m_valueList->valueAt(i);
        if (value->unit == CSSParserValue::Operator) {
            if (value->iValue != '/')
                return false;

            if (!i || indexAfterSlash || i + 1 == num || num > i + 5)
                return false;

            indexAfterSlash = i + 1;
            completeBorderRadii(radii[0]);
            continue;
        }

        if (i - indexAfterSlash >= 4)
            return false;

        if (!validUnit(value, FLength | FPercent | FNonNeg))
            return false;

        RefPtrWillBeRawPtr<CSSPrimitiveValue> radius = createPrimitiveNumericValue(value);

        if (!indexAfterSlash) {
            radii[0][i] = radius;

            // Legacy syntax: -webkit-border-radius: l1 l2; is equivalent to border-radius: l1 / l2;
            if (num == 2 && propId == CSSPropertyWebkitBorderRadius) {
                indexAfterSlash = 1;
                completeBorderRadii(radii[0]);
            }
        } else
            radii[1][i - indexAfterSlash] = radius.release();
    }

    if (!indexAfterSlash) {
        completeBorderRadii(radii[0]);
        for (unsigned i = 0; i < 4; ++i)
            radii[1][i] = radii[0][i];
    } else
        completeBorderRadii(radii[1]);

    ImplicitScope implicitScope(this, PropertyImplicit);
    addProperty(CSSPropertyBorderTopLeftRadius, createPrimitiveValuePair(radii[0][0].release(), radii[1][0].release()), important);
    addProperty(CSSPropertyBorderTopRightRadius, createPrimitiveValuePair(radii[0][1].release(), radii[1][1].release()), important);
    addProperty(CSSPropertyBorderBottomRightRadius, createPrimitiveValuePair(radii[0][2].release(), radii[1][2].release()), important);
    addProperty(CSSPropertyBorderBottomLeftRadius, createPrimitiveValuePair(radii[0][3].release(), radii[1][3].release()), important);
    return true;
}

bool CSSPropertyParser::parseAspectRatio(bool important)
{
    unsigned num = m_valueList->size();
    if (num == 1 && m_valueList->valueAt(0)->id == CSSValueNone) {
        addProperty(CSSPropertyWebkitAspectRatio, cssValuePool().createIdentifierValue(CSSValueNone), important);
        return true;
    }

    if (num != 3)
        return false;

    CSSParserValue* lvalue = m_valueList->valueAt(0);
    CSSParserValue* op = m_valueList->valueAt(1);
    CSSParserValue* rvalue = m_valueList->valueAt(2);

    if (!isForwardSlashOperator(op))
        return false;

    if (!validUnit(lvalue, FNumber | FNonNeg) || !validUnit(rvalue, FNumber | FNonNeg))
        return false;

    if (!lvalue->fValue || !rvalue->fValue)
        return false;

    addProperty(CSSPropertyWebkitAspectRatio, CSSAspectRatioValue::create(narrowPrecisionToFloat(lvalue->fValue), narrowPrecisionToFloat(rvalue->fValue)), important);

    return true;
}

bool CSSPropertyParser::parseCounter(CSSPropertyID propId, int defaultValue, bool important)
{
    enum { ID, VAL } state = ID;

    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createCommaSeparated();
    RefPtrWillBeRawPtr<CSSPrimitiveValue> counterName;

    while (true) {
        CSSParserValue* val = m_valueList->current();
        switch (state) {
            case ID:
                if (val && val->unit == CSSPrimitiveValue::CSS_IDENT) {
                    counterName = createPrimitiveStringValue(val);
                    state = VAL;
                    m_valueList->next();
                    continue;
                }
                break;
            case VAL: {
                int i = defaultValue;
                if (val && val->unit == CSSPrimitiveValue::CSS_NUMBER) {
                    i = clampToInteger(val->fValue);
                    m_valueList->next();
                }

                list->append(createPrimitiveValuePair(counterName.release(),
                    cssValuePool().createValue(i, CSSPrimitiveValue::CSS_NUMBER)));
                state = ID;
                continue;
            }
        }
        break;
    }

    if (list->length() > 0) {
        addProperty(propId, list.release(), important);
        return true;
    }

    return false;
}

// This should go away once we drop support for -webkit-gradient
static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> parseDeprecatedGradientPoint(CSSParserValue* a, bool horizontal)
{
    RefPtrWillBeRawPtr<CSSPrimitiveValue> result;
    if (a->unit == CSSPrimitiveValue::CSS_IDENT) {
        if ((equalIgnoringCase(a, "left") && horizontal)
            || (equalIgnoringCase(a, "top") && !horizontal))
            result = cssValuePool().createValue(0., CSSPrimitiveValue::CSS_PERCENTAGE);
        else if ((equalIgnoringCase(a, "right") && horizontal)
                 || (equalIgnoringCase(a, "bottom") && !horizontal))
            result = cssValuePool().createValue(100., CSSPrimitiveValue::CSS_PERCENTAGE);
        else if (equalIgnoringCase(a, "center"))
            result = cssValuePool().createValue(50., CSSPrimitiveValue::CSS_PERCENTAGE);
    } else if (a->unit == CSSPrimitiveValue::CSS_NUMBER || a->unit == CSSPrimitiveValue::CSS_PERCENTAGE)
        result = cssValuePool().createValue(a->fValue, static_cast<CSSPrimitiveValue::UnitTypes>(a->unit));
    return result;
}

bool parseDeprecatedGradientColorStop(CSSPropertyParser* p, CSSParserValue* a, CSSGradientColorStop& stop)
{
    if (a->unit != CSSParserValue::Function)
        return false;

    if (!equalIgnoringCase(a->function->name, "from(") &&
        !equalIgnoringCase(a->function->name, "to(") &&
        !equalIgnoringCase(a->function->name, "color-stop("))
        return false;

    CSSParserValueList* args = a->function->args.get();
    if (!args)
        return false;

    if (equalIgnoringCase(a->function->name, "from(")
        || equalIgnoringCase(a->function->name, "to(")) {
        // The "from" and "to" stops expect 1 argument.
        if (args->size() != 1)
            return false;

        if (equalIgnoringCase(a->function->name, "from("))
            stop.m_position = cssValuePool().createValue(0, CSSPrimitiveValue::CSS_NUMBER);
        else
            stop.m_position = cssValuePool().createValue(1, CSSPrimitiveValue::CSS_NUMBER);

        CSSValueID id = args->current()->id;
        if (id == CSSValueWebkitText || (id >= CSSValueAqua && id <= CSSValueWindowtext) || id == CSSValueMenu)
            stop.m_color = cssValuePool().createIdentifierValue(id);
        else
            stop.m_color = p->parseColor(args->current());
        if (!stop.m_color)
            return false;
    }

    // The "color-stop" function expects 3 arguments.
    if (equalIgnoringCase(a->function->name, "color-stop(")) {
        if (args->size() != 3)
            return false;

        CSSParserValue* stopArg = args->current();
        if (stopArg->unit == CSSPrimitiveValue::CSS_PERCENTAGE)
            stop.m_position = cssValuePool().createValue(stopArg->fValue / 100, CSSPrimitiveValue::CSS_NUMBER);
        else if (stopArg->unit == CSSPrimitiveValue::CSS_NUMBER)
            stop.m_position = cssValuePool().createValue(stopArg->fValue, CSSPrimitiveValue::CSS_NUMBER);
        else
            return false;

        stopArg = args->next();
        if (stopArg->unit != CSSParserValue::Operator || stopArg->iValue != ',')
            return false;

        stopArg = args->next();
        CSSValueID id = stopArg->id;
        if (id == CSSValueWebkitText || (id >= CSSValueAqua && id <= CSSValueWindowtext) || id == CSSValueMenu)
            stop.m_color = cssValuePool().createIdentifierValue(id);
        else
            stop.m_color = p->parseColor(stopArg);
        if (!stop.m_color)
            return false;
    }

    return true;
}

bool CSSPropertyParser::parseDeprecatedGradient(CSSParserValueList* valueList, RefPtrWillBeRawPtr<CSSValue>& gradient)
{
    // Walk the arguments.
    CSSParserValueList* args = valueList->current()->function->args.get();
    if (!args || args->size() == 0)
        return false;

    // The first argument is the gradient type.  It is an identifier.
    CSSGradientType gradientType;
    CSSParserValue* a = args->current();
    if (!a || a->unit != CSSPrimitiveValue::CSS_IDENT)
        return false;
    if (equalIgnoringCase(a, "linear"))
        gradientType = CSSDeprecatedLinearGradient;
    else if (equalIgnoringCase(a, "radial"))
        gradientType = CSSDeprecatedRadialGradient;
    else
        return false;

    RefPtrWillBeRawPtr<CSSGradientValue> result;
    switch (gradientType) {
    case CSSDeprecatedLinearGradient:
        result = CSSLinearGradientValue::create(NonRepeating, gradientType);
        break;
    case CSSDeprecatedRadialGradient:
        result = CSSRadialGradientValue::create(NonRepeating, gradientType);
        break;
    default:
        // The rest of the gradient types shouldn't appear here.
        ASSERT_NOT_REACHED();
    }

    // Comma.
    a = args->next();
    if (!isComma(a))
        return false;

    // Next comes the starting point for the gradient as an x y pair.  There is no
    // comma between the x and the y values.
    // First X.  It can be left, right, number or percent.
    a = args->next();
    if (!a)
        return false;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> point = parseDeprecatedGradientPoint(a, true);
    if (!point)
        return false;
    result->setFirstX(point.release());

    // First Y.  It can be top, bottom, number or percent.
    a = args->next();
    if (!a)
        return false;
    point = parseDeprecatedGradientPoint(a, false);
    if (!point)
        return false;
    result->setFirstY(point.release());

    // Comma after the first point.
    a = args->next();
    if (!isComma(a))
        return false;

    // For radial gradients only, we now expect a numeric radius.
    if (gradientType == CSSDeprecatedRadialGradient) {
        a = args->next();
        if (!a || a->unit != CSSPrimitiveValue::CSS_NUMBER)
            return false;
        toCSSRadialGradientValue(result.get())->setFirstRadius(createPrimitiveNumericValue(a));

        // Comma after the first radius.
        a = args->next();
        if (!isComma(a))
            return false;
    }

    // Next is the ending point for the gradient as an x, y pair.
    // Second X.  It can be left, right, number or percent.
    a = args->next();
    if (!a)
        return false;
    point = parseDeprecatedGradientPoint(a, true);
    if (!point)
        return false;
    result->setSecondX(point.release());

    // Second Y.  It can be top, bottom, number or percent.
    a = args->next();
    if (!a)
        return false;
    point = parseDeprecatedGradientPoint(a, false);
    if (!point)
        return false;
    result->setSecondY(point.release());

    // For radial gradients only, we now expect the second radius.
    if (gradientType == CSSDeprecatedRadialGradient) {
        // Comma after the second point.
        a = args->next();
        if (!isComma(a))
            return false;

        a = args->next();
        if (!a || a->unit != CSSPrimitiveValue::CSS_NUMBER)
            return false;
        toCSSRadialGradientValue(result.get())->setSecondRadius(createPrimitiveNumericValue(a));
    }

    // We now will accept any number of stops (0 or more).
    a = args->next();
    while (a) {
        // Look for the comma before the next stop.
        if (!isComma(a))
            return false;

        // Now examine the stop itself.
        a = args->next();
        if (!a)
            return false;

        // The function name needs to be one of "from", "to", or "color-stop."
        CSSGradientColorStop stop;
        if (!parseDeprecatedGradientColorStop(this, a, stop))
            return false;
        result->addStop(stop);

        // Advance
        a = args->next();
    }

    gradient = result.release();
    return true;
}

static PassRefPtrWillBeRawPtr<CSSPrimitiveValue> valueFromSideKeyword(CSSParserValue* a, bool& isHorizontal)
{
    if (a->unit != CSSPrimitiveValue::CSS_IDENT)
        return nullptr;

    switch (a->id) {
        case CSSValueLeft:
        case CSSValueRight:
            isHorizontal = true;
            break;
        case CSSValueTop:
        case CSSValueBottom:
            isHorizontal = false;
            break;
        default:
            return nullptr;
    }
    return cssValuePool().createIdentifierValue(a->id);
}

PassRefPtrWillBeRawPtr<CSSPrimitiveValue> parseGradientColorOrKeyword(CSSPropertyParser* p, CSSParserValue* value)
{
    CSSValueID id = value->id;
    if (id == CSSValueWebkitText || (id >= CSSValueAqua && id <= CSSValueWindowtext) || id == CSSValueMenu || id == CSSValueCurrentcolor)
        return cssValuePool().createIdentifierValue(id);

    return p->parseColor(value);
}

bool CSSPropertyParser::parseDeprecatedLinearGradient(CSSParserValueList* valueList, RefPtrWillBeRawPtr<CSSValue>& gradient, CSSGradientRepeat repeating)
{
    RefPtrWillBeRawPtr<CSSLinearGradientValue> result = CSSLinearGradientValue::create(repeating, CSSPrefixedLinearGradient);

    // Walk the arguments.
    CSSParserValueList* args = valueList->current()->function->args.get();
    if (!args || !args->size())
        return false;

    CSSParserValue* a = args->current();
    if (!a)
        return false;

    bool expectComma = false;
    // Look for angle.
    if (validUnit(a, FAngle, HTMLStandardMode)) {
        result->setAngle(createPrimitiveNumericValue(a));

        args->next();
        expectComma = true;
    } else {
        // Look one or two optional keywords that indicate a side or corner.
        RefPtrWillBeRawPtr<CSSPrimitiveValue> startX, startY;

        RefPtrWillBeRawPtr<CSSPrimitiveValue> location;
        bool isHorizontal = false;
        if ((location = valueFromSideKeyword(a, isHorizontal))) {
            if (isHorizontal)
                startX = location;
            else
                startY = location;

            if ((a = args->next())) {
                if ((location = valueFromSideKeyword(a, isHorizontal))) {
                    if (isHorizontal) {
                        if (startX)
                            return false;
                        startX = location;
                    } else {
                        if (startY)
                            return false;
                        startY = location;
                    }

                    args->next();
                }
            }

            expectComma = true;
        }

        if (!startX && !startY)
            startY = cssValuePool().createIdentifierValue(CSSValueTop);

        result->setFirstX(startX.release());
        result->setFirstY(startY.release());
    }

    if (!parseGradientColorStops(args, result.get(), expectComma))
        return false;

    if (!result->stopCount())
        return false;

    gradient = result.release();
    return true;
}

bool CSSPropertyParser::parseDeprecatedRadialGradient(CSSParserValueList* valueList, RefPtrWillBeRawPtr<CSSValue>& gradient, CSSGradientRepeat repeating)
{
    RefPtrWillBeRawPtr<CSSRadialGradientValue> result = CSSRadialGradientValue::create(repeating, CSSPrefixedRadialGradient);

    // Walk the arguments.
    CSSParserValueList* args = valueList->current()->function->args.get();
    if (!args || !args->size())
        return false;

    CSSParserValue* a = args->current();
    if (!a)
        return false;

    bool expectComma = false;

    // Optional background-position
    RefPtrWillBeRawPtr<CSSValue> centerX;
    RefPtrWillBeRawPtr<CSSValue> centerY;
    // parse2ValuesFillPosition advances the args next pointer.
    parse2ValuesFillPosition(args, centerX, centerY);
    a = args->current();
    if (!a)
        return false;

    if (centerX || centerY) {
        // Comma
        if (!isComma(a))
            return false;

        a = args->next();
        if (!a)
            return false;
    }

    result->setFirstX(toCSSPrimitiveValue(centerX.get()));
    result->setSecondX(toCSSPrimitiveValue(centerX.get()));
    // CSS3 radial gradients always share the same start and end point.
    result->setFirstY(toCSSPrimitiveValue(centerY.get()));
    result->setSecondY(toCSSPrimitiveValue(centerY.get()));

    RefPtrWillBeRawPtr<CSSPrimitiveValue> shapeValue;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> sizeValue;

    // Optional shape and/or size in any order.
    for (int i = 0; i < 2; ++i) {
        if (a->unit != CSSPrimitiveValue::CSS_IDENT)
            break;

        bool foundValue = false;
        switch (a->id) {
        case CSSValueCircle:
        case CSSValueEllipse:
            shapeValue = cssValuePool().createIdentifierValue(a->id);
            foundValue = true;
            break;
        case CSSValueClosestSide:
        case CSSValueClosestCorner:
        case CSSValueFarthestSide:
        case CSSValueFarthestCorner:
        case CSSValueContain:
        case CSSValueCover:
            sizeValue = cssValuePool().createIdentifierValue(a->id);
            foundValue = true;
            break;
        default:
            break;
        }

        if (foundValue) {
            a = args->next();
            if (!a)
                return false;

            expectComma = true;
        }
    }

    result->setShape(shapeValue);
    result->setSizingBehavior(sizeValue);

    // Or, two lengths or percentages
    RefPtrWillBeRawPtr<CSSPrimitiveValue> horizontalSize;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> verticalSize;

    if (!shapeValue && !sizeValue) {
        if (validUnit(a, FLength | FPercent)) {
            horizontalSize = createPrimitiveNumericValue(a);
            a = args->next();
            if (!a)
                return false;

            expectComma = true;
        }

        if (validUnit(a, FLength | FPercent)) {
            verticalSize = createPrimitiveNumericValue(a);

            a = args->next();
            if (!a)
                return false;
            expectComma = true;
        }
    }

    // Must have neither or both.
    if (!horizontalSize != !verticalSize)
        return false;

    result->setEndHorizontalSize(horizontalSize);
    result->setEndVerticalSize(verticalSize);

    if (!parseGradientColorStops(args, result.get(), expectComma))
        return false;

    gradient = result.release();
    return true;
}

bool CSSPropertyParser::parseLinearGradient(CSSParserValueList* valueList, RefPtrWillBeRawPtr<CSSValue>& gradient, CSSGradientRepeat repeating)
{
    RefPtrWillBeRawPtr<CSSLinearGradientValue> result = CSSLinearGradientValue::create(repeating, CSSLinearGradient);

    CSSParserValueList* args = valueList->current()->function->args.get();
    if (!args || !args->size())
        return false;

    CSSParserValue* a = args->current();
    if (!a)
        return false;

    bool expectComma = false;
    // Look for angle.
    if (validUnit(a, FAngle, HTMLStandardMode)) {
        result->setAngle(createPrimitiveNumericValue(a));

        args->next();
        expectComma = true;
    } else if (a->unit == CSSPrimitiveValue::CSS_IDENT && equalIgnoringCase(a, "to")) {
        // to [ [left | right] || [top | bottom] ]
        a = args->next();
        if (!a)
            return false;

        RefPtrWillBeRawPtr<CSSPrimitiveValue> endX, endY;
        RefPtrWillBeRawPtr<CSSPrimitiveValue> location;
        bool isHorizontal = false;

        location = valueFromSideKeyword(a, isHorizontal);
        if (!location)
            return false;

        if (isHorizontal)
            endX = location;
        else
            endY = location;

        a = args->next();
        if (!a)
            return false;

        location = valueFromSideKeyword(a, isHorizontal);
        if (location) {
            if (isHorizontal) {
                if (endX)
                    return false;
                endX = location;
            } else {
                if (endY)
                    return false;
                endY = location;
            }

            args->next();
        }

        expectComma = true;
        result->setFirstX(endX.release());
        result->setFirstY(endY.release());
    }

    if (!parseGradientColorStops(args, result.get(), expectComma))
        return false;

    if (!result->stopCount())
        return false;

    gradient = result.release();
    return true;
}

bool CSSPropertyParser::parseRadialGradient(CSSParserValueList* valueList, RefPtrWillBeRawPtr<CSSValue>& gradient, CSSGradientRepeat repeating)
{
    RefPtrWillBeRawPtr<CSSRadialGradientValue> result = CSSRadialGradientValue::create(repeating, CSSRadialGradient);

    CSSParserValueList* args = valueList->current()->function->args.get();
    if (!args || !args->size())
        return false;

    CSSParserValue* a = args->current();
    if (!a)
        return false;

    bool expectComma = false;

    RefPtrWillBeRawPtr<CSSPrimitiveValue> shapeValue;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> sizeValue;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> horizontalSize;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> verticalSize;

    // First part of grammar, the size/shape clause:
    // [ circle || <length> ] |
    // [ ellipse || [ <length> | <percentage> ]{2} ] |
    // [ [ circle | ellipse] || <size-keyword> ]
    for (int i = 0; i < 3; ++i) {
        if (a->unit == CSSPrimitiveValue::CSS_IDENT) {
            bool badIdent = false;
            switch (a->id) {
            case CSSValueCircle:
            case CSSValueEllipse:
                if (shapeValue)
                    return false;
                shapeValue = cssValuePool().createIdentifierValue(a->id);
                break;
            case CSSValueClosestSide:
            case CSSValueClosestCorner:
            case CSSValueFarthestSide:
            case CSSValueFarthestCorner:
                if (sizeValue || horizontalSize)
                    return false;
                sizeValue = cssValuePool().createIdentifierValue(a->id);
                break;
            default:
                badIdent = true;
            }

            if (badIdent)
                break;

            a = args->next();
            if (!a)
                return false;
        } else if (validUnit(a, FLength | FPercent)) {

            if (sizeValue || horizontalSize)
                return false;
            horizontalSize = createPrimitiveNumericValue(a);

            a = args->next();
            if (!a)
                return false;

            if (validUnit(a, FLength | FPercent)) {
                verticalSize = createPrimitiveNumericValue(a);
                ++i;
                a = args->next();
                if (!a)
                    return false;
            }
        } else
            break;
    }

    // You can specify size as a keyword or a length/percentage, not both.
    if (sizeValue && horizontalSize)
        return false;
    // Circles must have 0 or 1 lengths.
    if (shapeValue && shapeValue->getValueID() == CSSValueCircle && verticalSize)
        return false;
    // Ellipses must have 0 or 2 length/percentages.
    if (shapeValue && shapeValue->getValueID() == CSSValueEllipse && horizontalSize && !verticalSize)
        return false;
    // If there's only one size, it must be a length.
    if (!verticalSize && horizontalSize && horizontalSize->isPercentage())
        return false;

    result->setShape(shapeValue);
    result->setSizingBehavior(sizeValue);
    result->setEndHorizontalSize(horizontalSize);
    result->setEndVerticalSize(verticalSize);

    // Second part of grammar, the center-position clause:
    // at <position>
    RefPtrWillBeRawPtr<CSSValue> centerX;
    RefPtrWillBeRawPtr<CSSValue> centerY;
    if (a->unit == CSSPrimitiveValue::CSS_IDENT && equalIgnoringCase(a, "at")) {
        a = args->next();
        if (!a)
            return false;

        parseFillPosition(args, centerX, centerY);
        if (!(centerX && centerY))
            return false;

        a = args->current();
        if (!a)
            return false;
        result->setFirstX(toCSSPrimitiveValue(centerX.get()));
        result->setFirstY(toCSSPrimitiveValue(centerY.get()));
        // Right now, CSS radial gradients have the same start and end centers.
        result->setSecondX(toCSSPrimitiveValue(centerX.get()));
        result->setSecondY(toCSSPrimitiveValue(centerY.get()));
    }

    if (shapeValue || sizeValue || horizontalSize || centerX || centerY)
        expectComma = true;

    if (!parseGradientColorStops(args, result.get(), expectComma))
        return false;

    gradient = result.release();
    return true;
}

bool CSSPropertyParser::parseGradientColorStops(CSSParserValueList* valueList, CSSGradientValue* gradient, bool expectComma)
{
    CSSParserValue* a = valueList->current();

    // Now look for color stops.
    while (a) {
        // Look for the comma before the next stop.
        if (expectComma) {
            if (!isComma(a))
                return false;

            a = valueList->next();
            if (!a)
                return false;
        }

        // <color-stop> = <color> [ <percentage> | <length> ]?
        CSSGradientColorStop stop;
        stop.m_color = parseGradientColorOrKeyword(this, a);
        if (!stop.m_color)
            return false;

        a = valueList->next();
        if (a) {
            if (validUnit(a, FLength | FPercent)) {
                stop.m_position = createPrimitiveNumericValue(a);
                a = valueList->next();
            }
        }

        gradient->addStop(stop);
        expectComma = true;
    }

    // Must have 2 or more stops to be valid.
    return gradient->stopCount() >= 2;
}

bool CSSPropertyParser::parseGeneratedImage(CSSParserValueList* valueList, RefPtrWillBeRawPtr<CSSValue>& value)
{
    CSSParserValue* val = valueList->current();

    if (val->unit != CSSParserValue::Function)
        return false;

    if (equalIgnoringCase(val->function->name, "-webkit-gradient(")) {
        // FIXME: This should send a deprecation message.
        if (m_context.useCounter())
            m_context.useCounter()->count(UseCounter::DeprecatedWebKitGradient);
        return parseDeprecatedGradient(valueList, value);
    }

    if (equalIgnoringCase(val->function->name, "-webkit-linear-gradient(")) {
        // FIXME: This should send a deprecation message.
        if (m_context.useCounter())
            m_context.useCounter()->count(UseCounter::DeprecatedWebKitLinearGradient);
        return parseDeprecatedLinearGradient(valueList, value, NonRepeating);
    }

    if (equalIgnoringCase(val->function->name, "linear-gradient("))
        return parseLinearGradient(valueList, value, NonRepeating);

    if (equalIgnoringCase(val->function->name, "-webkit-repeating-linear-gradient(")) {
        // FIXME: This should send a deprecation message.
        if (m_context.useCounter())
            m_context.useCounter()->count(UseCounter::DeprecatedWebKitRepeatingLinearGradient);
        return parseDeprecatedLinearGradient(valueList, value, Repeating);
    }

    if (equalIgnoringCase(val->function->name, "repeating-linear-gradient("))
        return parseLinearGradient(valueList, value, Repeating);

    if (equalIgnoringCase(val->function->name, "-webkit-radial-gradient(")) {
        // FIXME: This should send a deprecation message.
        if (m_context.useCounter())
            m_context.useCounter()->count(UseCounter::DeprecatedWebKitRadialGradient);
        return parseDeprecatedRadialGradient(valueList, value, NonRepeating);
    }

    if (equalIgnoringCase(val->function->name, "radial-gradient("))
        return parseRadialGradient(valueList, value, NonRepeating);

    if (equalIgnoringCase(val->function->name, "-webkit-repeating-radial-gradient(")) {
        if (m_context.useCounter())
            m_context.useCounter()->count(UseCounter::DeprecatedWebKitRepeatingRadialGradient);
        return parseDeprecatedRadialGradient(valueList, value, Repeating);
    }

    if (equalIgnoringCase(val->function->name, "repeating-radial-gradient("))
        return parseRadialGradient(valueList, value, Repeating);

    if (equalIgnoringCase(val->function->name, "-webkit-canvas("))
        return parseCanvas(valueList, value);

    if (equalIgnoringCase(val->function->name, "-webkit-cross-fade("))
        return parseCrossfade(valueList, value);

    return false;
}

bool CSSPropertyParser::parseCrossfade(CSSParserValueList* valueList, RefPtrWillBeRawPtr<CSSValue>& crossfade)
{
    // Walk the arguments.
    CSSParserValueList* args = valueList->current()->function->args.get();
    if (!args || args->size() != 5)
        return false;
    CSSParserValue* a = args->current();
    RefPtrWillBeRawPtr<CSSValue> fromImageValue;
    RefPtrWillBeRawPtr<CSSValue> toImageValue;

    // The first argument is the "from" image. It is a fill image.
    if (!a || !parseFillImage(args, fromImageValue))
        return false;
    a = args->next();

    // Skip a comma
    if (!isComma(a))
        return false;
    a = args->next();

    // The second argument is the "to" image. It is a fill image.
    if (!a || !parseFillImage(args, toImageValue))
        return false;
    a = args->next();

    // Skip a comma
    if (!isComma(a))
        return false;
    a = args->next();

    // The third argument is the crossfade value. It is a percentage or a fractional number.
    RefPtrWillBeRawPtr<CSSPrimitiveValue> percentage;
    if (!a)
        return false;

    if (a->unit == CSSPrimitiveValue::CSS_PERCENTAGE)
        percentage = cssValuePool().createValue(clampTo<double>(a->fValue / 100, 0, 1), CSSPrimitiveValue::CSS_NUMBER);
    else if (a->unit == CSSPrimitiveValue::CSS_NUMBER)
        percentage = cssValuePool().createValue(clampTo<double>(a->fValue, 0, 1), CSSPrimitiveValue::CSS_NUMBER);
    else
        return false;

    RefPtrWillBeRawPtr<CSSCrossfadeValue> result = CSSCrossfadeValue::create(fromImageValue, toImageValue);
    result->setPercentage(percentage);

    crossfade = result;

    return true;
}

bool CSSPropertyParser::parseCanvas(CSSParserValueList* valueList, RefPtrWillBeRawPtr<CSSValue>& canvas)
{
    // Walk the arguments.
    CSSParserValueList* args = valueList->current()->function->args.get();
    if (!args || args->size() != 1)
        return false;

    // The first argument is the canvas name.  It is an identifier.
    CSSParserValue* value = args->current();
    if (!value || value->unit != CSSPrimitiveValue::CSS_IDENT)
        return false;

    canvas = CSSCanvasValue::create(value->string);
    return true;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseImageSet(CSSParserValueList* valueList)
{
    CSSParserValue* function = valueList->current();

    if (function->unit != CSSParserValue::Function)
        return nullptr;

    CSSParserValueList* functionArgs = valueList->current()->function->args.get();
    if (!functionArgs || !functionArgs->size() || !functionArgs->current())
        return nullptr;

    RefPtrWillBeRawPtr<CSSImageSetValue> imageSet = CSSImageSetValue::create();

    CSSParserValue* arg = functionArgs->current();
    while (arg) {
        if (arg->unit != CSSPrimitiveValue::CSS_URI)
            return nullptr;

        RefPtrWillBeRawPtr<CSSImageValue> image = CSSImageValue::create(completeURL(arg->string));
        imageSet->append(image);

        arg = functionArgs->next();
        if (!arg || arg->unit != CSSPrimitiveValue::CSS_DIMENSION)
            return nullptr;

        double imageScaleFactor = 0;
        const String& string = arg->string;
        unsigned length = string.length();
        if (!length)
            return nullptr;
        if (string.is8Bit()) {
            const LChar* start = string.characters8();
            parseDouble(start, start + length, 'x', imageScaleFactor);
        } else {
            const UChar* start = string.characters16();
            parseDouble(start, start + length, 'x', imageScaleFactor);
        }
        if (imageScaleFactor <= 0)
            return nullptr;
        imageSet->append(cssValuePool().createValue(imageScaleFactor, CSSPrimitiveValue::CSS_NUMBER));

        // If there are no more arguments, we're done.
        arg = functionArgs->next();
        if (!arg)
            break;

        // If there are more arguments, they should be after a comma.
        if (!isComma(arg))
            return nullptr;

        // Skip the comma and move on to the next argument.
        arg = functionArgs->next();
    }

    return imageSet.release();
}

bool CSSPropertyParser::parseWillChange(bool important)
{
    ASSERT(RuntimeEnabledFeatures::cssWillChangeEnabled());

    RefPtrWillBeRawPtr<CSSValueList> values = CSSValueList::createCommaSeparated();
    if (m_valueList->current()->id == CSSValueAuto) {
        if (m_valueList->next())
            return false;
    }

    CSSParserValue* currentValue;
    bool expectComma = false;

    // Every comma-separated list of CSS_IDENTs is a valid will-change value,
    // unless the list includes an explicitly disallowed CSS_IDENT.
    while ((currentValue = m_valueList->current())) {
        if (expectComma) {
            if (!isComma(currentValue))
                return false;
            expectComma = false;
            m_valueList->next();
            continue;
        }

        if (currentValue->unit != CSSPrimitiveValue::CSS_IDENT)
            return false;

        if (CSSPropertyID property = cssPropertyID(currentValue->string)) {
            if (property == CSSPropertyWillChange)
                return false;
            values->append(cssValuePool().createIdentifierValue(property));
        } else {
            switch (currentValue->id) {
            case CSSValueNone:
            case CSSValueAll:
            case CSSValueAuto:
            case CSSValueDefault:
            case CSSValueInitial:
            case CSSValueInherit:
                return false;
            case CSSValueContents:
            case CSSValueScrollPosition:
                values->append(cssValuePool().createIdentifierValue(currentValue->id));
                break;
            default:
                break;
            }
        }
        expectComma = true;
        m_valueList->next();
    }

    addProperty(CSSPropertyWillChange, values.release(), important);
    return true;
}

class TransformOperationInfo {
public:
    TransformOperationInfo(const CSSParserString& name)
        : m_type(CSSTransformValue::UnknownTransformOperation)
        , m_argCount(1)
        , m_allowSingleArgument(false)
        , m_unit(CSSPropertyParser::FUnknown)
    {
        const UChar* characters;
        unsigned nameLength = name.length();

        const unsigned longestNameLength = 12;
        UChar characterBuffer[longestNameLength];
        if (name.is8Bit()) {
            unsigned length = std::min(longestNameLength, nameLength);
            const LChar* characters8 = name.characters8();
            for (unsigned i = 0; i < length; ++i)
                characterBuffer[i] = characters8[i];
            characters = characterBuffer;
        } else
            characters = name.characters16();

        SWITCH(characters, nameLength) {
            CASE("skew(") {
                m_unit = CSSPropertyParser::FAngle;
                m_type = CSSTransformValue::SkewTransformOperation;
                m_allowSingleArgument = true;
                m_argCount = 3;
            }
            CASE("scale(") {
                m_unit = CSSPropertyParser::FNumber;
                m_type = CSSTransformValue::ScaleTransformOperation;
                m_allowSingleArgument = true;
                m_argCount = 3;
            }
            CASE("skewx(") {
                m_unit = CSSPropertyParser::FAngle;
                m_type = CSSTransformValue::SkewXTransformOperation;
            }
            CASE("skewy(") {
                m_unit = CSSPropertyParser::FAngle;
                m_type = CSSTransformValue::SkewYTransformOperation;
            }
            CASE("matrix(") {
                m_unit = CSSPropertyParser::FNumber;
                m_type = CSSTransformValue::MatrixTransformOperation;
                m_argCount = 11;
            }
            CASE("rotate(") {
                m_unit = CSSPropertyParser::FAngle;
                m_type = CSSTransformValue::RotateTransformOperation;
            }
            CASE("scalex(") {
                m_unit = CSSPropertyParser::FNumber;
                m_type = CSSTransformValue::ScaleXTransformOperation;
            }
            CASE("scaley(") {
                m_unit = CSSPropertyParser::FNumber;
                m_type = CSSTransformValue::ScaleYTransformOperation;
            }
            CASE("scalez(") {
                m_unit = CSSPropertyParser::FNumber;
                m_type = CSSTransformValue::ScaleZTransformOperation;
            }
            CASE("scale3d(") {
                m_unit = CSSPropertyParser::FNumber;
                m_type = CSSTransformValue::Scale3DTransformOperation;
                m_argCount = 5;
            }
            CASE("rotatex(") {
                m_unit = CSSPropertyParser::FAngle;
                m_type = CSSTransformValue::RotateXTransformOperation;
            }
            CASE("rotatey(") {
                m_unit = CSSPropertyParser::FAngle;
                m_type = CSSTransformValue::RotateYTransformOperation;
            }
            CASE("rotatez(") {
                m_unit = CSSPropertyParser::FAngle;
                m_type = CSSTransformValue::RotateZTransformOperation;
            }
            CASE("matrix3d(") {
                m_unit = CSSPropertyParser::FNumber;
                m_type = CSSTransformValue::Matrix3DTransformOperation;
                m_argCount = 31;
            }
            CASE("rotate3d(") {
                m_unit = CSSPropertyParser::FNumber;
                m_type = CSSTransformValue::Rotate3DTransformOperation;
                m_argCount = 7;
            }
            CASE("translate(") {
                m_unit = CSSPropertyParser::FLength | CSSPropertyParser::FPercent;
                m_type = CSSTransformValue::TranslateTransformOperation;
                m_allowSingleArgument = true;
                m_argCount = 3;
            }
            CASE("translatex(") {
                m_unit = CSSPropertyParser::FLength | CSSPropertyParser::FPercent;
                m_type = CSSTransformValue::TranslateXTransformOperation;
            }
            CASE("translatey(") {
                m_unit = CSSPropertyParser::FLength | CSSPropertyParser::FPercent;
                m_type = CSSTransformValue::TranslateYTransformOperation;
            }
            CASE("translatez(") {
                m_unit = CSSPropertyParser::FLength | CSSPropertyParser::FPercent;
                m_type = CSSTransformValue::TranslateZTransformOperation;
            }
            CASE("perspective(") {
                m_unit = CSSPropertyParser::FNumber;
                m_type = CSSTransformValue::PerspectiveTransformOperation;
            }
            CASE("translate3d(") {
                m_unit = CSSPropertyParser::FLength | CSSPropertyParser::FPercent;
                m_type = CSSTransformValue::Translate3DTransformOperation;
                m_argCount = 5;
            }
        }
    }

    CSSTransformValue::TransformOperationType type() const { return m_type; }
    unsigned argCount() const { return m_argCount; }
    CSSPropertyParser::Units unit() const { return m_unit; }

    bool unknown() const { return m_type == CSSTransformValue::UnknownTransformOperation; }
    bool hasCorrectArgCount(unsigned argCount) { return m_argCount == argCount || (m_allowSingleArgument && argCount == 1); }

private:
    CSSTransformValue::TransformOperationType m_type;
    unsigned m_argCount;
    bool m_allowSingleArgument;
    CSSPropertyParser::Units m_unit;
};

PassRefPtrWillBeRawPtr<CSSValueList> CSSPropertyParser::parseTransform()
{
    if (!m_valueList)
        return nullptr;

    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    for (CSSParserValue* value = m_valueList->current(); value; value = m_valueList->next()) {
        RefPtrWillBeRawPtr<CSSValue> parsedTransformValue = parseTransformValue(value);
        if (!parsedTransformValue)
            return nullptr;

        list->append(parsedTransformValue.release());
    }

    return list.release();
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseTransformValue(CSSParserValue *value)
{
    if (value->unit != CSSParserValue::Function || !value->function)
        return nullptr;

    // Every primitive requires at least one argument.
    CSSParserValueList* args = value->function->args.get();
    if (!args)
        return nullptr;

    // See if the specified primitive is one we understand.
    TransformOperationInfo info(value->function->name);
    if (info.unknown())
        return nullptr;

    if (!info.hasCorrectArgCount(args->size()))
        return nullptr;

    // The transform is a list of functional primitives that specify transform operations.
    // We collect a list of CSSTransformValues, where each value specifies a single operation.

    // Create the new CSSTransformValue for this operation and add it to our list.
    RefPtrWillBeRawPtr<CSSTransformValue> transformValue = CSSTransformValue::create(info.type());

    // Snag our values.
    CSSParserValue* a = args->current();
    unsigned argNumber = 0;
    while (a) {
        CSSPropertyParser::Units unit = info.unit();

        if (info.type() == CSSTransformValue::Rotate3DTransformOperation && argNumber == 3) {
            // 4th param of rotate3d() is an angle rather than a bare number, validate it as such
            if (!validUnit(a, FAngle, HTMLStandardMode))
                return nullptr;
        } else if (info.type() == CSSTransformValue::Translate3DTransformOperation && argNumber == 2) {
            // 3rd param of translate3d() cannot be a percentage
            if (!validUnit(a, FLength, HTMLStandardMode))
                return nullptr;
        } else if (info.type() == CSSTransformValue::TranslateZTransformOperation && !argNumber) {
            // 1st param of translateZ() cannot be a percentage
            if (!validUnit(a, FLength, HTMLStandardMode))
                return nullptr;
        } else if (info.type() == CSSTransformValue::PerspectiveTransformOperation && !argNumber) {
            // 1st param of perspective() must be a non-negative number (deprecated) or length.
            if (!validUnit(a, FNumber | FLength | FNonNeg, HTMLStandardMode))
                return nullptr;
        } else if (!validUnit(a, unit, HTMLStandardMode)) {
            return nullptr;
        }

        // Add the value to the current transform operation.
        transformValue->append(createPrimitiveNumericValue(a));

        a = args->next();
        if (!a)
            break;
        if (a->unit != CSSParserValue::Operator || a->iValue != ',')
            return nullptr;
        a = args->next();

        argNumber++;
    }

    return transformValue.release();
}

bool CSSPropertyParser::isBlendMode(CSSValueID valueID)
{
    return (valueID >= CSSValueMultiply && valueID <= CSSValueLuminosity)
        || valueID == CSSValueNormal
        || valueID == CSSValueOverlay;
}

bool CSSPropertyParser::isCompositeOperator(CSSValueID valueID)
{
    // FIXME: Add CSSValueDestination and CSSValueLighter when the Compositing spec updates.
    return valueID >= CSSValueClear && valueID <= CSSValueXor;
}

static void filterInfoForName(const CSSParserString& name, CSSFilterValue::FilterOperationType& filterType, unsigned& maximumArgumentCount)
{
    if (equalIgnoringCase(name, "grayscale("))
        filterType = CSSFilterValue::GrayscaleFilterOperation;
    else if (equalIgnoringCase(name, "sepia("))
        filterType = CSSFilterValue::SepiaFilterOperation;
    else if (equalIgnoringCase(name, "saturate("))
        filterType = CSSFilterValue::SaturateFilterOperation;
    else if (equalIgnoringCase(name, "hue-rotate("))
        filterType = CSSFilterValue::HueRotateFilterOperation;
    else if (equalIgnoringCase(name, "invert("))
        filterType = CSSFilterValue::InvertFilterOperation;
    else if (equalIgnoringCase(name, "opacity("))
        filterType = CSSFilterValue::OpacityFilterOperation;
    else if (equalIgnoringCase(name, "brightness("))
        filterType = CSSFilterValue::BrightnessFilterOperation;
    else if (equalIgnoringCase(name, "contrast("))
        filterType = CSSFilterValue::ContrastFilterOperation;
    else if (equalIgnoringCase(name, "blur("))
        filterType = CSSFilterValue::BlurFilterOperation;
    else if (equalIgnoringCase(name, "drop-shadow(")) {
        filterType = CSSFilterValue::DropShadowFilterOperation;
        maximumArgumentCount = 4;  // x-offset, y-offset, blur-radius, color -- spread and inset style not allowed.
    }
}

PassRefPtrWillBeRawPtr<CSSFilterValue> CSSPropertyParser::parseBuiltinFilterArguments(CSSParserValueList* args, CSSFilterValue::FilterOperationType filterType)
{
    RefPtrWillBeRawPtr<CSSFilterValue> filterValue = CSSFilterValue::create(filterType);
    ASSERT(args);

    switch (filterType) {
    case CSSFilterValue::GrayscaleFilterOperation:
    case CSSFilterValue::SepiaFilterOperation:
    case CSSFilterValue::SaturateFilterOperation:
    case CSSFilterValue::InvertFilterOperation:
    case CSSFilterValue::OpacityFilterOperation:
    case CSSFilterValue::ContrastFilterOperation: {
        // One optional argument, 0-1 or 0%-100%, if missing use 100%.
        if (args->size() > 1)
            return nullptr;

        if (args->size()) {
            CSSParserValue* value = args->current();
            if (!validUnit(value, FNumber | FPercent | FNonNeg, HTMLStandardMode))
                return nullptr;

            double amount = value->fValue;

            // Saturate and Contrast allow values over 100%.
            if (filterType != CSSFilterValue::SaturateFilterOperation
                && filterType != CSSFilterValue::ContrastFilterOperation) {
                double maxAllowed = value->unit == CSSPrimitiveValue::CSS_PERCENTAGE ? 100.0 : 1.0;
                if (amount > maxAllowed)
                    return nullptr;
            }

            filterValue->append(cssValuePool().createValue(amount, static_cast<CSSPrimitiveValue::UnitTypes>(value->unit)));
        }
        break;
    }
    case CSSFilterValue::BrightnessFilterOperation: {
        // One optional argument, if missing use 100%.
        if (args->size() > 1)
            return nullptr;

        if (args->size()) {
            CSSParserValue* value = args->current();
            if (!validUnit(value, FNumber | FPercent, HTMLStandardMode))
                return nullptr;

            filterValue->append(cssValuePool().createValue(value->fValue, static_cast<CSSPrimitiveValue::UnitTypes>(value->unit)));
        }
        break;
    }
    case CSSFilterValue::HueRotateFilterOperation: {
        // hue-rotate() takes one optional angle.
        if (args->size() > 1)
            return nullptr;

        if (args->size()) {
            CSSParserValue* argument = args->current();
            if (!validUnit(argument, FAngle, HTMLStandardMode))
                return nullptr;

            filterValue->append(createPrimitiveNumericValue(argument));
        }
        break;
    }
    case CSSFilterValue::BlurFilterOperation: {
        // Blur takes a single length. Zero parameters are allowed.
        if (args->size() > 1)
            return nullptr;

        if (args->size()) {
            CSSParserValue* argument = args->current();
            if (!validUnit(argument, FLength | FNonNeg, HTMLStandardMode))
                return nullptr;

            filterValue->append(createPrimitiveNumericValue(argument));
        }
        break;
    }
    case CSSFilterValue::DropShadowFilterOperation: {
        // drop-shadow() takes a single shadow.
        RefPtrWillBeRawPtr<CSSValueList> shadowValueList = parseShadow(args, CSSPropertyWebkitFilter);
        if (!shadowValueList || shadowValueList->length() != 1)
            return nullptr;

        filterValue->append((shadowValueList.release())->itemWithoutBoundsCheck(0));
        break;
    }
    default:
        ASSERT_NOT_REACHED();
    }
    return filterValue.release();
}

PassRefPtrWillBeRawPtr<CSSValueList> CSSPropertyParser::parseFilter()
{
    if (!m_valueList)
        return nullptr;

    // The filter is a list of functional primitives that specify individual operations.
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    for (CSSParserValue* value = m_valueList->current(); value; value = m_valueList->next()) {
        if (value->unit != CSSPrimitiveValue::CSS_URI && (value->unit != CSSParserValue::Function || !value->function))
            return nullptr;

        CSSFilterValue::FilterOperationType filterType = CSSFilterValue::UnknownFilterOperation;

        // See if the specified primitive is one we understand.
        if (value->unit == CSSPrimitiveValue::CSS_URI) {
            RefPtrWillBeRawPtr<CSSFilterValue> referenceFilterValue = CSSFilterValue::create(CSSFilterValue::ReferenceFilterOperation);
            list->append(referenceFilterValue);
            referenceFilterValue->append(CSSSVGDocumentValue::create(value->string));
        } else {
            const CSSParserString name = value->function->name;
            unsigned maximumArgumentCount = 1;

            filterInfoForName(name, filterType, maximumArgumentCount);

            if (filterType == CSSFilterValue::UnknownFilterOperation)
                return nullptr;

            CSSParserValueList* args = value->function->args.get();
            if (!args)
                return nullptr;

            RefPtrWillBeRawPtr<CSSFilterValue> filterValue = parseBuiltinFilterArguments(args, filterType);
            if (!filterValue)
                return nullptr;

            list->append(filterValue);
        }
    }

    return list.release();
}

bool CSSPropertyParser::parseTransformOrigin(CSSPropertyID propId, CSSPropertyID& propId1, CSSPropertyID& propId2, CSSPropertyID& propId3, RefPtrWillBeRawPtr<CSSValue>& value, RefPtrWillBeRawPtr<CSSValue>& value2, RefPtrWillBeRawPtr<CSSValue>& value3)
{
    propId1 = propId;
    propId2 = propId;
    propId3 = propId;
    if (propId == CSSPropertyWebkitTransformOrigin) {
        propId1 = CSSPropertyWebkitTransformOriginX;
        propId2 = CSSPropertyWebkitTransformOriginY;
        propId3 = CSSPropertyWebkitTransformOriginZ;
    }

    switch (propId) {
        case CSSPropertyWebkitTransformOrigin:
            if (!parseTransformOriginShorthand(value, value2, value3))
                return false;
            // parseTransformOriginShorthand advances the m_valueList pointer
            break;
        case CSSPropertyWebkitTransformOriginX: {
            value = parseFillPositionX(m_valueList.get());
            if (value)
                m_valueList->next();
            break;
        }
        case CSSPropertyWebkitTransformOriginY: {
            value = parseFillPositionY(m_valueList.get());
            if (value)
                m_valueList->next();
            break;
        }
        case CSSPropertyWebkitTransformOriginZ: {
            if (validUnit(m_valueList->current(), FLength))
                value = createPrimitiveNumericValue(m_valueList->current());
            if (value)
                m_valueList->next();
            break;
        }
        default:
            ASSERT_NOT_REACHED();
            return false;
    }

    return value;
}

bool CSSPropertyParser::parsePerspectiveOrigin(CSSPropertyID propId, CSSPropertyID& propId1, CSSPropertyID& propId2, RefPtrWillBeRawPtr<CSSValue>& value, RefPtrWillBeRawPtr<CSSValue>& value2)
{
    propId1 = propId;
    propId2 = propId;
    if (propId == CSSPropertyWebkitPerspectiveOrigin) {
        propId1 = CSSPropertyWebkitPerspectiveOriginX;
        propId2 = CSSPropertyWebkitPerspectiveOriginY;
    }

    switch (propId) {
        case CSSPropertyWebkitPerspectiveOrigin:
            if (m_valueList->size() > 2)
                return false;
            parse2ValuesFillPosition(m_valueList.get(), value, value2);
            break;
        case CSSPropertyWebkitPerspectiveOriginX: {
            value = parseFillPositionX(m_valueList.get());
            if (value)
                m_valueList->next();
            break;
        }
        case CSSPropertyWebkitPerspectiveOriginY: {
            value = parseFillPositionY(m_valueList.get());
            if (value)
                m_valueList->next();
            break;
        }
        default:
            ASSERT_NOT_REACHED();
            return false;
    }

    return value;
}

bool CSSPropertyParser::parseTouchAction(bool important)
{
    if (!RuntimeEnabledFeatures::cssTouchActionEnabled())
        return false;

    CSSParserValue* value = m_valueList->current();
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    if (m_valueList->size() == 1 && value && (value->id == CSSValueAuto || value->id == CSSValueNone)) {
        list->append(cssValuePool().createIdentifierValue(value->id));
        addProperty(CSSPropertyTouchAction, list.release(), important);
        m_valueList->next();
        return true;
    }

    bool isValid = true;
    while (isValid && value) {
        switch (value->id) {
        case CSSValuePanX:
        case CSSValuePanY: {
            RefPtrWillBeRawPtr<CSSValue> panValue = cssValuePool().createIdentifierValue(value->id);
            if (list->hasValue(panValue.get())) {
                isValid = false;
                break;
            }
            list->append(panValue.release());
            break;
        }
        default:
            isValid = false;
            break;
        }
        if (isValid)
            value = m_valueList->next();
    }

    if (list->length() && isValid) {
        addProperty(CSSPropertyTouchAction, list.release(), important);
        return true;
    }

    return false;
}

void CSSPropertyParser::addTextDecorationProperty(CSSPropertyID propId, PassRefPtrWillBeRawPtr<CSSValue> value, bool important)
{
    // The text-decoration-line property takes priority over text-decoration, unless the latter has important priority set.
    if (propId == CSSPropertyTextDecoration && !important && !inShorthand()) {
        for (unsigned i = 0; i < m_parsedProperties.size(); ++i) {
            if (m_parsedProperties[i].id() == CSSPropertyTextDecorationLine)
                return;
        }
    }
    addProperty(propId, value, important);
}

bool CSSPropertyParser::parseTextDecoration(CSSPropertyID propId, bool important)
{
    if (propId == CSSPropertyTextDecorationLine
        && !RuntimeEnabledFeatures::css3TextDecorationsEnabled())
        return false;

    CSSParserValue* value = m_valueList->current();
    if (value && value->id == CSSValueNone) {
        addTextDecorationProperty(propId, cssValuePool().createIdentifierValue(CSSValueNone), important);
        m_valueList->next();
        return true;
    }

    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();
    bool isValid = true;
    while (isValid && value) {
        switch (value->id) {
        case CSSValueUnderline:
        case CSSValueOverline:
        case CSSValueLineThrough:
        case CSSValueBlink:
            list->append(cssValuePool().createIdentifierValue(value->id));
            break;
        default:
            isValid = false;
            break;
        }
        if (isValid)
            value = m_valueList->next();
    }

    // Values are either valid or in shorthand scope.
    if (list->length() && (isValid || inShorthand())) {
        addTextDecorationProperty(propId, list.release(), important);
        return true;
    }

    return false;
}

bool CSSPropertyParser::parseTextUnderlinePosition(bool important)
{
    // The text-underline-position property has syntax "auto | [ under || [ left | right ] ]".
    // However, values 'left' and 'right' are not implemented yet, so we will parse syntax
    // "auto | under" for now.
    CSSParserValue* value = m_valueList->current();
    switch (value->id) {
    case CSSValueAuto:
    case CSSValueUnder:
        if (m_valueList->next())
            return false;
        addProperty(CSSPropertyTextUnderlinePosition, cssValuePool().createIdentifierValue(value->id), important);
        return true;
    default:
        return false;
    }
}

bool CSSPropertyParser::parseTextEmphasisStyle(bool important)
{
    unsigned valueListSize = m_valueList->size();

    RefPtrWillBeRawPtr<CSSPrimitiveValue> fill;
    RefPtrWillBeRawPtr<CSSPrimitiveValue> shape;

    for (CSSParserValue* value = m_valueList->current(); value; value = m_valueList->next()) {
        if (value->unit == CSSPrimitiveValue::CSS_STRING) {
            if (fill || shape || (valueListSize != 1 && !inShorthand()))
                return false;
            addProperty(CSSPropertyWebkitTextEmphasisStyle, createPrimitiveStringValue(value), important);
            m_valueList->next();
            return true;
        }

        if (value->id == CSSValueNone) {
            if (fill || shape || (valueListSize != 1 && !inShorthand()))
                return false;
            addProperty(CSSPropertyWebkitTextEmphasisStyle, cssValuePool().createIdentifierValue(CSSValueNone), important);
            m_valueList->next();
            return true;
        }

        if (value->id == CSSValueOpen || value->id == CSSValueFilled) {
            if (fill)
                return false;
            fill = cssValuePool().createIdentifierValue(value->id);
        } else if (value->id == CSSValueDot || value->id == CSSValueCircle || value->id == CSSValueDoubleCircle || value->id == CSSValueTriangle || value->id == CSSValueSesame) {
            if (shape)
                return false;
            shape = cssValuePool().createIdentifierValue(value->id);
        } else if (!inShorthand())
            return false;
        else
            break;
    }

    if (fill && shape) {
        RefPtrWillBeRawPtr<CSSValueList> parsedValues = CSSValueList::createSpaceSeparated();
        parsedValues->append(fill.release());
        parsedValues->append(shape.release());
        addProperty(CSSPropertyWebkitTextEmphasisStyle, parsedValues.release(), important);
        return true;
    }
    if (fill) {
        addProperty(CSSPropertyWebkitTextEmphasisStyle, fill.release(), important);
        return true;
    }
    if (shape) {
        addProperty(CSSPropertyWebkitTextEmphasisStyle, shape.release(), important);
        return true;
    }

    return false;
}

PassRefPtrWillBeRawPtr<CSSValue> CSSPropertyParser::parseTextIndent()
{
    RefPtrWillBeRawPtr<CSSValueList> list = CSSValueList::createSpaceSeparated();

    // <length> | <percentage> | inherit
    if (m_valueList->size() == 1) {
        CSSParserValue* value = m_valueList->current();
        if (!value->id && validUnit(value, FLength | FPercent)) {
            list->append(createPrimitiveNumericValue(value));
            m_valueList->next();
            return list.release();
        }
    }

    if (!RuntimeEnabledFeatures::css3TextEnabled())
        return nullptr;

    // The case where text-indent has only <length>(or <percentage>) value
    // is handled above if statement even though css3TextEnabled() returns true.

    // [ [ <length> | <percentage> ] && each-line ] | inherit
    if (m_valueList->size() != 2)
        return nullptr;

    CSSParserValue* firstValue = m_valueList->current();
    CSSParserValue* secondValue = m_valueList->next();
    CSSParserValue* lengthOrPercentageValue = 0;

    // [ <length> | <percentage> ] each-line
    if (validUnit(firstValue, FLength | FPercent) && secondValue->id == CSSValueEachLine)
        lengthOrPercentageValue = firstValue;
    // each-line [ <length> | <percentage> ]
    else if (firstValue->id == CSSValueEachLine && validUnit(secondValue, FLength | FPercent))
        lengthOrPercentageValue = secondValue;

    if (lengthOrPercentageValue) {
        list->append(createPrimitiveNumericValue(lengthOrPercentageValue));
        list->append(cssValuePool().createIdentifierValue(CSSValueEachLine));
        m_valueList->next();
        return list.release();
    }

    return nullptr;
}

bool CSSPropertyParser::parseLineBoxContain(bool important)
{
    LineBoxContain lineBoxContain = LineBoxContainNone;

    for (CSSParserValue* value = m_valueList->current(); value; value = m_valueList->next()) {
        if (value->id == CSSValueBlock) {
            if (lineBoxContain & LineBoxContainBlock)
                return false;
            lineBoxContain |= LineBoxContainBlock;
        } else if (value->id == CSSValueInline) {
            if (lineBoxContain & LineBoxContainInline)
                return false;
            lineBoxContain |= LineBoxContainInline;
        } else if (value->id == CSSValueFont) {
            if (lineBoxContain & LineBoxContainFont)
                return false;
            lineBoxContain |= LineBoxContainFont;
        } else if (value->id == CSSValueGlyphs) {
            if (lineBoxContain & LineBoxContainGlyphs)
                return false;
            lineBoxContain |= LineBoxContainGlyphs;
        } else if (value->id == CSSValueReplaced) {
            if (lineBoxContain & LineBoxContainReplaced)
                return false;
            lineBoxContain |= LineBoxContainReplaced;
        } else if (value->id == CSSValueInlineBox) {
            if (lineBoxContain & LineBoxContainInlineBox)
                return false;
            lineBoxContain |= LineBoxContainInlineBox;
        } else
            return false;
    }

    if (!lineBoxContain)
        return false;

    addProperty(CSSPropertyWebkitLineBoxContain, CSSLineBoxContainValue::create(lineBoxContain), important);
    return true;
}

bool CSSPropertyParser::parseFontFeatureTag(CSSValueList* settings)
{
    // Feature tag name consists of 4-letter characters.
    static const unsigned tagNameLength = 4;

    CSSParserValue* value = m_valueList->current();
    // Feature tag name comes first
    if (value->unit != CSSPrimitiveValue::CSS_STRING)
        return false;
    if (value->string.length() != tagNameLength)
        return false;
    for (unsigned i = 0; i < tagNameLength; ++i) {
        // Limits the range of characters to 0x20-0x7E, following the tag name rules defiend in the OpenType specification.
        UChar character = value->string[i];
        if (character < 0x20 || character > 0x7E)
            return false;
    }

    AtomicString tag = value->string;
    int tagValue = 1;
    // Feature tag values could follow: <integer> | on | off
    value = m_valueList->next();
    if (value) {
        if (value->unit == CSSPrimitiveValue::CSS_NUMBER && value->isInt && value->fValue >= 0) {
            tagValue = clampToInteger(value->fValue);
            if (tagValue < 0)
                return false;
            m_valueList->next();
        } else if (value->id == CSSValueOn || value->id == CSSValueOff) {
            tagValue = value->id == CSSValueOn;
            m_valueList->next();
        }
    }
    settings->append(CSSFontFeatureValue::create(tag, tagValue));
    return true;
}

bool CSSPropertyParser::parseFontFeatureSettings(bool important)
{
    if (m_valueList->size() == 1 && m_valueList->current()->id == CSSValueNormal) {
        RefPtrWillBeRawPtr<CSSPrimitiveValue> normalValue = cssValuePool().createIdentifierValue(CSSValueNormal);
        m_valueList->next();
        addProperty(CSSPropertyWebkitFontFeatureSettings, normalValue.release(), important);
        return true;
    }

    RefPtrWillBeRawPtr<CSSValueList> settings = CSSValueList::createCommaSeparated();
    for (CSSParserValue* value = m_valueList->current(); value; value = m_valueList->next()) {
        if (!parseFontFeatureTag(settings.get()))
            return false;

        // If the list isn't parsed fully, the current value should be comma.
        value = m_valueList->current();
        if (value && !isComma(value))
            return false;
    }
    if (settings->length()) {
        addProperty(CSSPropertyWebkitFontFeatureSettings, settings.release(), important);
        return true;
    }
    return false;
}

bool CSSPropertyParser::parseFontVariantLigatures(bool important)
{
    RefPtrWillBeRawPtr<CSSValueList> ligatureValues = CSSValueList::createSpaceSeparated();
    bool sawCommonLigaturesValue = false;
    bool sawDiscretionaryLigaturesValue = false;
    bool sawHistoricalLigaturesValue = false;

    for (CSSParserValue* value = m_valueList->current(); value; value = m_valueList->next()) {
        if (value->unit != CSSPrimitiveValue::CSS_IDENT)
            return false;

        switch (value->id) {
        case CSSValueNoCommonLigatures:
        case CSSValueCommonLigatures:
            if (sawCommonLigaturesValue)
                return false;
            sawCommonLigaturesValue = true;
            ligatureValues->append(cssValuePool().createIdentifierValue(value->id));
            break;
        case CSSValueNoDiscretionaryLigatures:
        case CSSValueDiscretionaryLigatures:
            if (sawDiscretionaryLigaturesValue)
                return false;
            sawDiscretionaryLigaturesValue = true;
            ligatureValues->append(cssValuePool().createIdentifierValue(value->id));
            break;
        case CSSValueNoHistoricalLigatures:
        case CSSValueHistoricalLigatures:
            if (sawHistoricalLigaturesValue)
                return false;
            sawHistoricalLigaturesValue = true;
            ligatureValues->append(cssValuePool().createIdentifierValue(value->id));
            break;
        default:
            return false;
        }
    }

    if (!ligatureValues->length())
        return false;

    addProperty(CSSPropertyFontVariantLigatures, ligatureValues.release(), important);
    return true;
}

bool CSSPropertyParser::parseCalculation(CSSParserValue* value, ValueRange range)
{
    ASSERT(isCalculation(value));

    CSSParserValueList* args = value->function->args.get();
    if (!args || !args->size())
        return false;

    ASSERT(!m_parsedCalculation);
    m_parsedCalculation = CSSCalcValue::create(value->function->name, args, range);

    if (!m_parsedCalculation)
        return false;

    return true;
}

void BisonCSSParser::ensureLineEndings()
{
    if (!m_lineEndings)
        m_lineEndings = lineEndings(*m_source);
}

CSSParserSelector* BisonCSSParser::createFloatingSelectorWithTagName(const QualifiedName& tagQName)
{
    CSSParserSelector* selector = new CSSParserSelector(tagQName);
    m_floatingSelectors.append(selector);
    return selector;
}

CSSParserSelector* BisonCSSParser::createFloatingSelector()
{
    CSSParserSelector* selector = new CSSParserSelector;
    m_floatingSelectors.append(selector);
    return selector;
}

PassOwnPtr<CSSParserSelector> BisonCSSParser::sinkFloatingSelector(CSSParserSelector* selector)
{
    if (selector) {
        size_t index = m_floatingSelectors.reverseFind(selector);
        ASSERT(index != kNotFound);
        m_floatingSelectors.remove(index);
    }
    return adoptPtr(selector);
}

Vector<OwnPtr<CSSParserSelector> >* BisonCSSParser::createFloatingSelectorVector()
{
    Vector<OwnPtr<CSSParserSelector> >* selectorVector = new Vector<OwnPtr<CSSParserSelector> >;
    m_floatingSelectorVectors.append(selectorVector);
    return selectorVector;
}

PassOwnPtr<Vector<OwnPtr<CSSParserSelector> > > BisonCSSParser::sinkFloatingSelectorVector(Vector<OwnPtr<CSSParserSelector> >* selectorVector)
{
    if (selectorVector) {
        size_t index = m_floatingSelectorVectors.reverseFind(selectorVector);
        ASSERT(index != kNotFound);
        m_floatingSelectorVectors.remove(index);
    }
    return adoptPtr(selectorVector);
}

CSSParserValueList* BisonCSSParser::createFloatingValueList()
{
    CSSParserValueList* list = new CSSParserValueList;
    m_floatingValueLists.append(list);
    return list;
}

PassOwnPtr<CSSParserValueList> BisonCSSParser::sinkFloatingValueList(CSSParserValueList* list)
{
    if (list) {
        size_t index = m_floatingValueLists.reverseFind(list);
        ASSERT(index != kNotFound);
        m_floatingValueLists.remove(index);
    }
    return adoptPtr(list);
}

CSSParserFunction* BisonCSSParser::createFloatingFunction()
{
    CSSParserFunction* function = new CSSParserFunction;
    m_floatingFunctions.append(function);
    return function;
}

CSSParserFunction* BisonCSSParser::createFloatingFunction(const CSSParserString& name, PassOwnPtr<CSSParserValueList> args)
{
    CSSParserFunction* function = createFloatingFunction();
    function->name = name;
    function->args = args;
    return function;
}

PassOwnPtr<CSSParserFunction> BisonCSSParser::sinkFloatingFunction(CSSParserFunction* function)
{
    if (function) {
        size_t index = m_floatingFunctions.reverseFind(function);
        ASSERT(index != kNotFound);
        m_floatingFunctions.remove(index);
    }
    return adoptPtr(function);
}

CSSParserValue& BisonCSSParser::sinkFloatingValue(CSSParserValue& value)
{
    if (value.unit == CSSParserValue::Function) {
        size_t index = m_floatingFunctions.reverseFind(value.function);
        ASSERT(index != kNotFound);
        m_floatingFunctions.remove(index);
    }
    return value;
}

MediaQueryExp* BisonCSSParser::createFloatingMediaQueryExp(const AtomicString& mediaFeature, CSSParserValueList* values)
{
    m_floatingMediaQueryExp = MediaQueryExp::create(mediaFeature, values);
    return m_floatingMediaQueryExp.get();
}

PassOwnPtr<MediaQueryExp> BisonCSSParser::sinkFloatingMediaQueryExp(MediaQueryExp* expression)
{
    ASSERT_UNUSED(expression, expression == m_floatingMediaQueryExp);
    return m_floatingMediaQueryExp.release();
}

Vector<OwnPtr<MediaQueryExp> >* BisonCSSParser::createFloatingMediaQueryExpList()
{
    m_floatingMediaQueryExpList = adoptPtr(new Vector<OwnPtr<MediaQueryExp> >);
    return m_floatingMediaQueryExpList.get();
}

PassOwnPtr<Vector<OwnPtr<MediaQueryExp> > > BisonCSSParser::sinkFloatingMediaQueryExpList(Vector<OwnPtr<MediaQueryExp> >* list)
{
    ASSERT_UNUSED(list, list == m_floatingMediaQueryExpList);
    return m_floatingMediaQueryExpList.release();
}

MediaQuery* BisonCSSParser::createFloatingMediaQuery(MediaQuery::Restrictor restrictor, const AtomicString& mediaType, PassOwnPtr<Vector<OwnPtr<MediaQueryExp> > > expressions)
{
    m_floatingMediaQuery = adoptPtr(new MediaQuery(restrictor, mediaType, expressions));
    return m_floatingMediaQuery.get();
}

MediaQuery* BisonCSSParser::createFloatingMediaQuery(PassOwnPtr<Vector<OwnPtr<MediaQueryExp> > > expressions)
{
    return createFloatingMediaQuery(MediaQuery::None, AtomicString("all", AtomicString::ConstructFromLiteral), expressions);
}

MediaQuery* BisonCSSParser::createFloatingNotAllQuery()
{
    return createFloatingMediaQuery(MediaQuery::Not, AtomicString("all", AtomicString::ConstructFromLiteral), sinkFloatingMediaQueryExpList(createFloatingMediaQueryExpList()));
}

PassOwnPtr<MediaQuery> BisonCSSParser::sinkFloatingMediaQuery(MediaQuery* query)
{
    ASSERT_UNUSED(query, query == m_floatingMediaQuery);
    return m_floatingMediaQuery.release();
}

Vector<RefPtr<StyleKeyframe> >* BisonCSSParser::createFloatingKeyframeVector()
{
    m_floatingKeyframeVector = adoptPtr(new Vector<RefPtr<StyleKeyframe> >());
    return m_floatingKeyframeVector.get();
}

PassOwnPtr<Vector<RefPtr<StyleKeyframe> > > BisonCSSParser::sinkFloatingKeyframeVector(Vector<RefPtr<StyleKeyframe> >* keyframeVector)
{
    ASSERT_UNUSED(keyframeVector, m_floatingKeyframeVector == keyframeVector);
    return m_floatingKeyframeVector.release();
}

MediaQuerySet* BisonCSSParser::createMediaQuerySet()
{
    RefPtr<MediaQuerySet> queries = MediaQuerySet::create();
    MediaQuerySet* result = queries.get();
    m_parsedMediaQuerySets.append(queries.release());
    return result;
}

StyleRuleBase* BisonCSSParser::createImportRule(const CSSParserString& url, MediaQuerySet* media)
{
    if (!media || !m_allowImportRules)
        return 0;
    RefPtrWillBeRawPtr<StyleRuleImport> rule = StyleRuleImport::create(url, media);
    StyleRuleImport* result = rule.get();
    m_parsedRules.append(rule.release());
    return result;
}

StyleRuleBase* BisonCSSParser::createMediaRule(MediaQuerySet* media, RuleList* rules)
{
    m_allowImportRules = m_allowNamespaceDeclarations = false;
    RefPtrWillBeRawPtr<StyleRuleMedia> rule;
    if (rules) {
        rule = StyleRuleMedia::create(media ? media : MediaQuerySet::create(), *rules);
    } else {
        RuleList emptyRules;
        rule = StyleRuleMedia::create(media ? media : MediaQuerySet::create(), emptyRules);
    }
    StyleRuleMedia* result = rule.get();
    m_parsedRules.append(rule.release());
    return result;
}

StyleRuleBase* BisonCSSParser::createSupportsRule(bool conditionIsSupported, RuleList* rules)
{
    m_allowImportRules = m_allowNamespaceDeclarations = false;

    RefPtr<CSSRuleSourceData> data = popSupportsRuleData();
    RefPtrWillBeRawPtr<StyleRuleSupports> rule;
    String conditionText;
    unsigned conditionOffset = data->ruleHeaderRange.start + 9;
    unsigned conditionLength = data->ruleHeaderRange.length() - 9;

    if (m_tokenizer.is8BitSource())
        conditionText = String(m_tokenizer.m_dataStart8.get() + conditionOffset, conditionLength).stripWhiteSpace();
    else
        conditionText = String(m_tokenizer.m_dataStart16.get() + conditionOffset, conditionLength).stripWhiteSpace();

    if (rules) {
        rule = StyleRuleSupports::create(conditionText, conditionIsSupported, *rules);
    } else {
        RuleList emptyRules;
        rule = StyleRuleSupports::create(conditionText, conditionIsSupported, emptyRules);
    }

    StyleRuleSupports* result = rule.get();
    m_parsedRules.append(rule.release());

    return result;
}

void BisonCSSParser::markSupportsRuleHeaderStart()
{
    if (!m_supportsRuleDataStack)
        m_supportsRuleDataStack = adoptPtr(new RuleSourceDataList());

    RefPtr<CSSRuleSourceData> data = CSSRuleSourceData::create(CSSRuleSourceData::SUPPORTS_RULE);
    data->ruleHeaderRange.start = m_tokenizer.tokenStartOffset();
    m_supportsRuleDataStack->append(data);
}

void BisonCSSParser::markSupportsRuleHeaderEnd()
{
    ASSERT(m_supportsRuleDataStack && !m_supportsRuleDataStack->isEmpty());

    if (m_tokenizer.is8BitSource())
        m_supportsRuleDataStack->last()->ruleHeaderRange.end = m_tokenizer.tokenStart<LChar>() - m_tokenizer.m_dataStart8.get();
    else
        m_supportsRuleDataStack->last()->ruleHeaderRange.end = m_tokenizer.tokenStart<UChar>() - m_tokenizer.m_dataStart16.get();
}

PassRefPtr<CSSRuleSourceData> BisonCSSParser::popSupportsRuleData()
{
    ASSERT(m_supportsRuleDataStack && !m_supportsRuleDataStack->isEmpty());
    RefPtr<CSSRuleSourceData> data = m_supportsRuleDataStack->last();
    m_supportsRuleDataStack->removeLast();
    return data.release();
}

BisonCSSParser::RuleList* BisonCSSParser::createRuleList()
{
    OwnPtrWillBeRawPtr<RuleList> list = adoptPtrWillBeNoop(new RuleList);
    RuleList* listPtr = list.get();

    m_parsedRuleLists.append(list.release());
    return listPtr;
}

BisonCSSParser::RuleList* BisonCSSParser::appendRule(RuleList* ruleList, StyleRuleBase* rule)
{
    if (rule) {
        if (!ruleList)
            ruleList = createRuleList();
        ruleList->append(rule);
    }
    return ruleList;
}

template <typename CharacterType>
ALWAYS_INLINE static void makeLower(const CharacterType* input, CharacterType* output, unsigned length)
{
    // FIXME: If we need Unicode lowercasing here, then we probably want the real kind
    // that can potentially change the length of the string rather than the character
    // by character kind. If we don't need Unicode lowercasing, it would be good to
    // simplify this function.

    if (charactersAreAllASCII(input, length)) {
        // Fast case for all-ASCII.
        for (unsigned i = 0; i < length; i++)
            output[i] = toASCIILower(input[i]);
    } else {
        for (unsigned i = 0; i < length; i++)
            output[i] = Unicode::toLower(input[i]);
    }
}

void BisonCSSParser::tokenToLowerCase(const CSSParserString& token)
{
    size_t length = token.length();
    if (m_tokenizer.is8BitSource()) {
        size_t offset = token.characters8() - m_tokenizer.m_dataStart8.get();
        makeLower(token.characters8(), m_tokenizer.m_dataStart8.get() + offset, length);
    } else {
        size_t offset = token.characters16() - m_tokenizer.m_dataStart16.get();
        makeLower(token.characters16(), m_tokenizer.m_dataStart16.get() + offset, length);
    }
}

void BisonCSSParser::endInvalidRuleHeader()
{
    if (m_ruleHeaderType == CSSRuleSourceData::UNKNOWN_RULE)
        return;

    CSSParserLocation location;
    location.lineNumber = m_tokenizer.m_lineNumber;
    location.offset = m_ruleHeaderStartOffset;
    if (m_tokenizer.is8BitSource())
        location.token.init(m_tokenizer.m_dataStart8.get() + m_ruleHeaderStartOffset, 0);
    else
        location.token.init(m_tokenizer.m_dataStart16.get() + m_ruleHeaderStartOffset, 0);

    reportError(location, m_ruleHeaderType == CSSRuleSourceData::STYLE_RULE ? InvalidSelectorCSSError : InvalidRuleCSSError);

    endRuleHeader();
}

void BisonCSSParser::reportError(const CSSParserLocation&, CSSParserError)
{
    // FIXME: error reporting temporatily disabled.
}

bool BisonCSSParser::isLoggingErrors()
{
    return m_logErrors && !m_ignoreErrors;
}

void BisonCSSParser::logError(const String& message, const CSSParserLocation& location)
{
    unsigned lineNumberInStyleSheet;
    unsigned columnNumber = 0;
    if (InspectorInstrumentation::hasFrontends()) {
        ensureLineEndings();
        TextPosition tokenPosition = TextPosition::fromOffsetAndLineEndings(location.offset, *m_lineEndings);
        lineNumberInStyleSheet = tokenPosition.m_line.zeroBasedInt();
        columnNumber = (lineNumberInStyleSheet ? 0 : m_startPosition.m_column.zeroBasedInt()) + tokenPosition.m_column.zeroBasedInt();
    } else {
        lineNumberInStyleSheet = location.lineNumber;
    }
    PageConsole& console = m_styleSheet->singleOwnerDocument()->frameHost()->console();
    console.addMessage(CSSMessageSource, WarningMessageLevel, message, m_styleSheet->baseURL().string(), lineNumberInStyleSheet + m_startPosition.m_line.zeroBasedInt() + 1, columnNumber + 1);
}

StyleRuleKeyframes* BisonCSSParser::createKeyframesRule(const String& name, PassOwnPtr<Vector<RefPtr<StyleKeyframe> > > popKeyframes, bool isPrefixed)
{
    OwnPtr<Vector<RefPtr<StyleKeyframe> > > keyframes = popKeyframes;
    m_allowImportRules = m_allowNamespaceDeclarations = false;
    RefPtrWillBeRawPtr<StyleRuleKeyframes> rule = StyleRuleKeyframes::create();
    for (size_t i = 0; i < keyframes->size(); ++i)
        rule->parserAppendKeyframe(keyframes->at(i));
    rule->setName(name);
    rule->setVendorPrefixed(isPrefixed);
    StyleRuleKeyframes* rulePtr = rule.get();
    m_parsedRules.append(rule.release());
    return rulePtr;
}

StyleRuleBase* BisonCSSParser::createStyleRule(Vector<OwnPtr<CSSParserSelector> >* selectors)
{
    StyleRule* result = 0;
    if (selectors) {
        m_allowImportRules = m_allowNamespaceDeclarations = false;
        RefPtrWillBeRawPtr<StyleRule> rule = StyleRule::create();
        rule->parserAdoptSelectorVector(*selectors);
        if (m_hasFontFaceOnlyValues)
            deleteFontFaceOnlyValues();
        rule->setProperties(createStylePropertySet());
        result = rule.get();
        m_parsedRules.append(rule.release());
    }
    clearProperties();
    return result;
}

StyleRuleBase* BisonCSSParser::createFontFaceRule()
{
    m_allowImportRules = m_allowNamespaceDeclarations = false;
    for (unsigned i = 0; i < m_parsedProperties.size(); ++i) {
        CSSProperty& property = m_parsedProperties[i];
        if (property.id() == CSSPropertyFontVariant && property.value()->isPrimitiveValue())
            property.wrapValueInCommaSeparatedList();
        else if (property.id() == CSSPropertyFontFamily && (!property.value()->isValueList() || toCSSValueList(property.value())->length() != 1)) {
            // Unlike font-family property, font-family descriptor in @font-face rule
            // has to be a value list with exactly one family name. It cannot have a
            // have 'initial' value and cannot 'inherit' from parent.
            // See http://dev.w3.org/csswg/css3-fonts/#font-family-desc
            clearProperties();
            return 0;
        }
    }
    RefPtrWillBeRawPtr<StyleRuleFontFace> rule = StyleRuleFontFace::create();
    rule->setProperties(createStylePropertySet());
    clearProperties();
    StyleRuleFontFace* result = rule.get();
    m_parsedRules.append(rule.release());
    if (m_styleSheet)
        m_styleSheet->setHasFontFaceRule(true);
    return result;
}

void BisonCSSParser::addNamespace(const AtomicString& prefix, const AtomicString& uri)
{
    if (!m_styleSheet || !m_allowNamespaceDeclarations)
        return;
    m_allowImportRules = false;
    m_styleSheet->parserAddNamespace(prefix, uri);
    if (prefix.isEmpty() && !uri.isNull())
        m_defaultNamespace = uri;
}

QualifiedName BisonCSSParser::determineNameInNamespace(const AtomicString& prefix, const AtomicString& localName)
{
    if (!m_styleSheet)
        return QualifiedName(prefix, localName, m_defaultNamespace);
    return QualifiedName(prefix, localName, m_styleSheet->determineNamespace(prefix));
}

CSSParserSelector* BisonCSSParser::rewriteSpecifiersWithNamespaceIfNeeded(CSSParserSelector* specifiers)
{
    if (m_defaultNamespace != starAtom || specifiers->needsCrossingTreeScopeBoundary())
        return rewriteSpecifiersWithElementName(nullAtom, starAtom, specifiers, /*tagIsForNamespaceRule*/true);
    if (CSSParserSelector* distributedPseudoElementSelector = specifiers->findDistributedPseudoElementSelector()) {
        specifiers->prependTagSelector(QualifiedName(nullAtom, starAtom, m_defaultNamespace), /*tagIsForNamespaceRule*/true);
        return rewriteSpecifiersForShadowDistributed(specifiers, distributedPseudoElementSelector);
    }
    return specifiers;
}

CSSParserSelector* BisonCSSParser::rewriteSpecifiersWithElementName(const AtomicString& namespacePrefix, const AtomicString& elementName, CSSParserSelector* specifiers, bool tagIsForNamespaceRule)
{
    AtomicString determinedNamespace = namespacePrefix != nullAtom && m_styleSheet ? m_styleSheet->determineNamespace(namespacePrefix) : m_defaultNamespace;
    QualifiedName tag(namespacePrefix, elementName, determinedNamespace);

    if (CSSParserSelector* distributedPseudoElementSelector = specifiers->findDistributedPseudoElementSelector()) {
        specifiers->prependTagSelector(tag, tagIsForNamespaceRule);
        return rewriteSpecifiersForShadowDistributed(specifiers, distributedPseudoElementSelector);
    }

    if (specifiers->needsCrossingTreeScopeBoundary())
        return rewriteSpecifiersWithElementNameForCustomPseudoElement(tag, elementName, specifiers, tagIsForNamespaceRule);

    if (specifiers->isContentPseudoElement())
        return rewriteSpecifiersWithElementNameForContentPseudoElement(tag, elementName, specifiers, tagIsForNamespaceRule);

    if (tag == anyQName())
        return specifiers;
    if (!(specifiers->pseudoType() == CSSSelector::PseudoCue))
        specifiers->prependTagSelector(tag, tagIsForNamespaceRule);
    return specifiers;
}

CSSParserSelector* BisonCSSParser::rewriteSpecifiersWithElementNameForCustomPseudoElement(const QualifiedName& tag, const AtomicString& elementName, CSSParserSelector* specifiers, bool tagIsForNamespaceRule)
{
    if (m_context.useCounter() && specifiers->pseudoType() == CSSSelector::PseudoUserAgentCustomElement)
        m_context.useCounter()->count(UseCounter::CSSPseudoElementUserAgentCustomPseudo);

    CSSParserSelector* lastShadowPseudo = specifiers;
    CSSParserSelector* history = specifiers;
    while (history->tagHistory()) {
        history = history->tagHistory();
        if (history->needsCrossingTreeScopeBoundary() || history->hasShadowPseudo())
            lastShadowPseudo = history;
    }

    if (lastShadowPseudo->tagHistory()) {
        if (tag != anyQName())
            lastShadowPseudo->tagHistory()->prependTagSelector(tag, tagIsForNamespaceRule);
        return specifiers;
    }

    // For shadow-ID pseudo-elements to be correctly matched, the ShadowPseudo combinator has to be used.
    // We therefore create a new Selector with that combinator here in any case, even if matching any (host) element in any namespace (i.e. '*').
    OwnPtr<CSSParserSelector> elementNameSelector = adoptPtr(new CSSParserSelector(tag));
    lastShadowPseudo->setTagHistory(elementNameSelector.release());
    lastShadowPseudo->setRelation(CSSSelector::ShadowPseudo);
    return specifiers;
}

CSSParserSelector* BisonCSSParser::rewriteSpecifiersWithElementNameForContentPseudoElement(const QualifiedName& tag, const AtomicString& elementName, CSSParserSelector* specifiers, bool tagIsForNamespaceRule)
{
    CSSParserSelector* last = specifiers;
    CSSParserSelector* history = specifiers;
    while (history->tagHistory()) {
        history = history->tagHistory();
        if (history->isContentPseudoElement() || history->relationIsAffectedByPseudoContent())
            last = history;
    }

    if (last->tagHistory()) {
        if (tag != anyQName())
            last->tagHistory()->prependTagSelector(tag, tagIsForNamespaceRule);
        return specifiers;
    }

    // For shadow-ID pseudo-elements to be correctly matched, the ShadowPseudo combinator has to be used.
    // We therefore create a new Selector with that combinator here in any case, even if matching any (host) element in any namespace (i.e. '*').
    OwnPtr<CSSParserSelector> elementNameSelector = adoptPtr(new CSSParserSelector(tag));
    last->setTagHistory(elementNameSelector.release());
    last->setRelation(CSSSelector::SubSelector);
    return specifiers;
}

CSSParserSelector* BisonCSSParser::rewriteSpecifiersForShadowDistributed(CSSParserSelector* specifiers, CSSParserSelector* distributedPseudoElementSelector)
{
    if (m_context.useCounter())
        m_context.useCounter()->count(UseCounter::CSSPseudoElementPrefixedDistributed);
    CSSParserSelector* argumentSelector = distributedPseudoElementSelector->functionArgumentSelector();
    ASSERT(argumentSelector);
    ASSERT(!specifiers->isDistributedPseudoElement());
    for (CSSParserSelector* end = specifiers; end->tagHistory(); end = end->tagHistory()) {
        if (end->tagHistory()->isDistributedPseudoElement()) {
            end->clearTagHistory();
            break;
        }
    }
    CSSParserSelector* end = argumentSelector;
    while (end->tagHistory())
        end = end->tagHistory();

    switch (end->relation()) {
    case CSSSelector::Child:
    case CSSSelector::Descendant:
        end->setTagHistory(sinkFloatingSelector(specifiers));
        end->setRelationIsAffectedByPseudoContent();
        return argumentSelector;
    default:
        return 0;
    }
}

CSSParserSelector* BisonCSSParser::rewriteSpecifiers(CSSParserSelector* specifiers, CSSParserSelector* newSpecifier)
{
    if (newSpecifier->needsCrossingTreeScopeBoundary()) {
        // Unknown pseudo element always goes at the top of selector chain.
        newSpecifier->appendTagHistory(CSSSelector::ShadowPseudo, sinkFloatingSelector(specifiers));
        return newSpecifier;
    }
    if (newSpecifier->isContentPseudoElement()) {
        newSpecifier->appendTagHistory(CSSSelector::SubSelector, sinkFloatingSelector(specifiers));
        return newSpecifier;
    }
    if (specifiers->needsCrossingTreeScopeBoundary()) {
        // Specifiers for unknown pseudo element go right behind it in the chain.
        specifiers->insertTagHistory(CSSSelector::SubSelector, sinkFloatingSelector(newSpecifier), CSSSelector::ShadowPseudo);
        return specifiers;
    }
    if (specifiers->isContentPseudoElement()) {
        specifiers->insertTagHistory(CSSSelector::SubSelector, sinkFloatingSelector(newSpecifier), CSSSelector::SubSelector);
        return specifiers;
    }
    specifiers->appendTagHistory(CSSSelector::SubSelector, sinkFloatingSelector(newSpecifier));
    return specifiers;
}

StyleRuleBase* BisonCSSParser::createPageRule(PassOwnPtr<CSSParserSelector> pageSelector)
{
    // FIXME: Margin at-rules are ignored.
    m_allowImportRules = m_allowNamespaceDeclarations = false;
    StyleRulePage* pageRule = 0;
    if (pageSelector) {
        RefPtrWillBeRawPtr<StyleRulePage> rule = StyleRulePage::create();
        Vector<OwnPtr<CSSParserSelector> > selectorVector;
        selectorVector.append(pageSelector);
        rule->parserAdoptSelectorVector(selectorVector);
        rule->setProperties(createStylePropertySet());
        pageRule = rule.get();
        m_parsedRules.append(rule.release());
    }
    clearProperties();
    return pageRule;
}

StyleRuleBase* BisonCSSParser::createMarginAtRule(CSSSelector::MarginBoxType /* marginBox */)
{
    // FIXME: Implement margin at-rule here, using:
    //        - marginBox: margin box
    //        - m_parsedProperties: properties at [m_numParsedPropertiesBeforeMarginBox, m_parsedProperties.size()] are for this at-rule.
    // Don't forget to also update the action for page symbol in CSSGrammar.y such that margin at-rule data is cleared if page_selector is invalid.

    endDeclarationsForMarginBox();
    return 0; // until this method is implemented.
}

void BisonCSSParser::startDeclarationsForMarginBox()
{
    m_numParsedPropertiesBeforeMarginBox = m_parsedProperties.size();
}

void BisonCSSParser::endDeclarationsForMarginBox()
{
    rollbackLastProperties(m_parsedProperties.size() - m_numParsedPropertiesBeforeMarginBox);
    m_numParsedPropertiesBeforeMarginBox = INVALID_NUM_PARSED_PROPERTIES;
}

void BisonCSSParser::deleteFontFaceOnlyValues()
{
    ASSERT(m_hasFontFaceOnlyValues);
    for (unsigned i = 0; i < m_parsedProperties.size();) {
        CSSProperty& property = m_parsedProperties[i];
        if (property.id() == CSSPropertyFontVariant && property.value()->isValueList()) {
            m_parsedProperties.remove(i);
            continue;
        }
        ++i;
    }
}

StyleKeyframe* BisonCSSParser::createKeyframe(CSSParserValueList* keys)
{
    OwnPtr<Vector<double> > keyVector = StyleKeyframe::createKeyList(keys);
    if (keyVector->isEmpty())
        return 0;

    RefPtr<StyleKeyframe> keyframe = StyleKeyframe::create();
    keyframe->setKeys(keyVector.release());
    keyframe->setProperties(createStylePropertySet());

    clearProperties();

    StyleKeyframe* keyframePtr = keyframe.get();
    m_parsedKeyframes.append(keyframe.release());
    return keyframePtr;
}

void BisonCSSParser::invalidBlockHit()
{
    if (m_styleSheet && !m_hadSyntacticallyValidCSSRule)
        m_styleSheet->setHasSyntacticallyValidCSSHeader(false);
}

void BisonCSSParser::startRule()
{
    if (!m_observer)
        return;

    ASSERT(m_ruleHasHeader);
    m_ruleHasHeader = false;
}

void BisonCSSParser::endRule(bool valid)
{
    if (!m_observer)
        return;

    if (m_ruleHasHeader)
        m_observer->endRuleBody(m_tokenizer.safeUserStringTokenOffset(), !valid);
    m_ruleHasHeader = true;
}

void BisonCSSParser::startRuleHeader(CSSRuleSourceData::Type ruleType)
{
    resumeErrorLogging();
    m_ruleHeaderType = ruleType;
    m_ruleHeaderStartOffset = m_tokenizer.safeUserStringTokenOffset();
    m_ruleHeaderStartLineNumber = m_tokenizer.m_tokenStartLineNumber;
    if (m_observer) {
        ASSERT(!m_ruleHasHeader);
        m_observer->startRuleHeader(ruleType, m_ruleHeaderStartOffset);
        m_ruleHasHeader = true;
    }
}

void BisonCSSParser::endRuleHeader()
{
    ASSERT(m_ruleHeaderType != CSSRuleSourceData::UNKNOWN_RULE);
    m_ruleHeaderType = CSSRuleSourceData::UNKNOWN_RULE;
    if (m_observer) {
        ASSERT(m_ruleHasHeader);
        m_observer->endRuleHeader(m_tokenizer.safeUserStringTokenOffset());
    }
}

void BisonCSSParser::startSelector()
{
    if (m_observer)
        m_observer->startSelector(m_tokenizer.safeUserStringTokenOffset());
}

void BisonCSSParser::endSelector()
{
    if (m_observer)
        m_observer->endSelector(m_tokenizer.safeUserStringTokenOffset());
}

void BisonCSSParser::startRuleBody()
{
    if (m_observer)
        m_observer->startRuleBody(m_tokenizer.safeUserStringTokenOffset());
}

void BisonCSSParser::startProperty()
{
    resumeErrorLogging();
    if (m_observer)
        m_observer->startProperty(m_tokenizer.safeUserStringTokenOffset());
}

void BisonCSSParser::endProperty(bool isImportantFound, bool isPropertyParsed, CSSParserError errorType)
{
    m_id = CSSPropertyInvalid;
    if (m_observer)
        m_observer->endProperty(isImportantFound, isPropertyParsed, m_tokenizer.safeUserStringTokenOffset(), errorType);
}

void BisonCSSParser::startEndUnknownRule()
{
    if (m_observer)
        m_observer->startEndUnknownRule();
}

StyleRuleBase* BisonCSSParser::createViewportRule()
{
    // Allow @viewport rules from UA stylesheets even if the feature is disabled.
    if (!RuntimeEnabledFeatures::cssViewportEnabled() && !isUASheetBehavior(m_context.mode()))
        return 0;

    m_allowImportRules = m_allowNamespaceDeclarations = false;

    RefPtrWillBeRawPtr<StyleRuleViewport> rule = StyleRuleViewport::create();

    rule->setProperties(createStylePropertySet());
    clearProperties();

    StyleRuleViewport* result = rule.get();
    m_parsedRules.append(rule.release());

    return result;
}

bool CSSPropertyParser::parseViewportProperty(CSSPropertyID propId, bool important)
{
    ASSERT(RuntimeEnabledFeatures::cssViewportEnabled() || isUASheetBehavior(m_context.mode()));

    CSSParserValue* value = m_valueList->current();
    if (!value)
        return false;

    CSSValueID id = value->id;
    bool validPrimitive = false;

    switch (propId) {
    case CSSPropertyMinWidth: // auto | extend-to-zoom | <length> | <percentage>
    case CSSPropertyMaxWidth:
    case CSSPropertyMinHeight:
    case CSSPropertyMaxHeight:
        if (id == CSSValueAuto || id == CSSValueInternalExtendToZoom)
            validPrimitive = true;
        else
            validPrimitive = (!id && validUnit(value, FLength | FPercent | FNonNeg));
        break;
    case CSSPropertyWidth: // shorthand
        return parseViewportShorthand(propId, CSSPropertyMinWidth, CSSPropertyMaxWidth, important);
    case CSSPropertyHeight:
        return parseViewportShorthand(propId, CSSPropertyMinHeight, CSSPropertyMaxHeight, important);
    case CSSPropertyMinZoom: // auto | <number> | <percentage>
    case CSSPropertyMaxZoom:
    case CSSPropertyZoom:
        if (id == CSSValueAuto)
            validPrimitive = true;
        else
            validPrimitive = (!id && validUnit(value, FNumber | FPercent | FNonNeg));
        break;
    case CSSPropertyUserZoom: // zoom | fixed
        if (id == CSSValueZoom || id == CSSValueFixed)
            validPrimitive = true;
        break;
    case CSSPropertyOrientation: // auto | portrait | landscape
        if (id == CSSValueAuto || id == CSSValuePortrait || id == CSSValueLandscape)
            validPrimitive = true;
    default:
        break;
    }

    RefPtrWillBeRawPtr<CSSValue> parsedValue;
    if (validPrimitive) {
        parsedValue = parseValidPrimitive(id, value);
        m_valueList->next();
    }

    if (parsedValue) {
        if (!m_valueList->current() || inShorthand()) {
            addProperty(propId, parsedValue.release(), important);
            return true;
        }
    }

    return false;
}

bool CSSPropertyParser::parseViewportShorthand(CSSPropertyID propId, CSSPropertyID first, CSSPropertyID second, bool important)
{
    ASSERT(RuntimeEnabledFeatures::cssViewportEnabled() || isUASheetBehavior(m_context.mode()));
    unsigned numValues = m_valueList->size();

    if (numValues > 2)
        return false;

    ShorthandScope scope(this, propId);

    if (!parseViewportProperty(first, important))
        return false;

    // If just one value is supplied, the second value
    // is implicitly initialized with the first value.
    if (numValues == 1)
        m_valueList->previous();

    return parseViewportProperty(second, important);
}

template <typename CharacterType>
static CSSPropertyID cssPropertyID(const CharacterType* propertyName, unsigned length)
{
    char buffer[maxCSSPropertyNameLength + 1]; // 1 for null character

    for (unsigned i = 0; i != length; ++i) {
        CharacterType c = propertyName[i];
        if (c == 0 || c >= 0x7F)
            return CSSPropertyInvalid; // illegal character
        buffer[i] = toASCIILower(c);
    }
    buffer[length] = '\0';

    const char* name = buffer;
    const Property* hashTableEntry = findProperty(name, length);
    return hashTableEntry ? static_cast<CSSPropertyID>(hashTableEntry->id) : CSSPropertyInvalid;
}

CSSPropertyID cssPropertyID(const String& string)
{
    unsigned length = string.length();

    if (!length)
        return CSSPropertyInvalid;
    if (length > maxCSSPropertyNameLength)
        return CSSPropertyInvalid;

    return string.is8Bit() ? cssPropertyID(string.characters8(), length) : cssPropertyID(string.characters16(), length);
}

CSSPropertyID cssPropertyID(const CSSParserString& string)
{
    unsigned length = string.length();

    if (!length)
        return CSSPropertyInvalid;
    if (length > maxCSSPropertyNameLength)
        return CSSPropertyInvalid;

    return string.is8Bit() ? cssPropertyID(string.characters8(), length) : cssPropertyID(string.characters16(), length);
}

template <typename CharacterType>
static CSSValueID cssValueKeywordID(const CharacterType* valueKeyword, unsigned length)
{
    char buffer[maxCSSValueKeywordLength + 1]; // 1 for null character

    for (unsigned i = 0; i != length; ++i) {
        CharacterType c = valueKeyword[i];
        if (c == 0 || c >= 0x7F)
            return CSSValueInvalid; // illegal character
        buffer[i] = WTF::toASCIILower(c);
    }
    buffer[length] = '\0';

    const Value* hashTableEntry = findValue(buffer, length);
    return hashTableEntry ? static_cast<CSSValueID>(hashTableEntry->id) : CSSValueInvalid;
}

CSSValueID cssValueKeywordID(const CSSParserString& string)
{
    unsigned length = string.length();
    if (!length)
        return CSSValueInvalid;
    if (length > maxCSSValueKeywordLength)
        return CSSValueInvalid;

    return string.is8Bit() ? cssValueKeywordID(string.characters8(), length) : cssValueKeywordID(string.characters16(), length);
}

bool isValidNthToken(const CSSParserString& token)
{
    // The tokenizer checks for the construct of an+b.
    // However, since the {ident} rule precedes the {nth} rule, some of those
    // tokens are identified as string literal. Furthermore we need to accept
    // "odd" and "even" which does not match to an+b.
    return equalIgnoringCase(token, "odd") || equalIgnoringCase(token, "even")
        || equalIgnoringCase(token, "n") || equalIgnoringCase(token, "-n");
}

}
