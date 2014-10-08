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

#include "core/CSSValueKeywords.h"
#include "core/MediaTypeNames.h"
#include "core/StylePropertyShorthand.h"
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
#include "core/dom/Document.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/FrameHost.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/rendering/RenderTheme.h"
#include "platform/FloatConversion.h"
#include "platform/RuntimeEnabledFeatures.h"
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

int cssyyparse(blink::BisonCSSParser*);

using namespace WTF;

namespace blink {

static const unsigned INVALID_NUM_PARSED_PROPERTIES = UINT_MAX;

BisonCSSParser::BisonCSSParser(const CSSParserContext& context)
    : m_context(context)
    , m_important(false)
    , m_id(CSSPropertyInvalid)
    , m_styleSheet(nullptr)
    , m_supportsCondition(false)
    , m_selectorListForParseSelector(0)
    , m_numParsedPropertiesBeforeMarginBox(INVALID_NUM_PARSED_PROPERTIES)
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

PassRefPtrWillBeRawPtr<StyleKeyframe> BisonCSSParser::parseKeyframeRule(StyleSheetContents* sheet, const String& string)
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

bool BisonCSSParser::parseValue(MutableStylePropertySet* declaration, CSSPropertyID propertyID, const String& string, bool important, const CSSParserContext& context)
{
    ASSERT(!string.isEmpty());
    BisonCSSParser parser(context);
    return parser.parseValue(declaration, propertyID, string, important);
}

bool BisonCSSParser::parseValue(MutableStylePropertySet* declaration, CSSPropertyID propertyID, const String& string, bool important)
{
    if (m_context.useCounter())
        m_context.useCounter()->count(m_context, propertyID);

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

StyleColor BisonCSSParser::colorFromRGBColorString(const String& colorString)
{
    // FIXME: Rework css parser so it is more SVG aware.
    RGBA32 color;
    if (parseColor(color, colorString.stripWhiteSpace()))
        return StyleColor(color);
    // FIXME: This branch catches the string currentColor, but we should error if we have an illegal color value.
    return StyleColor::currentColor();
}

bool BisonCSSParser::parseColor(const String& string)
{
    setupParser("@-internal-decls color:", string, "");
    cssyyparse(this);
    m_rule = nullptr;

    return !m_parsedProperties.isEmpty() && m_parsedProperties.first().id() == CSSPropertyColor;
}

bool BisonCSSParser::parseSystemColor(RGBA32& color, const String& string)
{
    CSSParserString cssColor;
    cssColor.init(string);
    CSSValueID id = cssValueKeywordID(cssColor);
    if (!CSSPropertyParser::isSystemColor(id))
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

PassRefPtrWillBeRawPtr<ImmutableStylePropertySet> BisonCSSParser::parseInlineStyleDeclaration(const String& string, Element* element)
{
    Document& document = element->document();
    CSSParserContext context = CSSParserContext(document.elementSheet().contents()->parserContext(), UseCounter::getFrom(&document));
    context.setMode((element->isHTMLElement() && !document.inQuirksMode()) ? HTMLStandardMode : HTMLQuirksMode);
    return BisonCSSParser(context).parseDeclaration(string, document.elementSheet().contents());
}

PassRefPtrWillBeRawPtr<ImmutableStylePropertySet> BisonCSSParser::parseDeclaration(const String& string, StyleSheetContents* contextStyleSheet)
{
    setStyleSheet(contextStyleSheet);

    setupParser("@-internal-decls ", string, "");
    cssyyparse(this);
    m_rule = nullptr;

    RefPtrWillBeRawPtr<ImmutableStylePropertySet> style = createStylePropertySet();
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
    if (!m_parsedProperties.isEmpty()) {
        ok = true;
        declaration->addParsedProperties(m_parsedProperties);
        clearProperties();
    }

    if (m_observer)
        m_observer->endRuleBody(string.length(), false);

    return ok;
}

bool BisonCSSParser::parseAttributeMatchType(CSSSelector::AttributeMatchType& matchType, const String& string)
{
    if (!RuntimeEnabledFeatures::cssAttributeCaseSensitivityEnabled() && !isUASheetBehavior(m_context.mode()))
        return false;
    if (string == "i") {
        matchType = CSSSelector::CaseInsensitive;
        return true;
    }
    return false;
}

static inline void filterProperties(bool important, const WillBeHeapVector<CSSProperty, 256>& input, WillBeHeapVector<CSSProperty, 256>& output, size_t& unusedEntries, BitArray<numCSSProperties>& seenProperties)
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

PassRefPtrWillBeRawPtr<ImmutableStylePropertySet> BisonCSSParser::createStylePropertySet()
{
    BitArray<numCSSProperties> seenProperties;
    size_t unusedEntries = m_parsedProperties.size();
    WillBeHeapVector<CSSProperty, 256> results(unusedEntries);

    // Important properties have higher priority, so add them first. Duplicate definitions can then be ignored when found.
    filterProperties(true, m_parsedProperties, results, unusedEntries, seenProperties);
    filterProperties(false, m_parsedProperties, results, unusedEntries, seenProperties);
    if (unusedEntries)
        results.remove(0, unusedEntries);

    CSSParserMode mode = inViewport() ? CSSViewportRuleMode : m_context.mode();

    return ImmutableStylePropertySet::create(results.data(), results.size(), mode);
}

void BisonCSSParser::rollbackLastProperties(int num)
{
    ASSERT(num >= 0);
    ASSERT(m_parsedProperties.size() >= static_cast<unsigned>(num));
    m_parsedProperties.shrink(m_parsedProperties.size() - num);
}

void BisonCSSParser::clearProperties()
{
    m_parsedProperties.clear();
    m_numParsedPropertiesBeforeMarginBox = INVALID_NUM_PARSED_PROPERTIES;
}

void BisonCSSParser::setCurrentProperty(CSSPropertyID propId)
{
    m_id = propId;
}

bool BisonCSSParser::parseValue(CSSPropertyID propId, bool important)
{
    return CSSPropertyParser::parseValue(propId, important, m_valueList.get(), m_context, m_inViewport, m_parsedProperties, m_ruleHeaderType);
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

CSSParserFunction* BisonCSSParser::createFloatingFunction(const CSSParserString& name, PassOwnPtr<CSSParserValueList> args)
{
    CSSParserFunction* function = new CSSParserFunction;
    m_floatingFunctions.append(function);
    function->id = cssValueKeywordID(name);
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

void BisonCSSParser::startMediaValue()
{
    if (!m_observer)
        return;
    m_mediaQueryValueStartOffset = m_tokenizer.safeUserStringTokenOffset();
}

void BisonCSSParser::endMediaValue()
{
    if (!m_observer)
        return;
    m_mediaQueryValueEndOffset = m_tokenizer.safeUserStringTokenOffset();
}

void BisonCSSParser::startMediaQuery()
{
    if (!m_observer)
        return;
    m_observer->startMediaQuery();
}

MediaQueryExp* BisonCSSParser::createFloatingMediaQueryExp(const AtomicString& mediaFeature, CSSParserValueList* values)
{
    m_floatingMediaQueryExp = MediaQueryExp::createIfValid(mediaFeature, values);
    if (m_observer && m_floatingMediaQueryExp.get() && values) {
        m_observer->startMediaQueryExp(m_mediaQueryValueStartOffset);
        m_observer->endMediaQueryExp(m_mediaQueryValueEndOffset);
    }
    return m_floatingMediaQueryExp.get();
}

PassOwnPtrWillBeRawPtr<MediaQueryExp> BisonCSSParser::sinkFloatingMediaQueryExp(MediaQueryExp* expression)
{
    ASSERT_UNUSED(expression, expression == m_floatingMediaQueryExp);
    return m_floatingMediaQueryExp.release();
}

WillBeHeapVector<OwnPtrWillBeMember<MediaQueryExp> >* BisonCSSParser::createFloatingMediaQueryExpList()
{
    m_floatingMediaQueryExpList = adoptPtrWillBeNoop(new WillBeHeapVector<OwnPtrWillBeMember<MediaQueryExp> >);
    return m_floatingMediaQueryExpList.get();
}

PassOwnPtrWillBeRawPtr<WillBeHeapVector<OwnPtrWillBeMember<MediaQueryExp> > > BisonCSSParser::sinkFloatingMediaQueryExpList(WillBeHeapVector<OwnPtrWillBeMember<MediaQueryExp> >* list)
{
    ASSERT_UNUSED(list, list == m_floatingMediaQueryExpList);
    return m_floatingMediaQueryExpList.release();
}

MediaQuery* BisonCSSParser::createFloatingMediaQuery(MediaQuery::Restrictor restrictor, const AtomicString& mediaType, PassOwnPtrWillBeRawPtr<WillBeHeapVector<OwnPtrWillBeMember<MediaQueryExp> > > expressions)
{
    m_floatingMediaQuery = adoptPtrWillBeNoop(new MediaQuery(restrictor, mediaType, expressions));
    if (m_observer)
        m_observer->endMediaQuery();
    return m_floatingMediaQuery.get();
}

MediaQuery* BisonCSSParser::createFloatingMediaQuery(PassOwnPtrWillBeRawPtr<WillBeHeapVector<OwnPtrWillBeMember<MediaQueryExp> > > expressions)
{
    return createFloatingMediaQuery(MediaQuery::None, MediaTypeNames::all, expressions);
}

MediaQuery* BisonCSSParser::createFloatingNotAllQuery()
{
    return createFloatingMediaQuery(MediaQuery::Not, MediaTypeNames::all, sinkFloatingMediaQueryExpList(createFloatingMediaQueryExpList()));
}

PassOwnPtrWillBeRawPtr<MediaQuery> BisonCSSParser::sinkFloatingMediaQuery(MediaQuery* query)
{
    ASSERT_UNUSED(query, query == m_floatingMediaQuery);
    return m_floatingMediaQuery.release();
}

WillBeHeapVector<RefPtrWillBeMember<StyleKeyframe> >* BisonCSSParser::createFloatingKeyframeVector()
{
    m_floatingKeyframeVector = adoptPtrWillBeNoop(new WillBeHeapVector<RefPtrWillBeMember<StyleKeyframe> >());
    return m_floatingKeyframeVector.get();
}

PassOwnPtrWillBeRawPtr<WillBeHeapVector<RefPtrWillBeMember<StyleKeyframe> > > BisonCSSParser::sinkFloatingKeyframeVector(WillBeHeapVector<RefPtrWillBeMember<StyleKeyframe> >* keyframeVector)
{
    ASSERT_UNUSED(keyframeVector, m_floatingKeyframeVector == keyframeVector);
    return m_floatingKeyframeVector.release();
}

MediaQuerySet* BisonCSSParser::createMediaQuerySet()
{
    RefPtrWillBeRawPtr<MediaQuerySet> queries = MediaQuerySet::create();
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
    RefPtrWillBeRawPtr<StyleRuleMedia> rule = nullptr;
    if (rules) {
        rule = StyleRuleMedia::create(media ? media : MediaQuerySet::create().get(), *rules);
    } else {
        RuleList emptyRules;
        rule = StyleRuleMedia::create(media ? media : MediaQuerySet::create().get(), emptyRules);
    }
    StyleRuleMedia* result = rule.get();
    m_parsedRules.append(rule.release());
    return result;
}

StyleRuleBase* BisonCSSParser::createSupportsRule(bool conditionIsSupported, RuleList* rules)
{
    m_allowImportRules = m_allowNamespaceDeclarations = false;

    RefPtrWillBeRawPtr<CSSRuleSourceData> data = popSupportsRuleData();
    RefPtrWillBeRawPtr<StyleRuleSupports> rule = nullptr;
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
        m_supportsRuleDataStack = adoptPtrWillBeNoop(new RuleSourceDataList());

    RefPtrWillBeRawPtr<CSSRuleSourceData> data = CSSRuleSourceData::create(CSSRuleSourceData::SUPPORTS_RULE);
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

PassRefPtrWillBeRawPtr<CSSRuleSourceData> BisonCSSParser::popSupportsRuleData()
{
    ASSERT(m_supportsRuleDataStack && !m_supportsRuleDataStack->isEmpty());
    RefPtrWillBeRawPtr<CSSRuleSourceData> data = m_supportsRuleDataStack->last();
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

void BisonCSSParser::tokenToLowerCase(CSSParserString& token)
{
    // Since it's our internal token, we know that we created it out
    // of our writable work buffers. Therefore the const_cast is just
    // ugly and not a potential crash.
    size_t length = token.length();
    if (token.is8Bit()) {
        makeLower(token.characters8(), const_cast<LChar*>(token.characters8()), length);
    } else {
        makeLower(token.characters16(), const_cast<UChar*>(token.characters16()), length);
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
    FrameConsole& console = m_styleSheet->singleOwnerDocument()->frame()->console();
    console.addMessage(ConsoleMessage::create(CSSMessageSource, WarningMessageLevel, message, m_styleSheet->baseURL().string(), lineNumberInStyleSheet + m_startPosition.m_line.zeroBasedInt() + 1, columnNumber + 1));
}

StyleRuleKeyframes* BisonCSSParser::createKeyframesRule(const String& name, PassOwnPtrWillBeRawPtr<WillBeHeapVector<RefPtrWillBeMember<StyleKeyframe> > > popKeyframes, bool isPrefixed)
{
    OwnPtrWillBeRawPtr<WillBeHeapVector<RefPtrWillBeMember<StyleKeyframe> > > keyframes = popKeyframes;
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

static void recordSelectorStats(const CSSParserContext& context, const CSSSelectorList& selectorList)
{
    if (!context.useCounter())
        return;

    for (const CSSSelector* selector = selectorList.first(); selector; selector = CSSSelectorList::next(*selector)) {
        for (const CSSSelector* current = selector; current ; current = current->tagHistory()) {
            UseCounter::Feature feature = UseCounter::NumberOfFeatures;
            switch (current->pseudoType()) {
            case CSSSelector::PseudoUnresolved:
                feature = UseCounter::CSSSelectorPseudoUnresolved;
                break;
            case CSSSelector::PseudoShadow:
                feature = UseCounter::CSSSelectorPseudoShadow;
                break;
            case CSSSelector::PseudoContent:
                feature = UseCounter::CSSSelectorPseudoContent;
                break;
            case CSSSelector::PseudoHost:
                feature = UseCounter::CSSSelectorPseudoHost;
                break;
            case CSSSelector::PseudoHostContext:
                feature = UseCounter::CSSSelectorPseudoHostContext;
                break;
            default:
                break;
            }
            if (feature != UseCounter::NumberOfFeatures)
                context.useCounter()->count(feature);
            if (current->relation() == CSSSelector::ShadowDeep)
                context.useCounter()->count(UseCounter::CSSDeepCombinator);
            if (current->selectorList())
                recordSelectorStats(context, *current->selectorList());
        }
    }
}

StyleRuleBase* BisonCSSParser::createStyleRule(Vector<OwnPtr<CSSParserSelector> >* selectors)
{
    StyleRule* result = 0;
    if (selectors) {
        m_allowImportRules = m_allowNamespaceDeclarations = false;
        RefPtrWillBeRawPtr<StyleRule> rule = StyleRule::create();
        rule->parserAdoptSelectorVector(*selectors);
        rule->setProperties(createStylePropertySet());
        result = rule.get();
        m_parsedRules.append(rule.release());
        recordSelectorStats(m_context, result->selectorList());
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
    if (m_defaultNamespace != starAtom || specifiers->crossesTreeScopes())
        return rewriteSpecifiersWithElementName(nullAtom, starAtom, specifiers, /*tagIsForNamespaceRule*/true);
    return specifiers;
}

CSSParserSelector* BisonCSSParser::rewriteSpecifiersWithElementName(const AtomicString& namespacePrefix, const AtomicString& elementName, CSSParserSelector* specifiers, bool tagIsForNamespaceRule)
{
    AtomicString determinedNamespace = namespacePrefix != nullAtom && m_styleSheet ? m_styleSheet->determineNamespace(namespacePrefix) : m_defaultNamespace;
    QualifiedName tag(namespacePrefix, elementName, determinedNamespace);

    if (specifiers->crossesTreeScopes())
        return rewriteSpecifiersWithElementNameForCustomPseudoElement(tag, elementName, specifiers, tagIsForNamespaceRule);

    if (specifiers->isContentPseudoElement())
        return rewriteSpecifiersWithElementNameForContentPseudoElement(tag, elementName, specifiers, tagIsForNamespaceRule);

    // *:host never matches, so we can't discard the * otherwise we can't tell the
    // difference between *:host and just :host.
    if (tag == anyQName() && !specifiers->hasHostPseudoSelector())
        return specifiers;
    if (specifiers->pseudoType() != CSSSelector::PseudoCue)
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
        if (history->crossesTreeScopes() || history->hasShadowPseudo())
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

CSSParserSelector* BisonCSSParser::rewriteSpecifiers(CSSParserSelector* specifiers, CSSParserSelector* newSpecifier)
{
    if (newSpecifier->crossesTreeScopes()) {
        // Unknown pseudo element always goes at the top of selector chain.
        newSpecifier->appendTagHistory(CSSSelector::ShadowPseudo, sinkFloatingSelector(specifiers));
        return newSpecifier;
    }
    if (newSpecifier->isContentPseudoElement()) {
        newSpecifier->appendTagHistory(CSSSelector::SubSelector, sinkFloatingSelector(specifiers));
        return newSpecifier;
    }
    if (specifiers->crossesTreeScopes()) {
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

StyleKeyframe* BisonCSSParser::createKeyframe(CSSParserValueList* keys)
{
    OwnPtr<Vector<double> > keyVector = StyleKeyframe::createKeyList(keys);
    if (keyVector->isEmpty())
        return 0;

    RefPtrWillBeRawPtr<StyleKeyframe> keyframe = StyleKeyframe::create();
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

}
