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

#include "core/css/CSSPropertySourceData.h"
#include "core/css/CSSStyleDeclaration.h"
#include "platform/heap/Handle.h"
#include "platform/inspector_protocol/TypeBuilder.h"
#include "platform/inspector_protocol/Values.h"
#include "wtf/HashMap.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class CSSKeyframeRule;
class CSSMediaRule;
class CSSStyleDeclaration;
class CSSStyleRule;
class CSSStyleSheet;
class Document;
class Element;
class ExceptionState;
class InspectorResourceAgent;
class InspectorResourceContainer;
class InspectorStyleSheetBase;

typedef HeapVector<Member<CSSRule>> CSSRuleVector;
typedef Vector<unsigned> LineEndings;

class InspectorStyle final : public GarbageCollected<InspectorStyle> {
public:
    static InspectorStyle* create(CSSStyleDeclaration*, CSSRuleSourceData*, InspectorStyleSheetBase* parentStyleSheet);

    CSSStyleDeclaration* cssStyle() { return m_style.get(); }
    PassOwnPtr<protocol::CSS::CSSStyle> buildObjectForStyle();
    PassOwnPtr<protocol::Array<protocol::CSS::CSSComputedStyleProperty>> buildArrayForComputedStyle();
    bool styleText(String* result);
    bool textForRange(const SourceRange&, String* result);

    DECLARE_TRACE();

private:
    InspectorStyle(CSSStyleDeclaration*, CSSRuleSourceData*, InspectorStyleSheetBase* parentStyleSheet);

    void populateAllProperties(HeapVector<CSSPropertySourceData>& result);
    PassOwnPtr<protocol::CSS::CSSStyle> styleWithProperties();
    String shorthandValue(const String& shorthandProperty);

    Member<CSSStyleDeclaration> m_style;
    Member<CSSRuleSourceData> m_sourceData;
    Member<InspectorStyleSheetBase> m_parentStyleSheet;
};

class InspectorStyleSheetBase : public GarbageCollectedFinalized<InspectorStyleSheetBase> {
public:
    class CORE_EXPORT Listener {
    public:
        Listener() { }
        virtual ~Listener() { }
        virtual void styleSheetChanged(InspectorStyleSheetBase*) = 0;
        virtual void willReparseStyleSheet() = 0;
        virtual void didReparseStyleSheet() = 0;
    };
    virtual ~InspectorStyleSheetBase() { }
    DEFINE_INLINE_VIRTUAL_TRACE() { }

    String id() { return m_id; }

    virtual bool setText(const String&, ExceptionState&) = 0;
    virtual bool getText(String* result) = 0;
    virtual String sourceMapURL() { return String(); }

    PassOwnPtr<protocol::CSS::CSSStyle> buildObjectForStyle(CSSStyleDeclaration*);
    PassOwnPtr<protocol::CSS::SourceRange> buildSourceRangeObject(const SourceRange&);
    bool lineNumberAndColumnToOffset(unsigned lineNumber, unsigned columnNumber, unsigned* offset);
    virtual bool isInlineStyle() = 0;

protected:
    explicit InspectorStyleSheetBase(Listener*);

    Listener* listener() { return m_listener; }
    void onStyleSheetTextChanged();
    const LineEndings* lineEndings();

    virtual InspectorStyle* inspectorStyle(CSSStyleDeclaration*) = 0;

private:
    friend class InspectorStyle;

    String m_id;
    Listener* m_listener;
    OwnPtr<LineEndings> m_lineEndings;
};

class InspectorStyleSheet : public InspectorStyleSheetBase {
public:
    static InspectorStyleSheet* create(InspectorResourceAgent*, CSSStyleSheet* pageStyleSheet, const String& origin, const String& documentURL, InspectorStyleSheetBase::Listener*, InspectorResourceContainer*);

    ~InspectorStyleSheet() override;
    DECLARE_VIRTUAL_TRACE();

    String finalURL();
    bool setText(const String&, ExceptionState&) override;
    bool getText(String* result) override;
    CSSStyleRule* setRuleSelector(const SourceRange&, const String& selector, SourceRange* newRange, String* oldSelector, ExceptionState&);
    CSSKeyframeRule* setKeyframeKey(const SourceRange&, const String& text, SourceRange* newRange, String* oldText, ExceptionState&);
    CSSRule* setStyleText(const SourceRange&, const String& text, SourceRange* newRange, String* oldSelector, ExceptionState&);
    CSSMediaRule* setMediaRuleText(const SourceRange&, const String& selector, SourceRange* newRange, String* oldSelector, ExceptionState&);
    CSSStyleRule* addRule(const String& ruleText, const SourceRange& location, SourceRange* addedRange, ExceptionState&);
    bool deleteRule(const SourceRange&, ExceptionState&);

    CSSStyleSheet* pageStyleSheet() { return m_pageStyleSheet.get(); }

    PassOwnPtr<protocol::CSS::CSSStyleSheetHeader> buildObjectForStyleSheetInfo();
    PassOwnPtr<protocol::CSS::CSSRule> buildObjectForRuleWithoutMedia(CSSStyleRule*);
    PassOwnPtr<protocol::CSS::CSSKeyframeRule> buildObjectForKeyframeRule(CSSKeyframeRule*);
    PassOwnPtr<protocol::CSS::SelectorList> buildObjectForSelectorList(CSSStyleRule*);

    PassOwnPtr<protocol::CSS::SourceRange> ruleHeaderSourceRange(CSSRule*);
    PassOwnPtr<protocol::CSS::SourceRange> mediaQueryExpValueSourceRange(CSSRule*, size_t mediaQueryIndex, size_t mediaQueryExpIndex);

    bool isInlineStyle() override { return false; }
    const CSSRuleVector& flatRules();
    CSSRuleSourceData* sourceDataForRule(CSSRule*);
    String sourceMapURL() override;

protected:
    InspectorStyle* inspectorStyle(CSSStyleDeclaration*) override;

private:
    InspectorStyleSheet(InspectorResourceAgent*, CSSStyleSheet* pageStyleSheet, const String& origin, const String& documentURL, InspectorStyleSheetBase::Listener*, InspectorResourceContainer*);
    CSSRuleSourceData* ruleSourceDataAfterSourceRange(const SourceRange&);
    CSSRuleSourceData* findRuleByHeaderRange(const SourceRange&);
    CSSRuleSourceData* findRuleByBodyRange(const SourceRange&);
    CSSRule* ruleForSourceData(CSSRuleSourceData*);
    CSSStyleRule* insertCSSOMRuleInStyleSheet(CSSRule* insertBefore, const String& ruleText, ExceptionState&);
    CSSStyleRule* insertCSSOMRuleInMediaRule(CSSMediaRule*, CSSRule* insertBefore, const String& ruleText, ExceptionState&);
    CSSStyleRule* insertCSSOMRuleBySourceRange(const SourceRange&, const String& ruleText, ExceptionState&);
    String sourceURL();
    void remapSourceDataToCSSOMIfNecessary();
    void mapSourceDataToCSSOM();
    bool resourceStyleSheetText(String* result);
    bool inlineStyleSheetText(String* result);
    PassOwnPtr<protocol::Array<protocol::CSS::Value>> selectorsFromSource(CSSRuleSourceData*, const String&);
    String url();
    bool hasSourceURL();
    bool startsAtZero();

    void replaceText(const SourceRange&, const String& text, SourceRange* newRange, String* oldText);
    void innerSetText(const String& newText, bool markAsLocallyModified);
    Element* ownerStyleElement();

    Member<InspectorResourceContainer> m_resourceContainer;
    Member<InspectorResourceAgent> m_resourceAgent;
    Member<CSSStyleSheet> m_pageStyleSheet;
    String m_origin;
    String m_documentURL;
    Member<RuleSourceDataList> m_sourceData;
    String m_text;
    CSSRuleVector m_cssomFlatRules;
    CSSRuleVector m_parsedFlatRules;
    typedef HashMap<unsigned, unsigned, WTF::IntHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> IndexMap;
    IndexMap m_ruleToSourceData;
    IndexMap m_sourceDataToRule;
    String m_sourceURL;
};

class InspectorStyleSheetForInlineStyle final : public InspectorStyleSheetBase {
public:
    static InspectorStyleSheetForInlineStyle* create(Element*, Listener*);

    void didModifyElementAttribute();
    bool setText(const String&, ExceptionState&) override;
    bool getText(String* result) override;
    CSSStyleDeclaration* inlineStyle();
    CSSRuleSourceData* ruleSourceData();

    DECLARE_VIRTUAL_TRACE();

protected:
    InspectorStyle* inspectorStyle(CSSStyleDeclaration*) override;

    // Also accessed by friend class InspectorStyle.
    bool isInlineStyle() override { return true; }

private:
    InspectorStyleSheetForInlineStyle(Element*, Listener*);
    const String& elementStyleText();

    Member<Element> m_element;
    Member<InspectorStyle> m_inspectorStyle;
};

} // namespace blink

#endif // !defined(InspectorStyleSheet_h)
