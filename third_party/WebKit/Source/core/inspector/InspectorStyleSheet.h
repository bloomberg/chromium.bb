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

#ifndef InspectorStyleSheet_h
#define InspectorStyleSheet_h

#include "InspectorTypeBuilder.h"
#include "core/css/CSSPropertySourceData.h"
#include "core/css/CSSStyleDeclaration.h"
#include "core/inspector/InspectorStyleTextEditor.h"
#include "core/platform/JSONValues.h"
#include "wtf/HashMap.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

class ParsedStyleSheet;

namespace WebCore {

class CSSRuleList;
class CSSStyleDeclaration;
class CSSStyleRule;
class CSSStyleSheet;
class Document;
class Element;
class ExceptionState;
class InspectorPageAgent;
class InspectorResourceAgent;
class InspectorStyleSheet;

typedef Vector<RefPtr<CSSRule> > CSSRuleVector;
typedef String ErrorString;

class InspectorCSSId {
public:
    InspectorCSSId()
        : m_ordinal(0)
    {
    }

    explicit InspectorCSSId(PassRefPtr<JSONObject> value)
    {
        if (!value->getString("styleSheetId", &m_styleSheetId))
            return;

        RefPtr<JSONValue> ordinalValue = value->get("ordinal");
        if (!ordinalValue || !ordinalValue->asNumber(&m_ordinal))
            m_styleSheetId = "";
    }

    InspectorCSSId(const String& styleSheetId, unsigned ordinal)
        : m_styleSheetId(styleSheetId)
        , m_ordinal(ordinal)
    {
    }

    bool isEmpty() const { return m_styleSheetId.isEmpty(); }

    const String& styleSheetId() const { return m_styleSheetId; }
    unsigned ordinal() const { return m_ordinal; }

    // ID type is either TypeBuilder::CSS::CSSStyleId or TypeBuilder::CSS::CSSRuleId.
    template<typename ID>
    PassRefPtr<ID> asProtocolValue() const
    {
        if (isEmpty())
            return 0;

        RefPtr<ID> result = ID::create()
            .setStyleSheetId(m_styleSheetId)
            .setOrdinal(m_ordinal);
        return result.release();
    }

private:
    String m_styleSheetId;
    unsigned m_ordinal;
};

struct InspectorStyleProperty {
    explicit InspectorStyleProperty(CSSPropertySourceData sourceData)
        : sourceData(sourceData)
        , hasSource(true)
    {
    }

    InspectorStyleProperty(CSSPropertySourceData sourceData, bool hasSource)
        : sourceData(sourceData)
        , hasSource(hasSource)
    {
    }

    void setRawTextFromStyleDeclaration(const String& styleDeclaration)
    {
        unsigned start = sourceData.range.start;
        unsigned end = sourceData.range.end;
        ASSERT(start < end);
        ASSERT(end <= styleDeclaration.length());
        rawText = styleDeclaration.substring(start, end - start);
    }

    bool hasRawText() const { return !rawText.isEmpty(); }

    CSSPropertySourceData sourceData;
    bool hasSource;
    String rawText;
};

class InspectorStyle : public RefCounted<InspectorStyle> {
public:
    static PassRefPtr<InspectorStyle> create(const InspectorCSSId& styleId, PassRefPtr<CSSStyleDeclaration> style, InspectorStyleSheet* parentStyleSheet);
    virtual ~InspectorStyle();

    CSSStyleDeclaration* cssStyle() const { return m_style.get(); }
    PassRefPtr<TypeBuilder::CSS::CSSStyle> buildObjectForStyle() const;
    PassRefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSComputedStyleProperty> > buildArrayForComputedStyle() const;
    bool setPropertyText(unsigned index, const String& text, bool overwrite, String* oldText, ExceptionState&);
    bool toggleProperty(unsigned index, bool disable, ExceptionState&);
    bool styleText(String* result) const;

private:
    InspectorStyle(const InspectorCSSId& styleId, PassRefPtr<CSSStyleDeclaration> style, InspectorStyleSheet* parentStyleSheet);

    void populateAllProperties(Vector<InspectorStyleProperty>& result) const;
    PassRefPtr<TypeBuilder::CSS::CSSStyle> styleWithProperties() const;
    PassRefPtr<CSSRuleSourceData> extractSourceData() const;
    bool applyStyleText(const String&);
    String shorthandValue(const String& shorthandProperty) const;
    String shorthandPriority(const String& shorthandProperty) const;
    Vector<String> longhandProperties(const String& shorthandProperty) const;
    NewLineAndWhitespace& newLineAndWhitespaceDelimiters() const;
    inline Document* ownerDocument() const;

    InspectorCSSId m_styleId;
    RefPtr<CSSStyleDeclaration> m_style;
    InspectorStyleSheet* m_parentStyleSheet;
    mutable std::pair<String, String> m_format;
    mutable bool m_formatAcquired;
};

class InspectorStyleSheet : public RefCounted<InspectorStyleSheet> {
public:
    class Listener {
    public:
        Listener() { }
        virtual ~Listener() { }
        virtual void styleSheetChanged(InspectorStyleSheet*) = 0;
        virtual void willReparseStyleSheet() = 0;
        virtual void didReparseStyleSheet() = 0;
    };

    typedef HashMap<CSSStyleDeclaration*, RefPtr<InspectorStyle> > InspectorStyleMap;
    static PassRefPtr<InspectorStyleSheet> create(InspectorPageAgent*, InspectorResourceAgent*, const String& id, PassRefPtr<CSSStyleSheet> pageStyleSheet, TypeBuilder::CSS::StyleSheetOrigin::Enum, const String& documentURL, Listener*);
    static String styleSheetURL(CSSStyleSheet* pageStyleSheet);
    static void collectFlatRules(PassRefPtr<CSSRuleList>, CSSRuleVector* result);

    virtual ~InspectorStyleSheet();

    String id() const { return m_id; }
    String finalURL() const;
    virtual Document* ownerDocument() const;
    bool canBind() const { return m_origin != TypeBuilder::CSS::StyleSheetOrigin::User_agent && m_origin != TypeBuilder::CSS::StyleSheetOrigin::User; }
    CSSStyleSheet* pageStyleSheet() const { return m_pageStyleSheet.get(); }
    bool isReparsing() const { return m_isReparsing; }
    void reparseStyleSheet(const String&);
    bool setText(const String&, ExceptionState&);
    String ruleSelector(const InspectorCSSId&, ExceptionState&);
    bool setRuleSelector(const InspectorCSSId&, const String& selector, ExceptionState&);
    CSSStyleRule* addRule(const String& selector, ExceptionState&);
    bool deleteRule(const InspectorCSSId&, ExceptionState&);
    CSSStyleRule* ruleForId(const InspectorCSSId&) const;
    bool fillObjectForStyleSheet(PassRefPtr<TypeBuilder::CSS::CSSStyleSheetBody>);
    PassRefPtr<TypeBuilder::CSS::CSSStyleSheetHeader> buildObjectForStyleSheetInfo() const;
    PassRefPtr<TypeBuilder::CSS::CSSRule> buildObjectForRule(CSSStyleRule*, PassRefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSMedia> >);
    PassRefPtr<TypeBuilder::CSS::CSSStyle> buildObjectForStyle(CSSStyleDeclaration*);
    bool setStyleText(const InspectorCSSId&, const String& text, String* oldText, ExceptionState&);
    bool setPropertyText(const InspectorCSSId&, unsigned propertyIndex, const String& text, bool overwrite, String* oldPropertyText, ExceptionState&);
    bool toggleProperty(const InspectorCSSId&, unsigned propertyIndex, bool disable, ExceptionState&);

    virtual TypeBuilder::CSS::StyleSheetOrigin::Enum origin() const { return m_origin; }
    virtual bool getText(String* result) const;
    virtual CSSStyleDeclaration* styleForId(const InspectorCSSId&) const;
    void fireStyleSheetChanged();
    PassRefPtr<TypeBuilder::CSS::SourceRange> ruleHeaderSourceRange(const CSSRule*);

    InspectorCSSId ruleId(CSSStyleRule*) const;
    InspectorCSSId styleId(CSSStyleDeclaration* style) const { return ruleOrStyleId(style); }

protected:
    InspectorStyleSheet(InspectorPageAgent*, InspectorResourceAgent*, const String& id, PassRefPtr<CSSStyleSheet> pageStyleSheet, TypeBuilder::CSS::StyleSheetOrigin::Enum, const String& documentURL, Listener*);

    InspectorCSSId ruleOrStyleId(CSSStyleDeclaration* style) const;
    virtual PassRefPtr<CSSRuleSourceData> ruleSourceDataFor(CSSStyleDeclaration*) const;
    virtual unsigned ruleIndexByStyle(CSSStyleDeclaration*) const;
    virtual unsigned ruleIndexByRule(const CSSRule*) const;
    virtual bool ensureParsedDataReady();
    virtual PassRefPtr<InspectorStyle> inspectorStyleForId(const InspectorCSSId&);
    virtual String sourceMapURL() const;
    virtual String sourceURL() const;

    // Also accessed by friend class InspectorStyle.
    virtual bool setStyleText(CSSStyleDeclaration*, const String&);
    virtual PassOwnPtr<Vector<unsigned> > lineEndings() const;

private:
    friend class InspectorStyle;

    bool checkPageStyleSheet(ExceptionState&) const;
    bool ensureText() const;
    bool ensureSourceData();
    void ensureFlatRules() const;
    bool styleSheetTextWithChangedStyle(CSSStyleDeclaration*, const String& newStyleText, String* result);
    void revalidateStyle(CSSStyleDeclaration*);
    bool originalStyleSheetText(String* result) const;
    bool resourceStyleSheetText(String* result) const;
    bool inlineStyleSheetText(String* result) const;
    PassRefPtr<TypeBuilder::CSS::SelectorList> buildObjectForSelectorList(CSSStyleRule*);
    String url() const;
    bool hasSourceURL() const;
    bool startsAtZero() const;

    InspectorPageAgent* m_pageAgent;
    InspectorResourceAgent* m_resourceAgent;
    String m_id;
    RefPtr<CSSStyleSheet> m_pageStyleSheet;
    TypeBuilder::CSS::StyleSheetOrigin::Enum m_origin;
    String m_documentURL;
    bool m_isRevalidating;
    bool m_isReparsing;
    ParsedStyleSheet* m_parsedStyleSheet;
    mutable CSSRuleVector m_flatRules;
    Listener* m_listener;
    mutable String m_sourceURL;
};

class InspectorStyleSheetForInlineStyle : public InspectorStyleSheet {
public:
    static PassRefPtr<InspectorStyleSheetForInlineStyle> create(InspectorPageAgent*, InspectorResourceAgent*, const String& id, PassRefPtr<Element>, TypeBuilder::CSS::StyleSheetOrigin::Enum, Listener*);

    void didModifyElementAttribute();
    virtual bool getText(String* result) const;
    virtual CSSStyleDeclaration* styleForId(const InspectorCSSId& id) const { ASSERT_UNUSED(id, !id.ordinal()); return inlineStyle(); }
    virtual TypeBuilder::CSS::StyleSheetOrigin::Enum origin() const { return TypeBuilder::CSS::StyleSheetOrigin::Regular; }

protected:
    InspectorStyleSheetForInlineStyle(InspectorPageAgent*, InspectorResourceAgent*, const String& id, PassRefPtr<Element>, TypeBuilder::CSS::StyleSheetOrigin::Enum, Listener*);

    virtual Document* ownerDocument() const;
    virtual PassRefPtr<CSSRuleSourceData> ruleSourceDataFor(CSSStyleDeclaration* style) const { ASSERT_UNUSED(style, style == inlineStyle()); return m_ruleSourceData; }
    virtual unsigned ruleIndexByStyle(CSSStyleDeclaration*) const { return 0; }
    virtual bool ensureParsedDataReady();
    virtual PassRefPtr<InspectorStyle> inspectorStyleForId(const InspectorCSSId&);
    virtual String sourceMapURL() const OVERRIDE { return String(); }
    virtual String sourceURL() const OVERRIDE { return String(); }

    // Also accessed by friend class InspectorStyle.
    virtual bool setStyleText(CSSStyleDeclaration*, const String&);
    virtual PassOwnPtr<Vector<unsigned> > lineEndings() const;

private:
    CSSStyleDeclaration* inlineStyle() const;
    const String& elementStyleText() const;
    PassRefPtr<CSSRuleSourceData> getStyleAttributeData() const;

    RefPtr<Element> m_element;
    RefPtr<CSSRuleSourceData> m_ruleSourceData;
    RefPtr<InspectorStyle> m_inspectorStyle;

    // Contains "style" attribute value.
    mutable String m_styleText;
    mutable bool m_isStyleTextValid;
};


} // namespace WebCore

#endif // !defined(InspectorStyleSheet_h)
