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

#include "core/InspectorTypeBuilder.h"
#include "core/css/CSSPropertySourceData.h"
#include "core/css/CSSStyleDeclaration.h"
#include "platform/JSONValues.h"
#include "platform/heap/Handle.h"
#include "wtf/HashMap.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

class ParsedStyleSheet;

namespace blink {

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

typedef WillBeHeapVector<RefPtrWillBeMember<CSSRule> > CSSRuleVector;
typedef String ErrorString;
typedef Vector<unsigned> LineEndings;

class InspectorStyle final : public RefCountedWillBeGarbageCollectedFinalized<InspectorStyle> {
public:
    static PassRefPtrWillBeRawPtr<InspectorStyle> create(PassRefPtrWillBeRawPtr<CSSStyleDeclaration>, PassRefPtrWillBeRawPtr<CSSRuleSourceData>, InspectorStyleSheetBase* parentStyleSheet);

    CSSStyleDeclaration* cssStyle() { return m_style.get(); }
    PassRefPtr<TypeBuilder::CSS::CSSStyle> buildObjectForStyle();
    PassRefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSComputedStyleProperty>> buildArrayForComputedStyle();
    bool styleText(String* result);
    bool textForRange(const SourceRange&, String* result);

    DECLARE_TRACE();

private:
    InspectorStyle(PassRefPtrWillBeRawPtr<CSSStyleDeclaration>, PassRefPtrWillBeRawPtr<CSSRuleSourceData>, InspectorStyleSheetBase* parentStyleSheet);

    void populateAllProperties(WillBeHeapVector<CSSPropertySourceData>& result);
    PassRefPtr<TypeBuilder::CSS::CSSStyle> styleWithProperties();
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

    PassRefPtr<TypeBuilder::CSS::CSSStyle> buildObjectForStyle(CSSStyleDeclaration*);
    bool lineNumberAndColumnToOffset(unsigned lineNumber, unsigned columnNumber, unsigned* offset);
    virtual bool isInlineStyle() = 0;

protected:
    InspectorStyleSheetBase(const String& id, Listener*);

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
    static PassRefPtrWillBeRawPtr<InspectorStyleSheet> create(InspectorResourceAgent*, const String& id, PassRefPtrWillBeRawPtr<CSSStyleSheet> pageStyleSheet, TypeBuilder::CSS::StyleSheetOrigin::Enum, const String& documentURL, InspectorCSSAgent*);

    virtual ~InspectorStyleSheet();
    DECLARE_VIRTUAL_TRACE();

    String finalURL();
    virtual bool setText(const String&, ExceptionState&) override;
    virtual bool getText(String* result) override;
    RefPtrWillBeRawPtr<CSSStyleRule>  setRuleSelector(const SourceRange&, const String& selector, SourceRange* newRange, String* oldSelector, ExceptionState&);
    RefPtrWillBeRawPtr<CSSStyleRule>  setStyleText(const SourceRange&, const String& text, SourceRange* newRange, String* oldSelector, ExceptionState&);
    RefPtrWillBeRawPtr<CSSMediaRule>  setMediaRuleText(const SourceRange&, const String& selector, SourceRange* newRange, String* oldSelector, ExceptionState&);
    RefPtrWillBeRawPtr<CSSStyleRule>  addRule(const String& ruleText, const SourceRange& location, SourceRange* addedRange, ExceptionState&);
    bool deleteRule(const SourceRange&, ExceptionState&);

    CSSStyleSheet* pageStyleSheet() { return m_pageStyleSheet.get(); }

    PassRefPtr<TypeBuilder::CSS::CSSStyleSheetHeader> buildObjectForStyleSheetInfo();
    PassRefPtr<TypeBuilder::CSS::CSSRule> buildObjectForRule(CSSStyleRule*, PassRefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSMedia> >);

    PassRefPtr<TypeBuilder::CSS::SourceRange> ruleHeaderSourceRange(const CSSRule*);
    PassRefPtr<TypeBuilder::CSS::SourceRange> mediaQueryExpValueSourceRange(const CSSRule*, size_t mediaQueryIndex, size_t mediaQueryExpIndex);

    bool isInlineStyle() override { return false; }
    const CSSRuleVector& flatRules();

protected:
    virtual PassRefPtrWillBeRawPtr<InspectorStyle> inspectorStyle(RefPtrWillBeRawPtr<CSSStyleDeclaration>) override;

private:
    InspectorStyleSheet(InspectorResourceAgent*, const String& id, PassRefPtrWillBeRawPtr<CSSStyleSheet> pageStyleSheet, TypeBuilder::CSS::StyleSheetOrigin::Enum, const String& documentURL, InspectorCSSAgent*);
    unsigned ruleIndexBySourceRange(const CSSMediaRule* parentMediaRule, const SourceRange&);
    bool findRuleByHeaderRange(const SourceRange&, CSSRule**, CSSRuleSourceData**);
    bool findRuleByBodyRange(const SourceRange&, CSSRule**, CSSRuleSourceData**);
    CSSStyleRule* insertCSSOMRuleInStyleSheet(const SourceRange&, const String& ruleText, ExceptionState&);
    CSSStyleRule* insertCSSOMRuleInMediaRule(CSSMediaRule*, const SourceRange&, const String& ruleText, ExceptionState&);
    CSSStyleRule* insertCSSOMRuleBySourceRange(const SourceRange&, const String& ruleText, ExceptionState&);
    String sourceMapURL();
    String sourceURL();
    bool ensureText();
    void ensureFlatRules();
    bool originalStyleSheetText(String* result);
    bool resourceStyleSheetText(String* result);
    bool inlineStyleSheetText(String* result);
    PassRefPtr<TypeBuilder::Array<TypeBuilder::CSS::Selector> > selectorsFromSource(const CSSRuleSourceData*, const String&);
    PassRefPtr<TypeBuilder::CSS::SelectorList> buildObjectForSelectorList(CSSStyleRule*);
    String url();
    bool hasSourceURL();
    bool startsAtZero();

    unsigned indexOf(CSSStyleDeclaration*);
    bool ensureParsedDataReady();
    void replaceText(const SourceRange&, const String& text, SourceRange* newRange, String* oldText);
    void innerSetText(const String& newText);
    Element* ownerStyleElement();

    RawPtrWillBeMember<InspectorCSSAgent> m_cssAgent;
    RawPtrWillBeMember<InspectorResourceAgent> m_resourceAgent;
    RefPtrWillBeMember<CSSStyleSheet> m_pageStyleSheet;
    TypeBuilder::CSS::StyleSheetOrigin::Enum m_origin;
    String m_documentURL;
    OwnPtrWillBeMember<ParsedStyleSheet> m_parsedStyleSheet;
    CSSRuleVector m_flatRules;
    String m_sourceURL;
};

class InspectorStyleSheetForInlineStyle final : public InspectorStyleSheetBase {
public:
    static PassRefPtrWillBeRawPtr<InspectorStyleSheetForInlineStyle> create(const String& id, PassRefPtrWillBeRawPtr<Element>, Listener*);

    void didModifyElementAttribute();
    virtual bool setText(const String&, ExceptionState&) override;
    virtual bool getText(String* result) override;
    CSSStyleDeclaration* inlineStyle();

    DECLARE_VIRTUAL_TRACE();

protected:
    virtual PassRefPtrWillBeRawPtr<InspectorStyle> inspectorStyle(RefPtrWillBeRawPtr<CSSStyleDeclaration>) override;

    // Also accessed by friend class InspectorStyle.
    bool isInlineStyle() override { return true; }

private:
    InspectorStyleSheetForInlineStyle(const String& id, PassRefPtrWillBeRawPtr<Element>, Listener*);
    const String& elementStyleText();

    RefPtrWillBeMember<Element> m_element;
    RefPtrWillBeMember<InspectorStyle> m_inspectorStyle;
};

} // namespace blink

#endif // !defined(InspectorStyleSheet_h)
