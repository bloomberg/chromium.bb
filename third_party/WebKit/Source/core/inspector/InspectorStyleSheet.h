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
#include "platform/JSONValues.h"
#include "platform/heap/Handle.h"
#include "platform/inspector_protocol/TypeBuilder.h"
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
class InspectorCSSAgent;
class InspectorResourceAgent;
class InspectorStyleSheetBase;

typedef WillBeHeapVector<RefPtrWillBeMember<CSSRule>> CSSRuleVector;
typedef String ErrorString;
typedef Vector<unsigned> LineEndings;

class InspectorStyle final : public RefCountedWillBeGarbageCollectedFinalized<InspectorStyle> {
public:
    static PassRefPtrWillBeRawPtr<InspectorStyle> create(PassRefPtrWillBeRawPtr<CSSStyleDeclaration>, PassRefPtrWillBeRawPtr<CSSRuleSourceData>, InspectorStyleSheetBase* parentStyleSheet);

    CSSStyleDeclaration* cssStyle() { return m_style.get(); }
    PassRefPtr<protocol::TypeBuilder::CSS::CSSStyle> buildObjectForStyle();
    PassRefPtr<protocol::TypeBuilder::Array<protocol::TypeBuilder::CSS::CSSComputedStyleProperty>> buildArrayForComputedStyle();
    bool styleText(String* result);
    bool textForRange(const SourceRange&, String* result);

    DECLARE_TRACE();

private:
    InspectorStyle(PassRefPtrWillBeRawPtr<CSSStyleDeclaration>, PassRefPtrWillBeRawPtr<CSSRuleSourceData>, InspectorStyleSheetBase* parentStyleSheet);

    void populateAllProperties(WillBeHeapVector<CSSPropertySourceData>& result);
    PassRefPtr<protocol::TypeBuilder::CSS::CSSStyle> styleWithProperties();
    String shorthandValue(const String& shorthandProperty);

    RefPtrWillBeMember<CSSStyleDeclaration> m_style;
    RefPtrWillBeMember<CSSRuleSourceData> m_sourceData;
    RawPtrWillBeMember<InspectorStyleSheetBase> m_parentStyleSheet;
};

class InspectorStyleSheetBase : public RefCountedWillBeGarbageCollectedFinalized<InspectorStyleSheetBase> {
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

    PassRefPtr<protocol::TypeBuilder::CSS::CSSStyle> buildObjectForStyle(CSSStyleDeclaration*);
    PassRefPtr<protocol::TypeBuilder::CSS::SourceRange> buildSourceRangeObject(const SourceRange&);
    bool lineNumberAndColumnToOffset(unsigned lineNumber, unsigned columnNumber, unsigned* offset);
    virtual bool isInlineStyle() = 0;

protected:
    explicit InspectorStyleSheetBase(Listener*);

    Listener* listener() { return m_listener; }
    void onStyleSheetTextChanged();
    const LineEndings* lineEndings();

    virtual PassRefPtrWillBeRawPtr<InspectorStyle> inspectorStyle(RefPtrWillBeRawPtr<CSSStyleDeclaration>) = 0;

private:
    friend class InspectorStyle;

    String m_id;
    Listener* m_listener;
    OwnPtr<LineEndings> m_lineEndings;
};

class InspectorStyleSheet : public InspectorStyleSheetBase {
public:
    static PassRefPtrWillBeRawPtr<InspectorStyleSheet> create(InspectorResourceAgent*, PassRefPtrWillBeRawPtr<CSSStyleSheet> pageStyleSheet, protocol::TypeBuilder::CSS::StyleSheetOrigin::Enum, const String& documentURL, InspectorCSSAgent*);

    ~InspectorStyleSheet() override;
    DECLARE_VIRTUAL_TRACE();

    String finalURL();
    bool setText(const String&, ExceptionState&) override;
    bool getText(String* result) override;
    RefPtrWillBeRawPtr<CSSStyleRule>  setRuleSelector(const SourceRange&, const String& selector, SourceRange* newRange, String* oldSelector, ExceptionState&);
    PassRefPtrWillBeRawPtr<CSSKeyframeRule>  setKeyframeKey(const SourceRange&, const String& text, SourceRange* newRange, String* oldText, ExceptionState&);
    PassRefPtrWillBeRawPtr<CSSRule>  setStyleText(const SourceRange&, const String& text, SourceRange* newRange, String* oldSelector, ExceptionState&);
    RefPtrWillBeRawPtr<CSSMediaRule>  setMediaRuleText(const SourceRange&, const String& selector, SourceRange* newRange, String* oldSelector, ExceptionState&);
    RefPtrWillBeRawPtr<CSSStyleRule>  addRule(const String& ruleText, const SourceRange& location, SourceRange* addedRange, ExceptionState&);
    bool deleteRule(const SourceRange&, ExceptionState&);

    CSSStyleSheet* pageStyleSheet() { return m_pageStyleSheet.get(); }

    PassRefPtr<protocol::TypeBuilder::CSS::CSSStyleSheetHeader> buildObjectForStyleSheetInfo();
    PassRefPtr<protocol::TypeBuilder::CSS::CSSRule> buildObjectForRuleWithoutMedia(CSSStyleRule*);
    PassRefPtr<protocol::TypeBuilder::CSS::CSSKeyframeRule> buildObjectForKeyframeRule(CSSKeyframeRule*);
    PassRefPtr<protocol::TypeBuilder::CSS::SelectorList> buildObjectForSelectorList(CSSStyleRule*);

    PassRefPtr<protocol::TypeBuilder::CSS::SourceRange> ruleHeaderSourceRange(CSSRule*);
    PassRefPtr<protocol::TypeBuilder::CSS::SourceRange> mediaQueryExpValueSourceRange(CSSRule*, size_t mediaQueryIndex, size_t mediaQueryExpIndex);

    bool isInlineStyle() override { return false; }
    const CSSRuleVector& flatRules();
    RefPtrWillBeRawPtr<CSSRuleSourceData> sourceDataForRule(RefPtrWillBeRawPtr<CSSRule>);
    String sourceMapURL() override;

protected:
    PassRefPtrWillBeRawPtr<InspectorStyle> inspectorStyle(RefPtrWillBeRawPtr<CSSStyleDeclaration>) override;

private:
    InspectorStyleSheet(InspectorResourceAgent*, PassRefPtrWillBeRawPtr<CSSStyleSheet> pageStyleSheet, protocol::TypeBuilder::CSS::StyleSheetOrigin::Enum, const String& documentURL, InspectorCSSAgent*);
    RefPtrWillBeRawPtr<CSSRuleSourceData> ruleSourceDataAfterSourceRange(const SourceRange&);
    RefPtrWillBeRawPtr<CSSRuleSourceData> findRuleByHeaderRange(const SourceRange&);
    RefPtrWillBeRawPtr<CSSRuleSourceData> findRuleByBodyRange(const SourceRange&);
    RefPtrWillBeRawPtr<CSSRule> ruleForSourceData(RefPtrWillBeRawPtr<CSSRuleSourceData>);
    CSSStyleRule* insertCSSOMRuleInStyleSheet(CSSRule* insertBefore, const String& ruleText, ExceptionState&);
    CSSStyleRule* insertCSSOMRuleInMediaRule(CSSMediaRule*, CSSRule* insertBefore, const String& ruleText, ExceptionState&);
    CSSStyleRule* insertCSSOMRuleBySourceRange(const SourceRange&, const String& ruleText, ExceptionState&);
    String sourceURL();
    void remapSourceDataToCSSOMIfNecessary();
    void mapSourceDataToCSSOM();
    bool resourceStyleSheetText(String* result);
    bool inlineStyleSheetText(String* result);
    PassRefPtr<protocol::TypeBuilder::Array<protocol::TypeBuilder::CSS::Value>> selectorsFromSource(CSSRuleSourceData*, const String&);
    String url();
    bool hasSourceURL();
    bool startsAtZero();

    void replaceText(const SourceRange&, const String& text, SourceRange* newRange, String* oldText);
    void innerSetText(const String& newText, bool markAsLocallyModified);
    Element* ownerStyleElement();

    RawPtrWillBeMember<InspectorCSSAgent> m_cssAgent;
    RawPtrWillBeMember<InspectorResourceAgent> m_resourceAgent;
    RefPtrWillBeMember<CSSStyleSheet> m_pageStyleSheet;
    protocol::TypeBuilder::CSS::StyleSheetOrigin::Enum m_origin;
    String m_documentURL;
    OwnPtrWillBeMember<RuleSourceDataList> m_sourceData;
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
    static PassRefPtrWillBeRawPtr<InspectorStyleSheetForInlineStyle> create(PassRefPtrWillBeRawPtr<Element>, Listener*);

    void didModifyElementAttribute();
    bool setText(const String&, ExceptionState&) override;
    bool getText(String* result) override;
    CSSStyleDeclaration* inlineStyle();
    RefPtrWillBeRawPtr<CSSRuleSourceData> ruleSourceData();

    DECLARE_VIRTUAL_TRACE();

protected:
    PassRefPtrWillBeRawPtr<InspectorStyle> inspectorStyle(RefPtrWillBeRawPtr<CSSStyleDeclaration>) override;

    // Also accessed by friend class InspectorStyle.
    bool isInlineStyle() override { return true; }

private:
    InspectorStyleSheetForInlineStyle(PassRefPtrWillBeRawPtr<Element>, Listener*);
    const String& elementStyleText();

    RefPtrWillBeMember<Element> m_element;
    RefPtrWillBeMember<InspectorStyle> m_inspectorStyle;
};

} // namespace blink

#endif // !defined(InspectorStyleSheet_h)
