/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/inspector/InspectorStyleSheet.h"

#include "CSSPropertyNames.h"
#include "HTMLNames.h"
#include "SVGNames.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/css/CSSHostRule.h"
#include "core/css/CSSKeyframesRule.h"
#include "core/css/CSSMediaRule.h"
#include "core/css/CSSParser.h"
#include "core/css/CSSPropertySourceData.h"
#include "core/css/CSSRule.h"
#include "core/css/CSSRuleList.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/CSSSupportsRule.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleSheetContents.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/Node.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/inspector/ContentSearchUtils.h"
#include "core/inspector/InspectorCSSAgent.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/page/Page.h"
#include "core/page/PageConsole.h"
#include "core/platform/JSONValues.h"
#include "core/platform/text/RegularExpression.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/StringBuilder.h"
#include "wtf/text/TextPosition.h"

using WebCore::TypeBuilder::Array;
using WebCore::RuleSourceDataList;
using WebCore::CSSRuleSourceData;

class ParsedStyleSheet {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ParsedStyleSheet();

    const String& text() const { ASSERT(m_hasText); return m_text; }
    void setText(const String& text);
    bool hasText() const { return m_hasText; }
    void setSourceData(PassOwnPtr<RuleSourceDataList>);
    bool hasSourceData() const { return m_sourceData; }
    PassRefPtr<WebCore::CSSRuleSourceData> ruleSourceDataAt(unsigned) const;

private:
    void flattenSourceData(RuleSourceDataList*);

    String m_text;
    bool m_hasText;
    OwnPtr<RuleSourceDataList> m_sourceData;
};

ParsedStyleSheet::ParsedStyleSheet()
    : m_hasText(false)
{
}

void ParsedStyleSheet::setText(const String& text)
{
    m_hasText = true;
    m_text = text;
    setSourceData(nullptr);
}

void ParsedStyleSheet::flattenSourceData(RuleSourceDataList* dataList)
{
    for (size_t i = 0; i < dataList->size(); ++i) {
        RefPtr<CSSRuleSourceData>& data = dataList->at(i);
        if (data->type == CSSRuleSourceData::STYLE_RULE) {
            m_sourceData->append(data);
        } else if (data->type == CSSRuleSourceData::IMPORT_RULE) {
            m_sourceData->append(data);
        } else if (data->type == CSSRuleSourceData::MEDIA_RULE) {
            m_sourceData->append(data);
            flattenSourceData(&data->childRules);
        } else if (data->type == CSSRuleSourceData::HOST_RULE) {
            flattenSourceData(&data->childRules);
        } else if (data->type == CSSRuleSourceData::SUPPORTS_RULE) {
            flattenSourceData(&data->childRules);
        }
    }
}

void ParsedStyleSheet::setSourceData(PassOwnPtr<RuleSourceDataList> sourceData)
{
    if (!sourceData) {
        m_sourceData.clear();
        return;
    }

    m_sourceData = adoptPtr(new RuleSourceDataList());

    // FIXME: This is a temporary solution to retain the original flat sourceData structure
    // containing only style rules, even though CSSParser now provides the full rule source data tree.
    // Normally, we should just assign m_sourceData = sourceData;
    flattenSourceData(sourceData.get());
}

PassRefPtr<WebCore::CSSRuleSourceData> ParsedStyleSheet::ruleSourceDataAt(unsigned index) const
{
    if (!hasSourceData() || index >= m_sourceData->size())
        return 0;

    return m_sourceData->at(index);
}

namespace WebCore {

static PassOwnPtr<CSSParser> createCSSParser(Document* document)
{
    UseCounter* counter = 0;
    return adoptPtr(new CSSParser(document ? CSSParserContext(*document) : strictCSSParserContext(), counter));
}

namespace {

class StyleSheetHandler : public CSSParser::SourceDataHandler {
public:
    StyleSheetHandler(const String& parsedText, Document* document, StyleSheetContents* styleSheetContents, RuleSourceDataList* result)
        : m_parsedText(parsedText)
        , m_document(document)
        , m_styleSheetContents(styleSheetContents)
        , m_result(result)
        , m_propertyRangeStart(UINT_MAX)
        , m_selectorRangeStart(UINT_MAX)
        , m_commentRangeStart(UINT_MAX)
    {
        ASSERT(m_result);
    }

private:
    virtual void startRuleHeader(CSSRuleSourceData::Type, unsigned) OVERRIDE;
    virtual void endRuleHeader(unsigned) OVERRIDE;
    virtual void startSelector(unsigned) OVERRIDE;
    virtual void endSelector(unsigned) OVERRIDE;
    virtual void startRuleBody(unsigned) OVERRIDE;
    virtual void endRuleBody(unsigned, bool) OVERRIDE;
    virtual void startEndUnknownRule() OVERRIDE { addNewRuleToSourceTree(CSSRuleSourceData::createUnknown()); }
    virtual void startProperty(unsigned) OVERRIDE;
    virtual void endProperty(bool, bool, unsigned, CSSParser::ErrorType) OVERRIDE;
    virtual void startComment(unsigned) OVERRIDE;
    virtual void endComment(unsigned) OVERRIDE;

    void addNewRuleToSourceTree(PassRefPtr<CSSRuleSourceData>);
    PassRefPtr<CSSRuleSourceData> popRuleData();
    template <typename CharacterType> inline void setRuleHeaderEnd(const CharacterType*, unsigned);
    void fixUnparsedPropertyRanges(CSSRuleSourceData*);

    const String& m_parsedText;
    Document* m_document;
    StyleSheetContents* m_styleSheetContents;
    RuleSourceDataList* m_result;
    RuleSourceDataList m_currentRuleDataStack;
    RefPtr<CSSRuleSourceData> m_currentRuleData;
    OwnPtr<CSSParser> m_commentParser;
    unsigned m_propertyRangeStart;
    unsigned m_selectorRangeStart;
    unsigned m_commentRangeStart;
};

void StyleSheetHandler::startRuleHeader(CSSRuleSourceData::Type type, unsigned offset)
{
    // Pop off data for a previous invalid rule.
    if (m_currentRuleData)
        m_currentRuleDataStack.removeLast();

    RefPtr<CSSRuleSourceData> data = CSSRuleSourceData::create(type);
    data->ruleHeaderRange.start = offset;
    m_currentRuleData = data;
    m_currentRuleDataStack.append(data.release());
}

template <typename CharacterType>
inline void StyleSheetHandler::setRuleHeaderEnd(const CharacterType* dataStart, unsigned listEndOffset)
{
    while (listEndOffset > 1) {
        if (isHTMLSpace<CharacterType>(*(dataStart + listEndOffset - 1)))
            --listEndOffset;
        else
            break;
    }

    m_currentRuleDataStack.last()->ruleHeaderRange.end = listEndOffset;
}

void StyleSheetHandler::endRuleHeader(unsigned offset)
{
    ASSERT(!m_currentRuleDataStack.isEmpty());

    if (m_parsedText.is8Bit())
        setRuleHeaderEnd<LChar>(m_parsedText.characters8(), offset);
    else
        setRuleHeaderEnd<UChar>(m_parsedText.characters16(), offset);
}

void StyleSheetHandler::startSelector(unsigned offset)
{
    m_selectorRangeStart = offset;
}

void StyleSheetHandler::endSelector(unsigned offset)
{
    ASSERT(m_currentRuleDataStack.size());
    m_currentRuleDataStack.last()->selectorRanges.append(SourceRange(m_selectorRangeStart, offset));
    m_selectorRangeStart = UINT_MAX;
}

void StyleSheetHandler::startRuleBody(unsigned offset)
{
    m_currentRuleData.clear();
    ASSERT(!m_currentRuleDataStack.isEmpty());
    if (m_parsedText[offset] == '{')
        ++offset; // Skip the rule body opening brace.
    m_currentRuleDataStack.last()->ruleBodyRange.start = offset;
}

void StyleSheetHandler::endRuleBody(unsigned offset, bool error)
{
    ASSERT(!m_currentRuleDataStack.isEmpty());
    m_currentRuleDataStack.last()->ruleBodyRange.end = offset;
    m_propertyRangeStart = UINT_MAX;
    RefPtr<CSSRuleSourceData> rule = popRuleData();
    if (error)
        return;

    fixUnparsedPropertyRanges(rule.get());
    addNewRuleToSourceTree(rule.release());
}

void StyleSheetHandler::addNewRuleToSourceTree(PassRefPtr<CSSRuleSourceData> rule)
{
    if (m_currentRuleDataStack.isEmpty())
        m_result->append(rule);
    else
        m_currentRuleDataStack.last()->childRules.append(rule);
}

PassRefPtr<CSSRuleSourceData> StyleSheetHandler::popRuleData()
{
    ASSERT(!m_currentRuleDataStack.isEmpty());
    m_currentRuleData.clear();
    RefPtr<CSSRuleSourceData> data = m_currentRuleDataStack.last();
    m_currentRuleDataStack.removeLast();
    return data.release();
}

template <typename CharacterType>
static inline void fixUnparsedProperties(const CharacterType* characters, CSSRuleSourceData* ruleData)
{
    Vector<CSSPropertySourceData>& propertyData = ruleData->styleSourceData->propertyData;
    unsigned size = propertyData.size();
    if (!size)
        return;

    unsigned styleStart = ruleData->ruleBodyRange.start;
    CSSPropertySourceData* nextData = &(propertyData.at(0));
    for (unsigned i = 0; i < size; ++i) {
        CSSPropertySourceData* currentData = nextData;
        nextData = i < size - 1 ? &(propertyData.at(i + 1)) : 0;

        if (currentData->parsedOk)
            continue;
        if (currentData->range.end > 0 && characters[styleStart + currentData->range.end - 1] == ';')
            continue;

        unsigned propertyEndInStyleSheet;
        if (!nextData)
            propertyEndInStyleSheet = ruleData->ruleBodyRange.end - 1;
        else
            propertyEndInStyleSheet = styleStart + nextData->range.start - 1;

        while (isHTMLSpace<CharacterType>(characters[propertyEndInStyleSheet]))
            --propertyEndInStyleSheet;

        // propertyEndInStyleSheet points at the last property text character.
        unsigned newPropertyEnd = propertyEndInStyleSheet - styleStart + 1; // Exclusive of the last property text character.
        if (currentData->range.end != newPropertyEnd) {
            currentData->range.end = newPropertyEnd;
            unsigned valueStartInStyleSheet = styleStart + currentData->range.start + currentData->name.length();
            while (valueStartInStyleSheet < propertyEndInStyleSheet && characters[valueStartInStyleSheet] != ':')
                ++valueStartInStyleSheet;
            if (valueStartInStyleSheet < propertyEndInStyleSheet)
                ++valueStartInStyleSheet; // Shift past the ':'.
            while (valueStartInStyleSheet < propertyEndInStyleSheet && isHTMLSpace<CharacterType>(characters[valueStartInStyleSheet]))
                ++valueStartInStyleSheet;
            // Need to exclude the trailing ';' from the property value.
            currentData->value = String(characters + valueStartInStyleSheet, propertyEndInStyleSheet - valueStartInStyleSheet + (characters[propertyEndInStyleSheet] == ';' ? 0 : 1));
        }
    }
}

void StyleSheetHandler::fixUnparsedPropertyRanges(CSSRuleSourceData* ruleData)
{
    if (!ruleData->styleSourceData)
        return;

    if (m_parsedText.is8Bit()) {
        fixUnparsedProperties<LChar>(m_parsedText.characters8(), ruleData);
        return;
    }

    fixUnparsedProperties<UChar>(m_parsedText.characters16(), ruleData);
}

void StyleSheetHandler::startProperty(unsigned offset)
{
    if (m_currentRuleDataStack.isEmpty() || !m_currentRuleDataStack.last()->styleSourceData)
        return;
    m_propertyRangeStart = offset;
}

void StyleSheetHandler::endProperty(bool isImportant, bool isParsed, unsigned offset, CSSParser::ErrorType errorType)
{
    if (errorType != CSSParser::NoError)
        m_propertyRangeStart = UINT_MAX;

    if (m_propertyRangeStart == UINT_MAX || m_currentRuleDataStack.isEmpty() || !m_currentRuleDataStack.last()->styleSourceData)
        return;

    ASSERT(offset <= m_parsedText.length());
    if (offset < m_parsedText.length() && m_parsedText[offset] == ';') // Include semicolon into the property text.
        ++offset;

    const unsigned start = m_propertyRangeStart;
    const unsigned end = offset;
    ASSERT(start < end);
    String propertyString = m_parsedText.substring(start, end - start).stripWhiteSpace();
    if (propertyString.endsWith(';'))
        propertyString = propertyString.left(propertyString.length() - 1);
    size_t colonIndex = propertyString.find(':');
    ASSERT(colonIndex != kNotFound);

    String name = propertyString.left(colonIndex).stripWhiteSpace();
    String value = propertyString.substring(colonIndex + 1, propertyString.length()).stripWhiteSpace();
    // The property range is relative to the declaration start offset.
    unsigned topRuleBodyRangeStart = m_currentRuleDataStack.last()->ruleBodyRange.start;
    m_currentRuleDataStack.last()->styleSourceData->propertyData.append(
        CSSPropertySourceData(name, value, isImportant, false, isParsed, SourceRange(start - topRuleBodyRangeStart, end - topRuleBodyRangeStart)));
    m_propertyRangeStart = UINT_MAX;
}

void StyleSheetHandler::startComment(unsigned offset)
{
    ASSERT(m_commentRangeStart == UINT_MAX);
    m_commentRangeStart = offset;
}

void StyleSheetHandler::endComment(unsigned offset)
{
    ASSERT(offset <= m_parsedText.length());

    unsigned startOffset = m_commentRangeStart;
    m_commentRangeStart = UINT_MAX;
    if (m_propertyRangeStart != UINT_MAX) {
        ASSERT(startOffset >= m_propertyRangeStart);
        // startProperty() is called automatically at the start of a style declaration.
        // Check if no text has been scanned yet, otherwise the comment is inside a property.
        if (!m_parsedText.substring(m_propertyRangeStart, startOffset).stripWhiteSpace().isEmpty())
            return;
        m_propertyRangeStart = UINT_MAX;
    }
    if (m_currentRuleDataStack.isEmpty() || !m_currentRuleDataStack.last()->ruleHeaderRange.end || !m_currentRuleDataStack.last()->styleSourceData)
        return;

    // The lexer is not inside a property AND it is scanning a declaration-aware rule body.
    String commentText = m_parsedText.substring(startOffset, offset - startOffset);

    ASSERT(commentText.startsWith("/*"));
    commentText = commentText.substring(2);

    // Require well-formed comments.
    if (!commentText.endsWith("*/"))
        return;
    commentText = commentText.substring(0, commentText.length() - 2).stripWhiteSpace();
    if (commentText.isEmpty())
        return;

    // FIXME: Use the actual rule type rather than STYLE_RULE?
    if (!m_commentParser)
        m_commentParser = createCSSParser(m_document);
    RuleSourceDataList sourceData;

    // FIXME: Use another subclass of CSSParser::SourceDataHandler and assert that
    // no comments are encountered (will not need m_document and m_styleSheetContents).
    StyleSheetHandler handler(commentText, m_document, m_styleSheetContents, &sourceData);
    RefPtr<MutableStylePropertySet> tempMutableStyle = MutableStylePropertySet::create();
    m_commentParser->parseDeclaration(tempMutableStyle.get(), commentText, &handler, m_styleSheetContents);
    Vector<CSSPropertySourceData>& commentPropertyData = sourceData.first()->styleSourceData->propertyData;
    if (commentPropertyData.size() != 1)
        return;
    CSSPropertySourceData& propertyData = commentPropertyData.at(0);
    if (propertyData.range.length() != commentText.length())
        return;

    unsigned topRuleBodyRangeStart = m_currentRuleDataStack.last()->ruleBodyRange.start;
    m_currentRuleDataStack.last()->styleSourceData->propertyData.append(
        CSSPropertySourceData(propertyData.name, propertyData.value, false, true, true, SourceRange(startOffset - topRuleBodyRangeStart, offset - topRuleBodyRangeStart)));
}

} // namespace

enum MediaListSource {
    MediaListSourceLinkedSheet,
    MediaListSourceInlineSheet,
    MediaListSourceMediaRule,
    MediaListSourceImportRule
};

static PassRefPtr<TypeBuilder::CSS::SourceRange> buildSourceRangeObject(const SourceRange& range, Vector<unsigned>* lineEndings)
{
    if (!lineEndings)
        return 0;
    TextPosition start = TextPosition::fromOffsetAndLineEndings(range.start, *lineEndings);
    TextPosition end = TextPosition::fromOffsetAndLineEndings(range.end, *lineEndings);

    RefPtr<TypeBuilder::CSS::SourceRange> result = TypeBuilder::CSS::SourceRange::create()
        .setStartLine(start.m_line.zeroBasedInt())
        .setStartColumn(start.m_column.zeroBasedInt())
        .setEndLine(end.m_line.zeroBasedInt())
        .setEndColumn(end.m_column.zeroBasedInt());
    return result.release();
}

static PassRefPtr<CSSRuleList> asCSSRuleList(CSSStyleSheet* styleSheet)
{
    if (!styleSheet)
        return 0;

    RefPtr<StaticCSSRuleList> list = StaticCSSRuleList::create();
    Vector<RefPtr<CSSRule> >& listRules = list->rules();
    for (unsigned i = 0, size = styleSheet->length(); i < size; ++i) {
        CSSRule* item = styleSheet->item(i);
        if (item->type() == CSSRule::CHARSET_RULE)
            continue;
        listRules.append(item);
    }
    return list.release();
}

static PassRefPtr<CSSRuleList> asCSSRuleList(CSSRule* rule)
{
    if (!rule)
        return 0;

    if (rule->type() == CSSRule::MEDIA_RULE)
        return static_cast<CSSMediaRule*>(rule)->cssRules();

    if (rule->type() == CSSRule::KEYFRAMES_RULE)
        return static_cast<CSSKeyframesRule*>(rule)->cssRules();

    if (rule->type() == CSSRule::HOST_RULE)
        return static_cast<CSSHostRule*>(rule)->cssRules();

    if (rule->type() == CSSRule::SUPPORTS_RULE)
        return static_cast<CSSSupportsRule*>(rule)->cssRules();

    return 0;
}

PassRefPtr<InspectorStyle> InspectorStyle::create(const InspectorCSSId& styleId, PassRefPtr<CSSStyleDeclaration> style, InspectorStyleSheet* parentStyleSheet)
{
    return adoptRef(new InspectorStyle(styleId, style, parentStyleSheet));
}

InspectorStyle::InspectorStyle(const InspectorCSSId& styleId, PassRefPtr<CSSStyleDeclaration> style, InspectorStyleSheet* parentStyleSheet)
    : m_styleId(styleId)
    , m_style(style)
    , m_parentStyleSheet(parentStyleSheet)
    , m_formatAcquired(false)
{
    ASSERT(m_style);
}

InspectorStyle::~InspectorStyle()
{
}

PassRefPtr<TypeBuilder::CSS::CSSStyle> InspectorStyle::buildObjectForStyle() const
{
    RefPtr<TypeBuilder::CSS::CSSStyle> result = styleWithProperties();
    if (!m_styleId.isEmpty())
        result->setStyleId(m_styleId.asProtocolValue<TypeBuilder::CSS::CSSStyleId>());

    result->setWidth(m_style->getPropertyValue("width"));
    result->setHeight(m_style->getPropertyValue("height"));

    RefPtr<CSSRuleSourceData> sourceData = extractSourceData();
    if (sourceData)
        result->setRange(buildSourceRangeObject(sourceData->ruleBodyRange, m_parentStyleSheet->lineEndings().get()));

    return result.release();
}

PassRefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSComputedStyleProperty> > InspectorStyle::buildArrayForComputedStyle() const
{
    RefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSComputedStyleProperty> > result = TypeBuilder::Array<TypeBuilder::CSS::CSSComputedStyleProperty>::create();
    Vector<InspectorStyleProperty> properties;
    populateAllProperties(properties);

    for (Vector<InspectorStyleProperty>::iterator it = properties.begin(), itEnd = properties.end(); it != itEnd; ++it) {
        const CSSPropertySourceData& propertyEntry = it->sourceData;
        RefPtr<TypeBuilder::CSS::CSSComputedStyleProperty> entry = TypeBuilder::CSS::CSSComputedStyleProperty::create()
            .setName(propertyEntry.name)
            .setValue(propertyEntry.value);
        result->addItem(entry);
    }

    return result.release();
}

// This method does the following preprocessing of |propertyText| with |overwrite| == false and |index| past the last active property:
// - If the last property (if present) has no closing ";", the ";" is prepended to the current |propertyText| value.
// - A heuristic formatting is attempted to retain the style structure.
//
// The propertyText (if not empty) is checked to be a valid style declaration (containing at least one property). If not,
// the method returns false (denoting an error).
bool InspectorStyle::setPropertyText(unsigned index, const String& propertyText, bool overwrite, String* oldText, ExceptionState& es)
{
    ASSERT(m_parentStyleSheet);
    DEFINE_STATIC_LOCAL(String, bogusPropertyName, ("-webkit-boguz-propertee"));

    if (!m_parentStyleSheet->ensureParsedDataReady()) {
        es.throwDOMException(NotFoundError);
        return false;
    }

    if (!propertyText.stripWhiteSpace().isEmpty()) {
        RefPtr<MutableStylePropertySet> tempMutableStyle = MutableStylePropertySet::create();
        String declarationText = propertyText + " " + bogusPropertyName + ": none";
        RuleSourceDataList sourceData;
        StyleSheetHandler handler(declarationText, ownerDocument(), m_style->parentStyleSheet()->contents(), &sourceData);
        createCSSParser(ownerDocument())->parseDeclaration(tempMutableStyle.get(), declarationText, &handler, m_style->parentStyleSheet()->contents());
        Vector<CSSPropertySourceData>& propertyData = sourceData.first()->styleSourceData->propertyData;
        unsigned propertyCount = propertyData.size();

        // At least one property + the bogus property added just above should be present.
        if (propertyCount < 2) {
            es.throwDOMException(SyntaxError);
            return false;
        }

        // Check for the proper propertyText termination (the parser could at least restore to the PROPERTY_NAME state).
        if (propertyData.at(propertyCount - 1).name != bogusPropertyName) {
            es.throwDOMException(SyntaxError);
            return false;
        }
    }

    RefPtr<CSSRuleSourceData> sourceData = extractSourceData();
    if (!sourceData) {
        es.throwDOMException(NotFoundError);
        return false;
    }

    String text;
    bool success = styleText(&text);
    if (!success) {
        es.throwDOMException(NotFoundError);
        return false;
    }

    Vector<InspectorStyleProperty> allProperties;
    populateAllProperties(allProperties);

    InspectorStyleTextEditor editor(&allProperties, text, newLineAndWhitespaceDelimiters());
    if (overwrite) {
        if (index >= allProperties.size()) {
            es.throwDOMException(IndexSizeError);
            return false;
        }
        *oldText = allProperties.at(index).rawText;
        editor.replaceProperty(index, propertyText);
    } else
        editor.insertProperty(index, propertyText, sourceData->ruleBodyRange.length());

    return applyStyleText(editor.styleText());
}

bool InspectorStyle::toggleProperty(unsigned index, bool disable, ExceptionState& es)
{
    ASSERT(m_parentStyleSheet);
    if (!m_parentStyleSheet->ensureParsedDataReady()) {
        es.throwDOMException(NoModificationAllowedError);
        return false;
    }

    RefPtr<CSSRuleSourceData> sourceData = extractSourceData();
    if (!sourceData) {
        es.throwDOMException(NotFoundError);
        return false;
    }

    String text;
    bool success = styleText(&text);
    if (!success) {
        es.throwDOMException(NotFoundError);
        return false;
    }

    Vector<InspectorStyleProperty> allProperties;
    populateAllProperties(allProperties);
    if (index >= allProperties.size()) {
        es.throwDOMException(IndexSizeError);
        return false;
    }

    InspectorStyleProperty& property = allProperties.at(index);
    if (property.sourceData.disabled == disable)
        return true; // Idempotent operation.

    InspectorStyleTextEditor editor(&allProperties, text, newLineAndWhitespaceDelimiters());
    if (disable)
        editor.disableProperty(index);
    else
        editor.enableProperty(index);

    return applyStyleText(editor.styleText());
}

bool InspectorStyle::styleText(String* result) const
{
    RefPtr<CSSRuleSourceData> sourceData = extractSourceData();
    if (!sourceData)
        return false;

    String styleSheetText;
    bool success = m_parentStyleSheet->getText(&styleSheetText);
    if (!success)
        return false;

    SourceRange& bodyRange = sourceData->ruleBodyRange;
    *result = styleSheetText.substring(bodyRange.start, bodyRange.end - bodyRange.start);
    return true;
}

void InspectorStyle::populateAllProperties(Vector<InspectorStyleProperty>& result) const
{
    HashSet<String> sourcePropertyNames;

    RefPtr<CSSRuleSourceData> sourceData = extractSourceData();
    if (sourceData) {
        String styleDeclaration;
        bool isStyleTextKnown = styleText(&styleDeclaration);
        ASSERT_UNUSED(isStyleTextKnown, isStyleTextKnown);
        Vector<CSSPropertySourceData>& sourcePropertyData = sourceData->styleSourceData->propertyData;
        for (Vector<CSSPropertySourceData>::const_iterator it = sourcePropertyData.begin(); it != sourcePropertyData.end(); ++it) {
            InspectorStyleProperty p(*it, true);
            p.setRawTextFromStyleDeclaration(styleDeclaration);
            result.append(p);
            sourcePropertyNames.add(it->name.lower());
        }
    }

    for (int i = 0, size = m_style->length(); i < size; ++i) {
        String name = m_style->item(i);
        if (!sourcePropertyNames.add(name.lower()).isNewEntry)
            continue;

        result.append(InspectorStyleProperty(CSSPropertySourceData(name, m_style->getPropertyValue(name), !m_style->getPropertyPriority(name).isEmpty(), false, true, SourceRange()), false));
    }
}

PassRefPtr<TypeBuilder::CSS::CSSStyle> InspectorStyle::styleWithProperties() const
{
    RefPtr<Array<TypeBuilder::CSS::CSSProperty> > propertiesObject = Array<TypeBuilder::CSS::CSSProperty>::create();
    RefPtr<Array<TypeBuilder::CSS::ShorthandEntry> > shorthandEntries = Array<TypeBuilder::CSS::ShorthandEntry>::create();
    HashMap<String, RefPtr<TypeBuilder::CSS::CSSProperty> > propertyNameToPreviousActiveProperty;
    HashSet<String> foundShorthands;
    String previousPriority;
    String previousStatus;
    OwnPtr<Vector<unsigned> > lineEndings(m_parentStyleSheet ? m_parentStyleSheet->lineEndings() : PassOwnPtr<Vector<unsigned> >());
    RefPtr<CSSRuleSourceData> sourceData = extractSourceData();
    unsigned ruleBodyRangeStart = sourceData ? sourceData->ruleBodyRange.start : 0;

    Vector<InspectorStyleProperty> properties;
    populateAllProperties(properties);

    for (Vector<InspectorStyleProperty>::iterator it = properties.begin(), itEnd = properties.end(); it != itEnd; ++it) {
        const CSSPropertySourceData& propertyEntry = it->sourceData;
        const String& name = propertyEntry.name;
        const bool disabled = it->sourceData.disabled;

        TypeBuilder::CSS::CSSProperty::Status::Enum status = disabled ? TypeBuilder::CSS::CSSProperty::Status::Disabled : TypeBuilder::CSS::CSSProperty::Status::Active;

        RefPtr<TypeBuilder::CSS::CSSProperty> property = TypeBuilder::CSS::CSSProperty::create()
            .setName(name)
            .setValue(propertyEntry.value);
        propertiesObject->addItem(property);

        // Default "parsedOk" == true.
        if (!propertyEntry.parsedOk)
            property->setParsedOk(false);
        if (it->hasRawText())
            property->setText(it->rawText);

        // Default "priority" == "".
        if (propertyEntry.important)
            property->setPriority("important");
        if (it->hasSource) {
            // The property range is relative to the style body start.
            // Should be converted into an absolute range (relative to the stylesheet start)
            // for the proper conversion into line:column.
            SourceRange absolutePropertyRange = propertyEntry.range;
            absolutePropertyRange.start += ruleBodyRangeStart;
            absolutePropertyRange.end += ruleBodyRangeStart;
            property->setRange(buildSourceRangeObject(absolutePropertyRange, lineEndings.get()));
        }
        if (!disabled) {
            if (it->hasSource) {
                ASSERT(sourceData);
                property->setImplicit(false);

                // Parsed property overrides any property with the same name. Non-parsed property overrides
                // previous non-parsed property with the same name (if any).
                bool shouldInactivate = false;
                CSSPropertyID propertyId = cssPropertyID(name);
                // Canonicalize property names to treat non-prefixed and vendor-prefixed property names the same (opacity vs. -webkit-opacity).
                String canonicalPropertyName = propertyId ? getPropertyNameString(propertyId) : name;
                HashMap<String, RefPtr<TypeBuilder::CSS::CSSProperty> >::iterator activeIt = propertyNameToPreviousActiveProperty.find(canonicalPropertyName);
                if (activeIt != propertyNameToPreviousActiveProperty.end()) {
                    if (propertyEntry.parsedOk) {
                        bool successPriority = activeIt->value->getString(TypeBuilder::CSS::CSSProperty::Priority, &previousPriority);
                        bool successStatus = activeIt->value->getString(TypeBuilder::CSS::CSSProperty::Status, &previousStatus);
                        if (successStatus && previousStatus != "inactive") {
                            if (propertyEntry.important || !successPriority) // Priority not set == "not important".
                                shouldInactivate = true;
                            else if (status == TypeBuilder::CSS::CSSProperty::Status::Active) {
                                // Inactivate a non-important property following the same-named important property.
                                status = TypeBuilder::CSS::CSSProperty::Status::Inactive;
                            }
                        }
                    } else {
                        bool previousParsedOk;
                        bool success = activeIt->value->getBoolean(TypeBuilder::CSS::CSSProperty::ParsedOk, &previousParsedOk);
                        if (success && !previousParsedOk)
                            shouldInactivate = true;
                    }
                } else
                    propertyNameToPreviousActiveProperty.set(canonicalPropertyName, property);

                if (shouldInactivate) {
                    activeIt->value->setStatus(TypeBuilder::CSS::CSSProperty::Status::Inactive);
                    propertyNameToPreviousActiveProperty.set(canonicalPropertyName, property);
                }
            } else {
                bool implicit = m_style->isPropertyImplicit(name);
                // Default "implicit" == false.
                if (implicit)
                    property->setImplicit(true);
                status = TypeBuilder::CSS::CSSProperty::Status::Style;

                String shorthand = m_style->getPropertyShorthand(name);
                if (!shorthand.isEmpty()) {
                    if (foundShorthands.add(shorthand).isNewEntry) {
                        RefPtr<TypeBuilder::CSS::ShorthandEntry> entry = TypeBuilder::CSS::ShorthandEntry::create()
                            .setName(shorthand)
                            .setValue(shorthandValue(shorthand));
                        shorthandEntries->addItem(entry);
                    }
                }
            }
        }

        // Default "status" == "style".
        if (status != TypeBuilder::CSS::CSSProperty::Status::Style)
            property->setStatus(status);
    }

    RefPtr<TypeBuilder::CSS::CSSStyle> result = TypeBuilder::CSS::CSSStyle::create()
        .setCssProperties(propertiesObject)
        .setShorthandEntries(shorthandEntries);
    return result.release();
}

PassRefPtr<CSSRuleSourceData> InspectorStyle::extractSourceData() const
{
    if (!m_parentStyleSheet || !m_parentStyleSheet->ensureParsedDataReady())
        return 0;
    return m_parentStyleSheet->ruleSourceDataFor(m_style.get());
}

bool InspectorStyle::applyStyleText(const String& text)
{
    return m_parentStyleSheet->setStyleText(m_style.get(), text);
}

String InspectorStyle::shorthandValue(const String& shorthandProperty) const
{
    String value = m_style->getPropertyValue(shorthandProperty);
    if (value.isEmpty()) {
        StringBuilder builder;

        for (unsigned i = 0; i < m_style->length(); ++i) {
            String individualProperty = m_style->item(i);
            if (m_style->getPropertyShorthand(individualProperty) != shorthandProperty)
                continue;
            if (m_style->isPropertyImplicit(individualProperty))
                continue;
            String individualValue = m_style->getPropertyValue(individualProperty);
            if (individualValue == "initial")
                continue;
            if (!builder.isEmpty())
                builder.append(" ");
            builder.append(individualValue);
        }

        return builder.toString();
    }
    return value;
}

String InspectorStyle::shorthandPriority(const String& shorthandProperty) const
{
    String priority = m_style->getPropertyPriority(shorthandProperty);
    if (priority.isEmpty()) {
        for (unsigned i = 0; i < m_style->length(); ++i) {
            String individualProperty = m_style->item(i);
            if (m_style->getPropertyShorthand(individualProperty) != shorthandProperty)
                continue;
            priority = m_style->getPropertyPriority(individualProperty);
            break;
        }
    }
    return priority;
}

Vector<String> InspectorStyle::longhandProperties(const String& shorthandProperty) const
{
    Vector<String> properties;
    HashSet<String> foundProperties;
    for (unsigned i = 0; i < m_style->length(); ++i) {
        String individualProperty = m_style->item(i);
        if (foundProperties.contains(individualProperty) || m_style->getPropertyShorthand(individualProperty) != shorthandProperty)
            continue;

        foundProperties.add(individualProperty);
        properties.append(individualProperty);
    }
    return properties;
}

NewLineAndWhitespace& InspectorStyle::newLineAndWhitespaceDelimiters() const
{
    DEFINE_STATIC_LOCAL(String, defaultPrefix, ("    "));

    if (m_formatAcquired)
        return m_format;

    RefPtr<CSSRuleSourceData> sourceData = extractSourceData();
    Vector<CSSPropertySourceData>* sourcePropertyData = sourceData ? &(sourceData->styleSourceData->propertyData) : 0;
    int propertyCount;
    if (!sourcePropertyData || !(propertyCount = sourcePropertyData->size())) {
        m_format.first = "\n";
        m_format.second = defaultPrefix;
        return m_format; // Do not remember the default formatting and attempt to acquire it later.
    }

    String text;
    bool success = styleText(&text);
    ASSERT_UNUSED(success, success);

    m_formatAcquired = true;

    String candidatePrefix = defaultPrefix;
    StringBuilder formatLineFeed;
    StringBuilder prefix;
    int scanStart = 0;
    int propertyIndex = 0;
    bool isFullPrefixScanned = false;
    bool lineFeedTerminated = false;
    while (propertyIndex < propertyCount) {
        const WebCore::CSSPropertySourceData& currentProperty = sourcePropertyData->at(propertyIndex++);

        bool processNextProperty = false;
        int scanEnd = currentProperty.range.start;
        for (int i = scanStart; i < scanEnd; ++i) {
            UChar ch = text[i];
            bool isLineFeed = isHTMLLineBreak(ch);
            if (isLineFeed) {
                if (!lineFeedTerminated)
                    formatLineFeed.append(ch);
                prefix.clear();
            } else if (isHTMLSpace<UChar>(ch))
                prefix.append(ch);
            else {
                candidatePrefix = prefix.toString();
                prefix.clear();
                scanStart = currentProperty.range.end;
                ++propertyIndex;
                processNextProperty = true;
                break;
            }
            if (!isLineFeed && formatLineFeed.length())
                lineFeedTerminated = true;
        }
        if (!processNextProperty) {
            isFullPrefixScanned = true;
            break;
        }
    }

    m_format.first = formatLineFeed.toString();
    m_format.second = isFullPrefixScanned ? prefix.toString() : candidatePrefix;
    return m_format;
}

Document* InspectorStyle::ownerDocument() const
{
    return m_parentStyleSheet->pageStyleSheet() ? m_parentStyleSheet->pageStyleSheet()->ownerDocument() : 0;
}

PassRefPtr<InspectorStyleSheet> InspectorStyleSheet::create(InspectorPageAgent* pageAgent, const String& id, PassRefPtr<CSSStyleSheet> pageStyleSheet, TypeBuilder::CSS::StyleSheetOrigin::Enum origin, const String& documentURL, Listener* listener)
{
    return adoptRef(new InspectorStyleSheet(pageAgent, id, pageStyleSheet, origin, documentURL, listener));
}

// static
String InspectorStyleSheet::styleSheetURL(CSSStyleSheet* pageStyleSheet)
{
    if (pageStyleSheet && !pageStyleSheet->contents()->baseURL().isEmpty())
        return pageStyleSheet->contents()->baseURL().string();
    return emptyString();
}

// static
void InspectorStyleSheet::collectFlatRules(PassRefPtr<CSSRuleList> ruleList, CSSRuleVector* result)
{
    if (!ruleList)
        return;

    for (unsigned i = 0, size = ruleList->length(); i < size; ++i) {
        CSSRule* rule = ruleList->item(i);

        // The result->append()'ed types should be exactly the same as in ParsedStyleSheet::flattenSourceData().
        switch (rule->type()) {
        case CSSRule::STYLE_RULE:
            result->append(rule);
            continue;
        case CSSRule::IMPORT_RULE:
        case CSSRule::MEDIA_RULE:
            result->append(rule);
            break;
        default:
            break;
        }
        RefPtr<CSSRuleList> childRuleList = asCSSRuleList(rule);
        if (childRuleList)
            collectFlatRules(childRuleList, result);
    }
}

InspectorStyleSheet::InspectorStyleSheet(InspectorPageAgent* pageAgent, const String& id, PassRefPtr<CSSStyleSheet> pageStyleSheet, TypeBuilder::CSS::StyleSheetOrigin::Enum origin, const String& documentURL, Listener* listener)
    : m_pageAgent(pageAgent)
    , m_id(id)
    , m_pageStyleSheet(pageStyleSheet)
    , m_origin(origin)
    , m_documentURL(documentURL)
    , m_isRevalidating(false)
    , m_isReparsing(false)
    , m_listener(listener)
{
    m_parsedStyleSheet = new ParsedStyleSheet();
}

InspectorStyleSheet::~InspectorStyleSheet()
{
    delete m_parsedStyleSheet;
}

String InspectorStyleSheet::finalURL() const
{
    String url = styleSheetURL(m_pageStyleSheet.get());
    return url.isEmpty() ? m_documentURL : url;
}

void InspectorStyleSheet::reparseStyleSheet(const String& text)
{
    if (m_listener)
        m_listener->willReparseStyleSheet();

    {
        // Have a separate scope for clearRules() (bug 95324).
        CSSStyleSheet::RuleMutationScope mutationScope(m_pageStyleSheet.get());
        m_pageStyleSheet->contents()->clearRules();
        m_pageStyleSheet->clearChildRuleCSSOMWrappers();
    }
    {
        m_isReparsing = true;
        CSSStyleSheet::RuleMutationScope mutationScope(m_pageStyleSheet.get());
        m_pageStyleSheet->contents()->parseString(text);
        m_isReparsing = false;
    }

    if (m_listener)
        m_listener->didReparseStyleSheet();
    fireStyleSheetChanged();
    m_pageStyleSheet->ownerDocument()->styleResolverChanged(RecalcStyleImmediately, FullStyleUpdate);
}

bool InspectorStyleSheet::setText(const String& text, ExceptionState& es)
{
    if (!checkPageStyleSheet(es))
        return false;
    if (!m_parsedStyleSheet)
        return false;

    m_parsedStyleSheet->setText(text);
    m_flatRules.clear();

    return true;
}

String InspectorStyleSheet::ruleSelector(const InspectorCSSId& id, ExceptionState& es)
{
    CSSStyleRule* rule = ruleForId(id);
    if (!rule) {
        es.throwDOMException(NotFoundError);
        return "";
    }
    return rule->selectorText();
}

bool InspectorStyleSheet::setRuleSelector(const InspectorCSSId& id, const String& selector, ExceptionState& es)
{
    if (!checkPageStyleSheet(es))
        return false;
    CSSStyleRule* rule = ruleForId(id);
    if (!rule) {
        es.throwDOMException(NotFoundError);
        return false;
    }
    CSSStyleSheet* styleSheet = rule->parentStyleSheet();
    if (!styleSheet || !ensureParsedDataReady()) {
        es.throwDOMException(NotFoundError);
        return false;
    }

    rule->setSelectorText(selector);
    RefPtr<CSSRuleSourceData> sourceData = ruleSourceDataFor(rule->style());
    if (!sourceData) {
        es.throwDOMException(NotFoundError);
        return false;
    }

    String sheetText = m_parsedStyleSheet->text();
    sheetText.replace(sourceData->ruleHeaderRange.start, sourceData->ruleHeaderRange.length(), selector);
    m_parsedStyleSheet->setText(sheetText);
    fireStyleSheetChanged();
    return true;
}

static bool checkStyleRuleSelector(Document* document, const String& selector)
{
    CSSSelectorList selectorList;
    createCSSParser(document)->parseSelector(selector, selectorList);
    return selectorList.isValid();
}

CSSStyleRule* InspectorStyleSheet::addRule(const String& selector, ExceptionState& es)
{
    if (!checkPageStyleSheet(es))
        return 0;
    if (!checkStyleRuleSelector(m_pageStyleSheet->ownerDocument(), selector)) {
        es.throwDOMException(SyntaxError);
        return 0;
    }

    String text;
    bool success = getText(&text);
    if (!success) {
        es.throwDOMException(NotFoundError);
        return 0;
    }
    StringBuilder styleSheetText;
    styleSheetText.append(text);

    m_pageStyleSheet->addRule(selector, "", es);
    if (es.hadException())
        return 0;
    ASSERT(m_pageStyleSheet->length());
    unsigned lastRuleIndex = m_pageStyleSheet->length() - 1;
    CSSRule* rule = m_pageStyleSheet->item(lastRuleIndex);
    ASSERT(rule);

    CSSStyleRule* styleRule = InspectorCSSAgent::asCSSStyleRule(rule);
    if (!styleRule) {
        // What we just added has to be a CSSStyleRule - we cannot handle other types of rules yet.
        // If it is not a style rule, pretend we never touched the stylesheet.
        m_pageStyleSheet->deleteRule(lastRuleIndex, ASSERT_NO_EXCEPTION);
        es.throwDOMException(SyntaxError);
        return 0;
    }

    if (!styleSheetText.isEmpty())
        styleSheetText.append('\n');

    styleSheetText.append(selector);
    styleSheetText.appendLiteral(" {}");
    // Using setText() as this operation changes the style sheet rule set.
    setText(styleSheetText.toString(), ASSERT_NO_EXCEPTION);

    fireStyleSheetChanged();

    return styleRule;
}

bool InspectorStyleSheet::deleteRule(const InspectorCSSId& id, ExceptionState& es)
{
    if (!checkPageStyleSheet(es))
        return false;
    RefPtr<CSSStyleRule> rule = ruleForId(id);
    if (!rule) {
        es.throwDOMException(NotFoundError);
        return false;
    }
    CSSStyleSheet* styleSheet = rule->parentStyleSheet();
    if (!styleSheet || !ensureParsedDataReady()) {
        es.throwDOMException(NotFoundError);
        return false;
    }

    RefPtr<CSSRuleSourceData> sourceData = ruleSourceDataFor(rule->style());
    if (!sourceData) {
        es.throwDOMException(NotFoundError);
        return false;
    }

    styleSheet->deleteRule(id.ordinal(), es);
    // |rule| MAY NOT be addressed after this line!

    if (es.hadException())
        return false;

    String sheetText = m_parsedStyleSheet->text();
    sheetText.remove(sourceData->ruleHeaderRange.start, sourceData->ruleBodyRange.end - sourceData->ruleHeaderRange.start + 1);
    setText(sheetText, ASSERT_NO_EXCEPTION);
    fireStyleSheetChanged();
    return true;
}

CSSStyleRule* InspectorStyleSheet::ruleForId(const InspectorCSSId& id) const
{
    if (!m_pageStyleSheet)
        return 0;

    ASSERT(!id.isEmpty());
    ensureFlatRules();
    return InspectorCSSAgent::asCSSStyleRule(id.ordinal() >= m_flatRules.size() ? 0 : m_flatRules.at(id.ordinal()).get());
}

bool InspectorStyleSheet::fillObjectForStyleSheet(PassRefPtr<TypeBuilder::CSS::CSSStyleSheetBody> prpResult)
{
    CSSStyleSheet* styleSheet = pageStyleSheet();
    if (!styleSheet)
        return false;

    RefPtr<TypeBuilder::CSS::CSSStyleSheetBody> result = prpResult;

    String styleSheetText;
    bool success = getText(&styleSheetText);
    if (success)
        result->setText(styleSheetText);
    return success;
}

PassRefPtr<TypeBuilder::CSS::CSSStyleSheetHeader> InspectorStyleSheet::buildObjectForStyleSheetInfo() const
{
    CSSStyleSheet* styleSheet = pageStyleSheet();
    if (!styleSheet)
        return 0;

    Document* document = styleSheet->ownerDocument();
    Frame* frame = document ? document->frame() : 0;

    RefPtr<TypeBuilder::CSS::CSSStyleSheetHeader> result = TypeBuilder::CSS::CSSStyleSheetHeader::create()
        .setStyleSheetId(id())
        .setOrigin(m_origin)
        .setDisabled(styleSheet->disabled())
        .setSourceURL(url())
        .setTitle(styleSheet->title())
        .setFrameId(m_pageAgent->frameId(frame))
        .setIsInline(styleSheet->isInline() && !startsAtZero())
        .setStartLine(styleSheet->startPositionInSource().m_line.zeroBasedInt())
        .setStartColumn(styleSheet->startPositionInSource().m_column.zeroBasedInt());

    if (hasSourceURL())
        result->setHasSourceURL(true);

    String sourceMapURLValue = sourceMapURL();
    if (!sourceMapURLValue.isEmpty())
        result->setSourceMapURL(sourceMapURLValue);
    return result.release();
}

static PassRefPtr<TypeBuilder::Array<String> > selectorsFromSource(const CSSRuleSourceData* sourceData, const String& sheetText)
{
    RegularExpression comment("/\\*[^]*?\\*/", TextCaseSensitive, MultilineEnabled);
    RefPtr<TypeBuilder::Array<String> > result = TypeBuilder::Array<String>::create();
    const SelectorRangeList& ranges = sourceData->selectorRanges;
    for (size_t i = 0, size = ranges.size(); i < size; ++i) {
        const SourceRange& range = ranges.at(i);
        String selector = sheetText.substring(range.start, range.length());

        // We don't want to see any comments in the selector components, only the meaningful parts.
        int matchLength;
        int offset = 0;
        while ((offset = comment.match(selector, offset, &matchLength)) >= 0)
            selector.replace(offset, matchLength, "");

        result->addItem(selector.stripWhiteSpace());
    }
    return result.release();
}

PassRefPtr<TypeBuilder::CSS::SelectorList> InspectorStyleSheet::buildObjectForSelectorList(CSSStyleRule* rule)
{
    RefPtr<CSSRuleSourceData> sourceData;
    if (ensureParsedDataReady())
        sourceData = ruleSourceDataFor(rule->style());
    RefPtr<TypeBuilder::Array<String> > selectors;

    // This intentionally does not rely on the source data to avoid catching the trailing comments (before the declaration starting '{').
    String selectorText = rule->selectorText();

    if (sourceData)
        selectors = selectorsFromSource(sourceData.get(), m_parsedStyleSheet->text());
    else {
        selectors = TypeBuilder::Array<String>::create();
        const CSSSelectorList& selectorList = rule->styleRule()->selectorList();
        for (const CSSSelector* selector = selectorList.first(); selector; selector = CSSSelectorList::next(selector))
            selectors->addItem(selector->selectorText());
    }
    RefPtr<TypeBuilder::CSS::SelectorList> result = TypeBuilder::CSS::SelectorList::create()
        .setSelectors(selectors)
        .setText(selectorText)
        .release();
    if (sourceData)
        result->setRange(buildSourceRangeObject(sourceData->ruleHeaderRange, lineEndings().get()));
    return result.release();
}

PassRefPtr<TypeBuilder::CSS::CSSRule> InspectorStyleSheet::buildObjectForRule(CSSStyleRule* rule, PassRefPtr<Array<TypeBuilder::CSS::CSSMedia> > mediaStack)
{
    CSSStyleSheet* styleSheet = pageStyleSheet();
    if (!styleSheet)
        return 0;

    RefPtr<TypeBuilder::CSS::CSSRule> result = TypeBuilder::CSS::CSSRule::create()
        .setSelectorList(buildObjectForSelectorList(rule))
        .setOrigin(m_origin)
        .setStyle(buildObjectForStyle(rule->style()));

    String url = this->url();
    if (!url.isEmpty())
        result->setSourceURL(url);

    if (canBind()) {
        InspectorCSSId id(ruleId(rule));
        if (!id.isEmpty())
            result->setRuleId(id.asProtocolValue<TypeBuilder::CSS::CSSRuleId>());
    }

    RefPtr<Array<TypeBuilder::CSS::CSSMedia> > mediaArray = Array<TypeBuilder::CSS::CSSMedia>::create();

    if (mediaStack)
        result->setMedia(mediaStack);

    return result.release();
}

PassRefPtr<TypeBuilder::CSS::CSSStyle> InspectorStyleSheet::buildObjectForStyle(CSSStyleDeclaration* style)
{
    RefPtr<CSSRuleSourceData> sourceData;
    if (ensureParsedDataReady())
        sourceData = ruleSourceDataFor(style);

    InspectorCSSId id = ruleOrStyleId(style);
    if (id.isEmpty()) {
        RefPtr<TypeBuilder::CSS::CSSStyle> bogusStyle = TypeBuilder::CSS::CSSStyle::create()
            .setCssProperties(Array<TypeBuilder::CSS::CSSProperty>::create())
            .setShorthandEntries(Array<TypeBuilder::CSS::ShorthandEntry>::create());
        return bogusStyle.release();
    }
    RefPtr<InspectorStyle> inspectorStyle = inspectorStyleForId(id);
    RefPtr<TypeBuilder::CSS::CSSStyle> result = inspectorStyle->buildObjectForStyle();

    // Style text cannot be retrieved without stylesheet, so set cssText here.
    if (sourceData) {
        String sheetText;
        bool success = getText(&sheetText);
        if (success) {
            const SourceRange& bodyRange = sourceData->ruleBodyRange;
            result->setCssText(sheetText.substring(bodyRange.start, bodyRange.end - bodyRange.start));
        }
    }

    return result.release();
}

bool InspectorStyleSheet::setStyleText(const InspectorCSSId& id, const String& text, String* oldText, ExceptionState& es)
{
    RefPtr<InspectorStyle> inspectorStyle = inspectorStyleForId(id);
    if (!inspectorStyle || !inspectorStyle->cssStyle()) {
        es.throwDOMException(NotFoundError);
        return false;
    }

    bool success = inspectorStyle->styleText(oldText);
    if (!success) {
        es.throwDOMException(NotFoundError);
        return false;
    }

    success = setStyleText(inspectorStyle->cssStyle(), text);
    if (success)
        fireStyleSheetChanged();
    else
        es.throwDOMException(SyntaxError);
    return success;
}

bool InspectorStyleSheet::setPropertyText(const InspectorCSSId& id, unsigned propertyIndex, const String& text, bool overwrite, String* oldText, ExceptionState& es)
{
    RefPtr<InspectorStyle> inspectorStyle = inspectorStyleForId(id);
    if (!inspectorStyle) {
        es.throwDOMException(NotFoundError);
        return false;
    }

    bool success = inspectorStyle->setPropertyText(propertyIndex, text, overwrite, oldText, es);
    if (success)
        fireStyleSheetChanged();
    return success;
}

bool InspectorStyleSheet::toggleProperty(const InspectorCSSId& id, unsigned propertyIndex, bool disable, ExceptionState& es)
{
    RefPtr<InspectorStyle> inspectorStyle = inspectorStyleForId(id);
    if (!inspectorStyle) {
        es.throwDOMException(NotFoundError);
        return false;
    }

    bool success = inspectorStyle->toggleProperty(propertyIndex, disable, es);
    if (success)
        fireStyleSheetChanged();
    return success;
}

bool InspectorStyleSheet::getText(String* result) const
{
    if (!ensureText())
        return false;
    *result = m_parsedStyleSheet->text();
    return true;
}

CSSStyleDeclaration* InspectorStyleSheet::styleForId(const InspectorCSSId& id) const
{
    CSSStyleRule* rule = ruleForId(id);
    if (!rule)
        return 0;

    return rule->style();
}

void InspectorStyleSheet::fireStyleSheetChanged()
{
    if (m_listener)
        m_listener->styleSheetChanged(this);
}

PassRefPtr<TypeBuilder::CSS::SourceRange> InspectorStyleSheet::ruleHeaderSourceRange(const CSSRule* rule)
{
    if (!ensureParsedDataReady())
        return 0;

    RefPtr<CSSRuleSourceData> sourceData = m_parsedStyleSheet->ruleSourceDataAt(ruleIndexByRule(rule));
    if (!sourceData)
        return 0;
    return buildSourceRangeObject(sourceData->ruleHeaderRange, lineEndings().get());
}

PassRefPtr<InspectorStyle> InspectorStyleSheet::inspectorStyleForId(const InspectorCSSId& id)
{
    CSSStyleDeclaration* style = styleForId(id);
    if (!style)
        return 0;

    return InspectorStyle::create(id, style, this);
}

String InspectorStyleSheet::sourceURL() const
{
    if (!m_sourceURL.isNull())
        return m_sourceURL;
    if (m_origin != TypeBuilder::CSS::StyleSheetOrigin::Regular) {
        m_sourceURL = "";
        return m_sourceURL;
    }

    String styleSheetText;
    bool success = getText(&styleSheetText);
    if (success) {
        bool deprecated;
        String commentValue = ContentSearchUtils::findSourceURL(styleSheetText, ContentSearchUtils::CSSMagicComment, &deprecated);
        if (!commentValue.isEmpty()) {
            // FIXME: add deprecated console message here.
            m_sourceURL = commentValue;
            return commentValue;
        }
    }
    m_sourceURL = "";
    return m_sourceURL;
}

String InspectorStyleSheet::url() const
{
    // "sourceURL" is present only for regular rules, otherwise "origin" should be used in the frontend.
    if (m_origin != TypeBuilder::CSS::StyleSheetOrigin::Regular)
        return String();

    CSSStyleSheet* styleSheet = pageStyleSheet();
    if (!styleSheet)
        return String();

    if (hasSourceURL())
        return sourceURL();

    if (styleSheet->isInline() && startsAtZero())
        return String();

    return finalURL();
}

bool InspectorStyleSheet::hasSourceURL() const
{
    return !sourceURL().isEmpty();
}

bool InspectorStyleSheet::startsAtZero() const
{
    CSSStyleSheet* styleSheet = pageStyleSheet();
    if (!styleSheet)
        return true;

    return styleSheet->startPositionInSource() == TextPosition::minimumPosition();
}

String InspectorStyleSheet::sourceMapURL() const
{
    if (m_origin != TypeBuilder::CSS::StyleSheetOrigin::Regular)
        return String();

    String styleSheetText;
    bool success = getText(&styleSheetText);
    if (success) {
        bool deprecated;
        String commentValue = ContentSearchUtils::findSourceMapURL(styleSheetText, ContentSearchUtils::CSSMagicComment, &deprecated);
        if (!commentValue.isEmpty()) {
            // FIXME: add deprecated console message here.
            return commentValue;
        }
    }
    return m_pageAgent->resourceSourceMapURL(finalURL());
}

InspectorCSSId InspectorStyleSheet::ruleOrStyleId(CSSStyleDeclaration* style) const
{
    unsigned index = ruleIndexByStyle(style);
    if (index != UINT_MAX)
        return InspectorCSSId(id(), index);
    return InspectorCSSId();
}

Document* InspectorStyleSheet::ownerDocument() const
{
    return m_pageStyleSheet->ownerDocument();
}

PassRefPtr<CSSRuleSourceData> InspectorStyleSheet::ruleSourceDataFor(CSSStyleDeclaration* style) const
{
    return m_parsedStyleSheet->ruleSourceDataAt(ruleIndexByStyle(style));
}

PassOwnPtr<Vector<unsigned> > InspectorStyleSheet::lineEndings() const
{
    if (!m_parsedStyleSheet->hasText())
        return PassOwnPtr<Vector<unsigned> >();
    return WTF::lineEndings(m_parsedStyleSheet->text());
}

unsigned InspectorStyleSheet::ruleIndexByStyle(CSSStyleDeclaration* pageStyle) const
{
    ensureFlatRules();
    for (unsigned i = 0, size = m_flatRules.size(); i < size; ++i) {
        CSSStyleRule* styleRule = InspectorCSSAgent::asCSSStyleRule(m_flatRules.at(i).get());
        if (styleRule && styleRule->style() == pageStyle)
            return i;
    }
    return UINT_MAX;
}

unsigned InspectorStyleSheet::ruleIndexByRule(const CSSRule* rule) const
{
    ensureFlatRules();
    size_t index = m_flatRules.find(rule);
    return index == kNotFound ? UINT_MAX : static_cast<unsigned>(index);
}

bool InspectorStyleSheet::checkPageStyleSheet(ExceptionState& es) const
{
    if (!m_pageStyleSheet) {
        es.throwDOMException(NotSupportedError);
        return false;
    }
    return true;
}

bool InspectorStyleSheet::ensureParsedDataReady()
{
    return ensureText() && ensureSourceData();
}

bool InspectorStyleSheet::ensureText() const
{
    if (!m_parsedStyleSheet)
        return false;
    if (m_parsedStyleSheet->hasText())
        return true;

    String text;
    bool success = originalStyleSheetText(&text);
    if (success)
        m_parsedStyleSheet->setText(text);
    // No need to clear m_flatRules here - it's empty.

    return success;
}

bool InspectorStyleSheet::ensureSourceData()
{
    if (m_parsedStyleSheet->hasSourceData())
        return true;

    if (!m_parsedStyleSheet->hasText())
        return false;

    RefPtr<StyleSheetContents> newStyleSheet = StyleSheetContents::create();
    OwnPtr<RuleSourceDataList> result = adoptPtr(new RuleSourceDataList());
    StyleSheetHandler handler(m_parsedStyleSheet->text(), m_pageStyleSheet->ownerDocument(), newStyleSheet.get(), result.get());
    createCSSParser(m_pageStyleSheet->ownerDocument())->parseSheet(newStyleSheet.get(), m_parsedStyleSheet->text(), TextPosition::minimumPosition(), &handler);
    m_parsedStyleSheet->setSourceData(result.release());
    return m_parsedStyleSheet->hasSourceData();
}

void InspectorStyleSheet::ensureFlatRules() const
{
    // We are fine with redoing this for empty stylesheets as this will run fast.
    if (m_flatRules.isEmpty())
        collectFlatRules(asCSSRuleList(pageStyleSheet()), &m_flatRules);
}

bool InspectorStyleSheet::setStyleText(CSSStyleDeclaration* style, const String& text)
{
    if (!m_pageStyleSheet)
        return false;
    if (!ensureParsedDataReady())
        return false;

    String patchedStyleSheetText;
    bool success = styleSheetTextWithChangedStyle(style, text, &patchedStyleSheetText);
    if (!success)
        return false;

    InspectorCSSId id = ruleOrStyleId(style);
    if (id.isEmpty())
        return false;

    TrackExceptionState es;
    style->setCssText(text, es);
    if (!es.hadException())
        m_parsedStyleSheet->setText(patchedStyleSheetText);

    return !es.hadException();
}

bool InspectorStyleSheet::styleSheetTextWithChangedStyle(CSSStyleDeclaration* style, const String& newStyleText, String* result)
{
    if (!style)
        return false;

    if (!ensureParsedDataReady())
        return false;

    RefPtr<CSSRuleSourceData> sourceData = ruleSourceDataFor(style);
    unsigned bodyStart = sourceData->ruleBodyRange.start;
    unsigned bodyEnd = sourceData->ruleBodyRange.end;
    ASSERT(bodyStart <= bodyEnd);

    String text = m_parsedStyleSheet->text();
    ASSERT_WITH_SECURITY_IMPLICATION(bodyEnd <= text.length()); // bodyEnd is exclusive

    text.replace(bodyStart, bodyEnd - bodyStart, newStyleText);
    *result = text;
    return true;
}

InspectorCSSId InspectorStyleSheet::ruleId(CSSStyleRule* rule) const
{
    return ruleOrStyleId(rule->style());
}

void InspectorStyleSheet::revalidateStyle(CSSStyleDeclaration* pageStyle)
{
    if (m_isRevalidating)
        return;

    m_isRevalidating = true;
    ensureFlatRules();
    for (unsigned i = 0, size = m_flatRules.size(); i < size; ++i) {
        CSSStyleRule* parsedRule = InspectorCSSAgent::asCSSStyleRule(m_flatRules.at(i).get());
        if (parsedRule && parsedRule->style() == pageStyle) {
            if (parsedRule->styleRule()->properties()->asText() != pageStyle->cssText())
                setStyleText(pageStyle, pageStyle->cssText());
            break;
        }
    }
    m_isRevalidating = false;
}

bool InspectorStyleSheet::originalStyleSheetText(String* result) const
{
    bool success = inlineStyleSheetText(result);
    if (!success)
        success = resourceStyleSheetText(result);
    return success;
}

bool InspectorStyleSheet::resourceStyleSheetText(String* result) const
{
    if (m_origin == TypeBuilder::CSS::StyleSheetOrigin::User || m_origin == TypeBuilder::CSS::StyleSheetOrigin::User_agent)
        return false;

    if (!m_pageStyleSheet || !ownerDocument() || !ownerDocument()->frame())
        return false;

    String error;
    bool base64Encoded;
    InspectorPageAgent::resourceContent(&error, ownerDocument()->frame(), KURL(ParsedURLString, m_pageStyleSheet->href()), result, &base64Encoded);
    return error.isEmpty() && !base64Encoded;
}

bool InspectorStyleSheet::inlineStyleSheetText(String* result) const
{
    if (!m_pageStyleSheet)
        return false;

    Node* ownerNode = m_pageStyleSheet->ownerNode();
    if (!ownerNode || ownerNode->nodeType() != Node::ELEMENT_NODE)
        return false;
    Element* ownerElement = toElement(ownerNode);

    if (!ownerElement->hasTagName(HTMLNames::styleTag) && !ownerElement->hasTagName(SVGNames::styleTag))
        return false;
    *result = ownerElement->textContent();
    return true;
}

PassRefPtr<InspectorStyleSheetForInlineStyle> InspectorStyleSheetForInlineStyle::create(InspectorPageAgent* pageAgent, const String& id, PassRefPtr<Element> element, TypeBuilder::CSS::StyleSheetOrigin::Enum origin, Listener* listener)
{
    return adoptRef(new InspectorStyleSheetForInlineStyle(pageAgent, id, element, origin, listener));
}

InspectorStyleSheetForInlineStyle::InspectorStyleSheetForInlineStyle(InspectorPageAgent* pageAgent, const String& id, PassRefPtr<Element> element, TypeBuilder::CSS::StyleSheetOrigin::Enum origin, Listener* listener)
    : InspectorStyleSheet(pageAgent, id, 0, origin, "", listener)
    , m_element(element)
    , m_ruleSourceData(0)
    , m_isStyleTextValid(false)
{
    ASSERT(m_element);
    m_inspectorStyle = InspectorStyle::create(InspectorCSSId(id, 0), inlineStyle(), this);
    m_styleText = m_element->isStyledElement() ? m_element->getAttribute("style").string() : String();
}

void InspectorStyleSheetForInlineStyle::didModifyElementAttribute()
{
    m_isStyleTextValid = false;
    if (m_element->isStyledElement() && m_element->style() != m_inspectorStyle->cssStyle())
        m_inspectorStyle = InspectorStyle::create(InspectorCSSId(id(), 0), inlineStyle(), this);
    m_ruleSourceData.clear();
}

bool InspectorStyleSheetForInlineStyle::getText(String* result) const
{
    if (!m_isStyleTextValid) {
        m_styleText = elementStyleText();
        m_isStyleTextValid = true;
    }
    *result = m_styleText;
    return true;
}

bool InspectorStyleSheetForInlineStyle::setStyleText(CSSStyleDeclaration* style, const String& text)
{
    ASSERT_UNUSED(style, style == inlineStyle());
    TrackExceptionState es;

    {
        InspectorCSSAgent::InlineStyleOverrideScope overrideScope(m_element->ownerDocument());
        m_element->setAttribute("style", text, es);
    }

    m_styleText = text;
    m_isStyleTextValid = true;
    m_ruleSourceData.clear();
    return !es.hadException();
}

PassOwnPtr<Vector<unsigned> > InspectorStyleSheetForInlineStyle::lineEndings() const
{
    return WTF::lineEndings(elementStyleText());
}

Document* InspectorStyleSheetForInlineStyle::ownerDocument() const
{
    return &m_element->document();
}

bool InspectorStyleSheetForInlineStyle::ensureParsedDataReady()
{
    // The "style" property value can get changed indirectly, e.g. via element.style.borderWidth = "2px".
    const String& currentStyleText = elementStyleText();
    if (m_styleText != currentStyleText) {
        m_ruleSourceData.clear();
        m_styleText = currentStyleText;
        m_isStyleTextValid = true;
    }

    if (m_ruleSourceData)
        return true;

    m_ruleSourceData = getStyleAttributeData();

    bool success = !!m_ruleSourceData;
    if (!success) {
        m_ruleSourceData = CSSRuleSourceData::create(CSSRuleSourceData::STYLE_RULE);
        return false;
    }

    return true;
}

PassRefPtr<InspectorStyle> InspectorStyleSheetForInlineStyle::inspectorStyleForId(const InspectorCSSId& id)
{
    ASSERT_UNUSED(id, !id.ordinal());
    return m_inspectorStyle;
}

CSSStyleDeclaration* InspectorStyleSheetForInlineStyle::inlineStyle() const
{
    return m_element->style();
}

const String& InspectorStyleSheetForInlineStyle::elementStyleText() const
{
    return m_element->getAttribute("style").string();
}

PassRefPtr<CSSRuleSourceData> InspectorStyleSheetForInlineStyle::getStyleAttributeData() const
{
    if (!m_element->isStyledElement())
        return 0;

    if (m_styleText.isEmpty()) {
        RefPtr<CSSRuleSourceData> result = CSSRuleSourceData::create(CSSRuleSourceData::STYLE_RULE);
        result->ruleBodyRange.start = 0;
        result->ruleBodyRange.end = 0;
        return result.release();
    }

    RefPtr<MutableStylePropertySet> tempDeclaration = MutableStylePropertySet::create();
    RuleSourceDataList ruleSourceDataResult;
    StyleSheetHandler handler(m_styleText, &m_element->document(), m_element->document().elementSheet()->contents(), &ruleSourceDataResult);
    createCSSParser(&m_element->document())->parseDeclaration(tempDeclaration.get(), m_styleText, &handler, m_element->document().elementSheet()->contents());
    return ruleSourceDataResult.first().release();
}

} // namespace WebCore

