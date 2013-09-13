/*
 * Copyright (C) 2003 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2009 - 2010  Torch Mobile (Beijing) Co. Ltd. All rights reserved.
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

#ifndef CSSParser_h
#define CSSParser_h

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "core/css/CSSCalculationValue.h"
#include "core/css/CSSFilterValue.h"
#include "core/css/CSSGradientValue.h"
#include "core/css/CSSParserMode.h"
#include "core/css/CSSParserValues.h"
#include "core/css/CSSProperty.h"
#include "core/css/CSSPropertySourceData.h"
#include "core/css/CSSSelector.h"
#include "core/css/MediaQuery.h"
#include "core/page/UseCounter.h"
#include "core/platform/graphics/Color.h"
#include "wtf/HashSet.h"
#include "wtf/OwnArrayPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/TextPosition.h"

namespace WebCore {

class AnimationParseContext;
class CSSArrayFunctionValue;
class CSSBorderImageSliceValue;
class CSSMixFunctionValue;
class CSSPrimitiveValue;
class CSSSelectorList;
class CSSShaderValue;
class CSSValue;
class CSSValueList;
class CSSBasicShape;
class Document;
class Element;
class ImmutableStylePropertySet;
class MediaQueryExp;
class MediaQuerySet;
class MutableStylePropertySet;
class StyleKeyframe;
class StylePropertyShorthand;
class StyleRuleBase;
class StyleRuleKeyframes;
class StyleKeyframe;
class StyleSheetContents;

struct CSSParserLocation {
    unsigned offset;
    unsigned lineNumber;
    CSSParserString token;
};

class CSSParser {
    friend inline int cssyylex(void*, CSSParser*);

public:
    class SourceDataHandler;
    enum ErrorType {
        NoError,
        PropertyDeclarationError,
        InvalidPropertyValueError,
        InvalidPropertyError,
        InvalidSelectorError,
        InvalidSupportsConditionError,
        InvalidRuleError,
        InvalidMediaQueryError,
        InvalidKeyframeSelectorError,
        InvalidSelectorPseudoError,
        UnterminatedCommentError,
        GeneralError
    };

    CSSParser(const CSSParserContext&, UseCounter* = 0);

    ~CSSParser();

    void parseSheet(StyleSheetContents*, const String&, const TextPosition& startPosition = TextPosition::minimumPosition(), SourceDataHandler* = 0, bool = false);
    PassRefPtr<StyleRuleBase> parseRule(StyleSheetContents*, const String&);
    PassRefPtr<StyleKeyframe> parseKeyframeRule(StyleSheetContents*, const String&);
    bool parseSupportsCondition(const String&);
    static bool parseValue(MutableStylePropertySet*, CSSPropertyID, const String&, bool important, CSSParserMode, StyleSheetContents*);
    static bool parseColor(RGBA32& color, const String&, bool strict = false);
    static bool parseSystemColor(RGBA32& color, const String&, Document*);
    static PassRefPtr<CSSValueList> parseFontFaceValue(const AtomicString&);
    PassRefPtr<CSSPrimitiveValue> parseValidPrimitive(CSSValueID ident, CSSParserValue*);
    bool parseDeclaration(MutableStylePropertySet*, const String&, SourceDataHandler*, StyleSheetContents* contextStyleSheet);
    static PassRefPtr<ImmutableStylePropertySet> parseInlineStyleDeclaration(const String&, Element*);
    PassRefPtr<MediaQuerySet> parseMediaQueryList(const String&);
    PassOwnPtr<Vector<double> > parseKeyframeKeyList(const String&);

    void addPropertyWithPrefixingVariant(CSSPropertyID, PassRefPtr<CSSValue>, bool important, bool implicit = false);
    void addProperty(CSSPropertyID, PassRefPtr<CSSValue>, bool important, bool implicit = false);
    void rollbackLastProperties(int num);
    bool hasProperties() const { return !m_parsedProperties.isEmpty(); }
    void addExpandedPropertyForValue(CSSPropertyID propId, PassRefPtr<CSSValue>, bool);
    void setCurrentProperty(CSSPropertyID);

    bool parseValue(CSSPropertyID, bool important);
    bool parseShorthand(CSSPropertyID, const StylePropertyShorthand&, bool important);
    bool parse4Values(CSSPropertyID, const CSSPropertyID* properties, bool important);
    bool parseContent(CSSPropertyID, bool important);
    bool parseQuotes(CSSPropertyID, bool important);

    static bool parseValue(MutableStylePropertySet*, CSSPropertyID, const String&, bool important, const Document&);
    void storeVariableDeclaration(const CSSParserString&, PassOwnPtr<CSSParserValueList>, bool important);

    PassRefPtr<CSSValue> parseAttr(CSSParserValueList* args);

    PassRefPtr<CSSValue> parseBackgroundColor();

    bool parseFillImage(CSSParserValueList*, RefPtr<CSSValue>&);

    enum FillPositionFlag { InvalidFillPosition = 0, AmbiguousFillPosition = 1, XFillPosition = 2, YFillPosition = 4 };
    enum FillPositionParsingMode { ResolveValuesAsPercent = 0, ResolveValuesAsKeyword = 1 };
    PassRefPtr<CSSPrimitiveValue> parseFillPositionComponent(CSSParserValueList*, unsigned& cumulativeFlags, FillPositionFlag& individualFlag, FillPositionParsingMode = ResolveValuesAsPercent);
    PassRefPtr<CSSValue> parseFillPositionX(CSSParserValueList*);
    PassRefPtr<CSSValue> parseFillPositionY(CSSParserValueList*);
    void parse2ValuesFillPosition(CSSParserValueList*, RefPtr<CSSValue>&, RefPtr<CSSValue>&);
    bool isPotentialPositionValue(CSSParserValue*);
    void parseFillPosition(CSSParserValueList*, RefPtr<CSSValue>&, RefPtr<CSSValue>&);
    void parse3ValuesFillPosition(CSSParserValueList*, RefPtr<CSSValue>&, RefPtr<CSSValue>&, PassRefPtr<CSSPrimitiveValue>, PassRefPtr<CSSPrimitiveValue>);
    void parse4ValuesFillPosition(CSSParserValueList*, RefPtr<CSSValue>&, RefPtr<CSSValue>&, PassRefPtr<CSSPrimitiveValue>, PassRefPtr<CSSPrimitiveValue>);

    void parseFillRepeat(RefPtr<CSSValue>&, RefPtr<CSSValue>&);
    PassRefPtr<CSSValue> parseFillSize(CSSPropertyID, bool &allowComma);

    bool parseFillProperty(CSSPropertyID propId, CSSPropertyID& propId1, CSSPropertyID& propId2, RefPtr<CSSValue>&, RefPtr<CSSValue>&);
    bool parseFillShorthand(CSSPropertyID, const CSSPropertyID* properties, int numProperties, bool important);

    void addFillValue(RefPtr<CSSValue>& lval, PassRefPtr<CSSValue> rval);

    void addAnimationValue(RefPtr<CSSValue>& lval, PassRefPtr<CSSValue> rval);

    PassRefPtr<CSSValue> parseAnimationDelay();
    PassRefPtr<CSSValue> parseAnimationDirection();
    PassRefPtr<CSSValue> parseAnimationDuration();
    PassRefPtr<CSSValue> parseAnimationFillMode();
    PassRefPtr<CSSValue> parseAnimationIterationCount();
    PassRefPtr<CSSValue> parseAnimationName();
    PassRefPtr<CSSValue> parseAnimationPlayState();
    PassRefPtr<CSSValue> parseAnimationProperty(AnimationParseContext&);
    PassRefPtr<CSSValue> parseAnimationTimingFunction();

    bool parseTransformOriginShorthand(RefPtr<CSSValue>&, RefPtr<CSSValue>&, RefPtr<CSSValue>&);
    bool parseCubicBezierTimingFunctionValue(CSSParserValueList*& args, double& result);
    bool parseAnimationProperty(CSSPropertyID, RefPtr<CSSValue>&, AnimationParseContext&);
    bool parseTransitionShorthand(CSSPropertyID, bool important);
    bool parseAnimationShorthand(CSSPropertyID, bool important);

    PassRefPtr<CSSValue> parseColumnWidth();
    PassRefPtr<CSSValue> parseColumnCount();
    bool parseColumnsShorthand(bool important);

    PassRefPtr<CSSValue> parseGridPosition();
    bool parseIntegerOrStringFromGridPosition(RefPtr<CSSPrimitiveValue>& numericValue, RefPtr<CSSPrimitiveValue>& gridLineName);
    bool parseGridItemPositionShorthand(CSSPropertyID, bool important);
    bool parseGridAreaShorthand(bool important);
    bool parseSingleGridAreaLonghand(RefPtr<CSSValue>&);
    bool parseGridTrackList(CSSPropertyID, bool important);
    bool parseGridTrackRepeatFunction(CSSValueList&);
    PassRefPtr<CSSValue> parseGridTrackSize(CSSParserValueList& inputList);
    PassRefPtr<CSSPrimitiveValue> parseGridBreadth(CSSParserValue*);
    PassRefPtr<CSSValue> parseGridTemplate();

    bool parseClipShape(CSSPropertyID, bool important);

    bool parseBasicShape(CSSPropertyID, bool important);
    PassRefPtr<CSSBasicShape> parseBasicShapeRectangle(CSSParserValueList* args);
    PassRefPtr<CSSBasicShape> parseBasicShapeCircle(CSSParserValueList* args);
    PassRefPtr<CSSBasicShape> parseBasicShapeEllipse(CSSParserValueList* args);
    PassRefPtr<CSSBasicShape> parseBasicShapePolygon(CSSParserValueList* args);
    PassRefPtr<CSSBasicShape> parseBasicShapeInsetRectangle(CSSParserValueList* args);

    bool parseFont(bool important);
    PassRefPtr<CSSValueList> parseFontFamily();

    bool parseCounter(CSSPropertyID, int defaultValue, bool important);
    PassRefPtr<CSSValue> parseCounterContent(CSSParserValueList* args, bool counters);

    bool parseColorParameters(CSSParserValue*, int* colorValues, bool parseAlpha);
    bool parseHSLParameters(CSSParserValue*, double* colorValues, bool parseAlpha);
    PassRefPtr<CSSPrimitiveValue> parseColor(CSSParserValue* = 0);
    bool parseColorFromValue(CSSParserValue*, RGBA32&);
    void parseSelector(const String&, CSSSelectorList&);

    template<typename StringType>
    static bool fastParseColor(RGBA32&, const StringType&, bool strict);

    bool parseLineHeight(bool important);
    bool parseFontSize(bool important);
    bool parseFontVariant(bool important);
    bool parseFontWeight(bool important);
    bool parseFontFaceSrc();
    bool parseFontFaceUnicodeRange();

    bool parseSVGValue(CSSPropertyID propId, bool important);
    PassRefPtr<CSSValue> parseSVGPaint();
    PassRefPtr<CSSValue> parseSVGColor();
    PassRefPtr<CSSValue> parseSVGStrokeDasharray();

    PassRefPtr<CSSValue> parsePaintOrder() const;

    // CSS3 Parsing Routines (for properties specific to CSS3)
    PassRefPtr<CSSValueList> parseShadow(CSSParserValueList*, CSSPropertyID);
    bool parseBorderImage(CSSPropertyID, RefPtr<CSSValue>&, bool important = false);
    bool parseBorderImageRepeat(RefPtr<CSSValue>&);
    bool parseBorderImageSlice(CSSPropertyID, RefPtr<CSSBorderImageSliceValue>&);
    bool parseBorderImageWidth(RefPtr<CSSPrimitiveValue>&);
    bool parseBorderImageOutset(RefPtr<CSSPrimitiveValue>&);
    bool parseBorderRadius(CSSPropertyID, bool important);

    bool parseAspectRatio(bool important);

    bool parseReflect(CSSPropertyID, bool important);

    bool parseFlex(CSSParserValueList* args, bool important);

    bool parseObjectPosition(bool important);

    // Image generators
    bool parseCanvas(CSSParserValueList*, RefPtr<CSSValue>&);

    bool parseDeprecatedGradient(CSSParserValueList*, RefPtr<CSSValue>&);
    bool parseDeprecatedLinearGradient(CSSParserValueList*, RefPtr<CSSValue>&, CSSGradientRepeat repeating);
    bool parseDeprecatedRadialGradient(CSSParserValueList*, RefPtr<CSSValue>&, CSSGradientRepeat repeating);
    bool parseLinearGradient(CSSParserValueList*, RefPtr<CSSValue>&, CSSGradientRepeat repeating);
    bool parseRadialGradient(CSSParserValueList*, RefPtr<CSSValue>&, CSSGradientRepeat repeating);
    bool parseGradientColorStops(CSSParserValueList*, CSSGradientValue*, bool expectComma);

    bool parseCrossfade(CSSParserValueList*, RefPtr<CSSValue>&);

    PassRefPtr<CSSValue> parseImageSet(CSSParserValueList*);

    PassRefPtr<CSSValueList> parseFilter();
    PassRefPtr<CSSFilterValue> parseBuiltinFilterArguments(CSSParserValueList*, CSSFilterValue::FilterOperationType);
    PassRefPtr<CSSMixFunctionValue> parseMixFunction(CSSParserValue*);
    PassRefPtr<CSSArrayFunctionValue> parseCustomFilterArrayFunction(CSSParserValue*);
    PassRefPtr<CSSValueList> parseCustomFilterTransform(CSSParserValueList*);
    PassRefPtr<CSSValueList> parseCustomFilterParameters(CSSParserValueList*);
    PassRefPtr<CSSFilterValue> parseCustomFilterFunctionWithAtRuleReferenceSyntax(CSSParserValue*);
    PassRefPtr<CSSFilterValue> parseCustomFilterFunctionWithInlineSyntax(CSSParserValue*);
    PassRefPtr<CSSFilterValue> parseCustomFilterFunction(CSSParserValue*);
    bool parseFilterRuleSrc();
    PassRefPtr<CSSShaderValue> parseFilterRuleSrcUriAndFormat(CSSParserValueList*);

    static bool isBlendMode(CSSValueID);
    static bool isCompositeOperator(CSSValueID);

    PassRefPtr<CSSValueList> parseTransform();
    PassRefPtr<CSSValue> parseTransformValue(CSSParserValue*);
    bool parseTransformOrigin(CSSPropertyID propId, CSSPropertyID& propId1, CSSPropertyID& propId2, CSSPropertyID& propId3, RefPtr<CSSValue>&, RefPtr<CSSValue>&, RefPtr<CSSValue>&);
    bool parsePerspectiveOrigin(CSSPropertyID propId, CSSPropertyID& propId1, CSSPropertyID& propId2,  RefPtr<CSSValue>&, RefPtr<CSSValue>&);

    bool parseTextEmphasisStyle(bool important);

    void addTextDecorationProperty(CSSPropertyID, PassRefPtr<CSSValue>, bool important);
    bool parseTextDecoration(CSSPropertyID propId, bool important);
#if ENABLE(CSS3_TEXT)
    bool parseTextUnderlinePosition(bool important);
#endif // CSS3_TEXT

    PassRefPtr<CSSValue> parseTextIndent();

    bool parseLineBoxContain(bool important);
    bool parseCalculation(CSSParserValue*, CalculationPermittedValueRange);

    bool parseFontFeatureTag(CSSValueList*);
    bool parseFontFeatureSettings(bool important);

    bool parseFlowThread(const String& flowName);
    bool parseFlowThread(CSSPropertyID, bool important);
    bool parseRegionThread(CSSPropertyID, bool important);

    bool parseFontVariantLigatures(bool important);

    CSSParserSelector* createFloatingSelector();
    CSSParserSelector* createFloatingSelectorWithTagName(const QualifiedName&);
    PassOwnPtr<CSSParserSelector> sinkFloatingSelector(CSSParserSelector*);

    Vector<OwnPtr<CSSParserSelector> >* createFloatingSelectorVector();
    PassOwnPtr<Vector<OwnPtr<CSSParserSelector> > > sinkFloatingSelectorVector(Vector<OwnPtr<CSSParserSelector> >*);

    CSSParserValueList* createFloatingValueList();
    PassOwnPtr<CSSParserValueList> sinkFloatingValueList(CSSParserValueList*);

    CSSParserFunction* createFloatingFunction();
    CSSParserFunction* createFloatingFunction(const CSSParserString& name, PassOwnPtr<CSSParserValueList> args);
    PassOwnPtr<CSSParserFunction> sinkFloatingFunction(CSSParserFunction*);

    CSSParserValue& sinkFloatingValue(CSSParserValue&);

    MediaQuerySet* createMediaQuerySet();
    StyleRuleBase* createImportRule(const CSSParserString&, MediaQuerySet*);
    StyleKeyframe* createKeyframe(CSSParserValueList*);
    StyleRuleKeyframes* createKeyframesRule(const String&, PassOwnPtr<Vector<RefPtr<StyleKeyframe> > >, bool isPrefixed);

    typedef Vector<RefPtr<StyleRuleBase> > RuleList;
    StyleRuleBase* createMediaRule(MediaQuerySet*, RuleList*);
    RuleList* createRuleList();
    StyleRuleBase* createStyleRule(Vector<OwnPtr<CSSParserSelector> >* selectors);
    StyleRuleBase* createFontFaceRule();
    StyleRuleBase* createPageRule(PassOwnPtr<CSSParserSelector> pageSelector);
    StyleRuleBase* createRegionRule(Vector<OwnPtr<CSSParserSelector> >* regionSelector, RuleList* rules);
    StyleRuleBase* createMarginAtRule(CSSSelector::MarginBoxType);
    StyleRuleBase* createSupportsRule(bool conditionIsSupported, RuleList*);
    void markSupportsRuleHeaderStart();
    void markSupportsRuleHeaderEnd();
    PassRefPtr<CSSRuleSourceData> popSupportsRuleData();
    StyleRuleBase* createHostRule(RuleList* rules);
    StyleRuleBase* createFilterRule(const CSSParserString&);

    void startDeclarationsForMarginBox();
    void endDeclarationsForMarginBox();

    MediaQueryExp* createFloatingMediaQueryExp(const AtomicString&, CSSParserValueList*);
    PassOwnPtr<MediaQueryExp> sinkFloatingMediaQueryExp(MediaQueryExp*);
    Vector<OwnPtr<MediaQueryExp> >* createFloatingMediaQueryExpList();
    PassOwnPtr<Vector<OwnPtr<MediaQueryExp> > > sinkFloatingMediaQueryExpList(Vector<OwnPtr<MediaQueryExp> >*);
    MediaQuery* createFloatingMediaQuery(MediaQuery::Restrictor, const String&, PassOwnPtr<Vector<OwnPtr<MediaQueryExp> > >);
    MediaQuery* createFloatingMediaQuery(PassOwnPtr<Vector<OwnPtr<MediaQueryExp> > >);
    MediaQuery* createFloatingNotAllQuery();
    PassOwnPtr<MediaQuery> sinkFloatingMediaQuery(MediaQuery*);

    Vector<RefPtr<StyleKeyframe> >* createFloatingKeyframeVector();
    PassOwnPtr<Vector<RefPtr<StyleKeyframe> > > sinkFloatingKeyframeVector(Vector<RefPtr<StyleKeyframe> >*);

    void addNamespace(const AtomicString& prefix, const AtomicString& uri);
    QualifiedName determineNameInNamespace(const AtomicString& prefix, const AtomicString& localName);

    CSSParserSelector* rewriteSpecifiersWithElementName(const AtomicString& namespacePrefix, const AtomicString& elementName, CSSParserSelector*, bool isNamespacePlaceholder = false);
    CSSParserSelector* rewriteSpecifiersWithElementNameForCustomPseudoElement(const QualifiedName& tag, const AtomicString& elementName, CSSParserSelector* specifiers, bool tagIsForNamespaceRule);
    CSSParserSelector* rewriteSpecifiersWithElementNameForContentPseudoElement(const QualifiedName& tag, const AtomicString& elementName, CSSParserSelector* specifiers, bool tagIsForNamespaceRule);
    CSSParserSelector* rewriteSpecifiersWithNamespaceIfNeeded(CSSParserSelector*);
    CSSParserSelector* rewriteSpecifiers(CSSParserSelector*, CSSParserSelector*);
    CSSParserSelector* rewriteSpecifiersForShadowDistributed(CSSParserSelector* specifiers, CSSParserSelector* distributedPseudoElementSelector);

    void invalidBlockHit();

    Vector<OwnPtr<CSSParserSelector> >* reusableSelectorVector() { return &m_reusableSelectorVector; }

    void setReusableRegionSelectorVector(Vector<OwnPtr<CSSParserSelector> >* selectors);
    Vector<OwnPtr<CSSParserSelector> >* reusableRegionSelectorVector() { return &m_reusableRegionSelectorVector; }

    void clearProperties();

    PassRefPtr<ImmutableStylePropertySet> createStylePropertySet();

    CSSParserContext m_context;

    bool m_important;
    CSSPropertyID m_id;
    StyleSheetContents* m_styleSheet;
    RefPtr<StyleRuleBase> m_rule;
    RefPtr<StyleKeyframe> m_keyframe;
    RefPtr<MediaQuerySet> m_mediaList;
    OwnPtr<CSSParserValueList> m_valueList;
    bool m_supportsCondition;

    typedef Vector<CSSProperty, 256> ParsedPropertyVector;
    ParsedPropertyVector m_parsedProperties;
    CSSSelectorList* m_selectorListForParseSelector;

    unsigned m_numParsedPropertiesBeforeMarginBox;

    int m_inParseShorthand;
    CSSPropertyID m_currentShorthand;
    bool m_implicitShorthand;

    bool m_hasFontFaceOnlyValues;
    bool m_hadSyntacticallyValidCSSRule;
    bool m_logErrors;
    bool m_ignoreErrors;

    bool m_inFilterRule;

    AtomicString m_defaultNamespace;

    // tokenizer methods and data
    size_t m_parsedTextPrefixLength;
    size_t m_parsedTextSuffixLength;
    SourceDataHandler* m_sourceDataHandler;

    void startRuleHeader(CSSRuleSourceData::Type);
    void endRuleHeader();
    void startSelector();
    void endSelector();
    void startRuleBody();
    void endRuleBody(bool discard = false);
    void startProperty();
    void endProperty(bool isImportantFound, bool isPropertyParsed, ErrorType = NoError);
    void startEndUnknownRule();

    void endInvalidRuleHeader();
    void reportError(const CSSParserLocation&, ErrorType = GeneralError);
    void resumeErrorLogging() { m_ignoreErrors = false; }
    void setLocationLabel(const CSSParserLocation& location) { m_locationLabel = location; }
    const CSSParserLocation& lastLocationLabel() const { return m_locationLabel; }

    inline int lex(void* yylval) { return (this->*m_lexFunc)(yylval); }

    int token() { return m_token; }

    void tokenToLowerCase(const CSSParserString& token);

    void markViewportRuleBodyStart() { m_inViewport = true; }
    void markViewportRuleBodyEnd() { m_inViewport = false; }
    StyleRuleBase* createViewportRule();

    PassRefPtr<CSSPrimitiveValue> createPrimitiveNumericValue(CSSParserValue*);
    PassRefPtr<CSSPrimitiveValue> createPrimitiveStringValue(CSSParserValue*);
    PassRefPtr<CSSPrimitiveValue> createPrimitiveVariableNameValue(CSSParserValue*);

    static KURL completeURL(const CSSParserContext&, const String& url);

    CSSParserLocation currentLocation();

private:
    enum PropertyType {
        PropertyExplicit,
        PropertyImplicit
    };

    class ImplicitScope {
        WTF_MAKE_NONCOPYABLE(ImplicitScope);
    public:
        ImplicitScope(WebCore::CSSParser* parser, PropertyType propertyType)
            : m_parser(parser)
        {
            m_parser->m_implicitShorthand = propertyType == CSSParser::PropertyImplicit;
        }

        ~ImplicitScope()
        {
            m_parser->m_implicitShorthand = false;
        }

    private:
        WebCore::CSSParser* m_parser;
    };

    bool is8BitSource() const { return m_is8BitSource; }

    template <typename SourceCharacterType>
    int realLex(void* yylval);

    UChar*& currentCharacter16();

    template <typename CharacterType>
    inline CharacterType*& currentCharacter();

    template <typename CharacterType>
    inline CharacterType* tokenStart();

    template <typename CharacterType>
    inline CharacterType* dataStart();

    template <typename CharacterType>
    inline void setTokenStart(CharacterType*);

    inline unsigned tokenStartOffset();
    inline UChar tokenStartChar();

    template <typename CharacterType>
    inline bool isIdentifierStart();

    inline void ensureLineEndings();

    template <typename CharacterType>
    inline CSSParserLocation tokenLocation();

    template <typename CharacterType>
    unsigned parseEscape(CharacterType*&);
    template <typename DestCharacterType>
    inline void UnicodeToChars(DestCharacterType*&, unsigned);
    template <typename SrcCharacterType, typename DestCharacterType>
    inline bool parseIdentifierInternal(SrcCharacterType*&, DestCharacterType*&, bool&);

    template <typename CharacterType>
    inline void parseIdentifier(CharacterType*&, CSSParserString&, bool&);

    template <typename SrcCharacterType, typename DestCharacterType>
    inline bool parseStringInternal(SrcCharacterType*&, DestCharacterType*&, UChar);

    template <typename CharacterType>
    inline void parseString(CharacterType*&, CSSParserString& resultString, UChar);

    template <typename CharacterType>
    inline bool findURI(CharacterType*& start, CharacterType*& end, UChar& quote);

    template <typename SrcCharacterType, typename DestCharacterType>
    inline bool parseURIInternal(SrcCharacterType*&, DestCharacterType*&, UChar quote);

    template <typename CharacterType>
    inline void parseURI(CSSParserString&);
    template <typename CharacterType>
    inline bool parseUnicodeRange();
    template <typename CharacterType>
    bool parseNthChild();
    template <typename CharacterType>
    bool parseNthChildExtra();
    template <typename CharacterType>
    inline bool detectFunctionTypeToken(int);
    template <typename CharacterType>
    inline void detectMediaQueryToken(int);
    template <typename CharacterType>
    inline void detectNumberToken(CharacterType*, int);
    template <typename CharacterType>
    inline void detectDashToken(int);
    template <typename CharacterType>
    inline void detectAtToken(int, bool);
    template <typename CharacterType>
    inline void detectSupportsToken(int);
    template <typename CharacterType>
    inline void detectCSSVariableDefinitionToken(int);

    void setStyleSheet(StyleSheetContents* styleSheet) { m_styleSheet = styleSheet; }

    inline bool inStrictMode() const { return m_context.mode == CSSStrictMode || m_context.mode == SVGAttributeMode; }
    inline bool inQuirksMode() const { return m_context.mode == CSSQuirksMode; }

    KURL completeURL(const String& url) const;

    void recheckAtKeyword(const UChar* str, int len);

    template<unsigned prefixLength, unsigned suffixLength>
    inline void setupParser(const char (&prefix)[prefixLength], const String& string, const char (&suffix)[suffixLength])
    {
        setupParser(prefix, prefixLength - 1, string, suffix, suffixLength - 1);
    }
    void setupParser(const char* prefix, unsigned prefixLength, const String&, const char* suffix, unsigned suffixLength);
    bool inShorthand() const { return m_inParseShorthand; }

    bool validWidthOrHeight(CSSParserValue*);

    void deleteFontFaceOnlyValues();

    bool isGeneratedImageValue(CSSParserValue*) const;
    bool parseGeneratedImage(CSSParserValueList*, RefPtr<CSSValue>&);

    bool parseValue(MutableStylePropertySet*, CSSPropertyID, const String&, bool important, StyleSheetContents* contextStyleSheet);
    PassRefPtr<ImmutableStylePropertySet> parseDeclaration(const String&, StyleSheetContents* contextStyleSheet);

    enum SizeParameterType {
        None,
        Auto,
        Length,
        PageSize,
        Orientation,
    };

    bool parsePage(CSSPropertyID propId, bool important);
    bool parseSize(CSSPropertyID propId, bool important);
    SizeParameterType parseSizeParameter(CSSValueList* parsedValues, CSSParserValue* value, SizeParameterType prevParamType);

    bool parseFontFaceSrcURI(CSSValueList*);
    bool parseFontFaceSrcLocal(CSSValueList*);

    bool parseColor(const String&);

    enum ParsingMode {
        NormalMode,
        MediaQueryMode,
        SupportsMode,
        NthChildMode
    };

    ParsingMode m_parsingMode;
    bool m_is8BitSource;
    OwnArrayPtr<LChar> m_dataStart8;
    OwnArrayPtr<UChar> m_dataStart16;
    LChar* m_currentCharacter8;
    UChar* m_currentCharacter16;
    const String* m_source;
    union {
        LChar* ptr8;
        UChar* ptr16;
    } m_tokenStart;
    unsigned m_length;
    int m_token;
    TextPosition m_startPosition;
    int m_lineNumber;
    int m_tokenStartLineNumber;
    CSSRuleSourceData::Type m_ruleHeaderType;
    unsigned m_ruleHeaderStartOffset;
    int m_ruleHeaderStartLineNumber;
    OwnPtr<Vector<unsigned> > m_lineEndings;

    bool m_allowImportRules;
    bool m_allowNamespaceDeclarations;

    bool parseViewportProperty(CSSPropertyID propId, bool important);
    bool parseViewportShorthand(CSSPropertyID propId, CSSPropertyID first, CSSPropertyID second, bool important);

    bool inViewport() const { return m_inViewport; }
    bool m_inViewport;

    CSSParserLocation m_locationLabel;

    bool useLegacyBackgroundSizeShorthandBehavior() const;

    int (CSSParser::*m_lexFunc)(void*);

    Vector<RefPtr<StyleRuleBase> > m_parsedRules;
    Vector<RefPtr<StyleKeyframe> > m_parsedKeyframes;
    Vector<RefPtr<MediaQuerySet> > m_parsedMediaQuerySets;
    Vector<OwnPtr<RuleList> > m_parsedRuleLists;
    HashSet<CSSParserSelector*> m_floatingSelectors;
    HashSet<Vector<OwnPtr<CSSParserSelector> >*> m_floatingSelectorVectors;
    HashSet<CSSParserValueList*> m_floatingValueLists;
    HashSet<CSSParserFunction*> m_floatingFunctions;

    OwnPtr<MediaQuery> m_floatingMediaQuery;
    OwnPtr<MediaQueryExp> m_floatingMediaQueryExp;
    OwnPtr<Vector<OwnPtr<MediaQueryExp> > > m_floatingMediaQueryExpList;

    OwnPtr<Vector<RefPtr<StyleKeyframe> > > m_floatingKeyframeVector;

    Vector<OwnPtr<CSSParserSelector> > m_reusableSelectorVector;
    Vector<OwnPtr<CSSParserSelector> > m_reusableRegionSelectorVector;

    RefPtr<CSSCalcValue> m_parsedCalculation;

    OwnPtr<RuleSourceDataList> m_supportsRuleDataStack;

    // defines units allowed for a certain property, used in parseUnit
    enum Units {
        FUnknown = 0x0000,
        FInteger = 0x0001,
        FNumber = 0x0002, // Real Numbers
        FPercent = 0x0004,
        FLength = 0x0008,
        FAngle = 0x0010,
        FTime = 0x0020,
        FFrequency = 0x0040,
        FPositiveInteger = 0x0080,
        FRelative = 0x0100,
        FResolution = 0x0200,
        FNonNeg = 0x0400
    };

    friend inline Units operator|(Units a, Units b)
    {
        return static_cast<Units>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
    }

    enum ReleaseParsedCalcValueCondition {
        ReleaseParsedCalcValue,
        DoNotReleaseParsedCalcValue
    };

    bool isLoggingErrors();
    void logError(const String& message, const CSSParserLocation&);

    bool validCalculationUnit(CSSParserValue*, Units, ReleaseParsedCalcValueCondition releaseCalc = DoNotReleaseParsedCalcValue);

    bool shouldAcceptUnitLessValues(CSSParserValue*, Units, CSSParserMode);

    inline bool validUnit(CSSParserValue* value, Units unitflags, ReleaseParsedCalcValueCondition releaseCalc = DoNotReleaseParsedCalcValue) { return validUnit(value, unitflags, m_context.mode, releaseCalc); }
    bool validUnit(CSSParserValue*, Units, CSSParserMode, ReleaseParsedCalcValueCondition releaseCalc = DoNotReleaseParsedCalcValue);

    bool parseBorderImageQuad(Units, RefPtr<CSSPrimitiveValue>&);
    int colorIntFromValue(CSSParserValue*);
    double parsedDouble(CSSParserValue*, ReleaseParsedCalcValueCondition releaseCalc = DoNotReleaseParsedCalcValue);
    bool isCalculation(CSSParserValue*);

    inline unsigned safeUserStringTokenOffset();

    UseCounter* m_useCounter;

    friend class TransformOperationInfo;
    friend class FilterOperationInfo;
};

CSSPropertyID cssPropertyID(const CSSParserString&);
CSSPropertyID cssPropertyID(const String&);
CSSValueID cssValueKeywordID(const CSSParserString&);

class ShorthandScope {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ShorthandScope(CSSParser* parser, CSSPropertyID propId) : m_parser(parser)
    {
        if (!(m_parser->m_inParseShorthand++))
            m_parser->m_currentShorthand = propId;
    }
    ~ShorthandScope()
    {
        if (!(--m_parser->m_inParseShorthand))
            m_parser->m_currentShorthand = CSSPropertyInvalid;
    }

private:
    CSSParser* m_parser;
};

class CSSParser::SourceDataHandler {
public:
    virtual void startRuleHeader(CSSRuleSourceData::Type, unsigned offset) = 0;
    virtual void endRuleHeader(unsigned offset) = 0;
    virtual void startSelector(unsigned offset) = 0;
    virtual void endSelector(unsigned offset) = 0;
    virtual void startRuleBody(unsigned offset) = 0;
    virtual void endRuleBody(unsigned offset, bool error) = 0;
    virtual void startEndUnknownRule() = 0;
    virtual void startProperty(unsigned offset) = 0;
    virtual void endProperty(bool isImportant, bool isParsed, unsigned offset, CSSParser::ErrorType) = 0;
    virtual void startComment(unsigned offset) = 0;
    virtual void endComment(unsigned offset) = 0;
};

String quoteCSSString(const String&);
String quoteCSSStringIfNeeded(const String&);
String quoteCSSURLIfNeeded(const String&);

bool isValidNthToken(const CSSParserString&);

template <>
inline void CSSParser::setTokenStart<LChar>(LChar* tokenStart)
{
    m_tokenStart.ptr8 = tokenStart;
}

template <>
inline void CSSParser::setTokenStart<UChar>(UChar* tokenStart)
{
    m_tokenStart.ptr16 = tokenStart;
}

inline unsigned CSSParser::tokenStartOffset()
{
    if (is8BitSource())
        return m_tokenStart.ptr8 - m_dataStart8.get();
    return m_tokenStart.ptr16 - m_dataStart16.get();
}

inline UChar CSSParser::tokenStartChar()
{
    if (is8BitSource())
        return *m_tokenStart.ptr8;
    return *m_tokenStart.ptr16;
}

inline int cssyylex(void* yylval, CSSParser* parser)
{
    return parser->lex(yylval);
}

} // namespace WebCore

#endif // CSSParser_h
