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

#ifndef InspectorCSSAgent_h
#define InspectorCSSAgent_h

#include "core/css/CSSSelector.h"
#include "core/dom/SecurityContext.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorStyleSheet.h"
#include "core/page/ContentSecurityPolicy.h"
#include "core/platform/JSONValues.h"
#include "wtf/HashMap.h"
#include "wtf/HashSet.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class ChangeRegionOversetTask;
struct CSSParserString;
class CSSRule;
class CSSRuleList;
class CSSStyleRule;
class CSSStyleSheet;
class Document;
class Element;
class InspectorFrontend;
class InspectorResourceAgent;
class InstrumentingAgents;
class MediaList;
class Node;
class NodeList;
class PlatformFontUsage;
class StyleResolver;
class UpdateRegionLayoutTask;

typedef HashMap<CSSStyleSheet*, RefPtr<InspectorStyleSheet> > CSSStyleSheetToInspectorStyleSheet;
typedef Vector<RefPtr<StyleSheet> > StyleSheetVector;

class InspectorCSSAgent
    : public InspectorBaseAgent<InspectorCSSAgent>
    , public InspectorDOMAgent::DOMListener
    , public InspectorBackendDispatcher::CSSCommandHandler
    , public InspectorStyleSheet::Listener {
    WTF_MAKE_NONCOPYABLE(InspectorCSSAgent);
public:
    enum MediaListSource {
        MediaListSourceLinkedSheet,
        MediaListSourceInlineSheet,
        MediaListSourceMediaRule,
        MediaListSourceImportRule
    };

    class InlineStyleOverrideScope {
    public:
        InlineStyleOverrideScope(SecurityContext* context)
            : m_contentSecurityPolicy(context->contentSecurityPolicy())
        {
            m_contentSecurityPolicy->setOverrideAllowInlineStyle(true);
        }

        ~InlineStyleOverrideScope()
        {
            m_contentSecurityPolicy->setOverrideAllowInlineStyle(false);
        }

    private:
        ContentSecurityPolicy* m_contentSecurityPolicy;
    };

    static CSSStyleRule* asCSSStyleRule(CSSRule*);
    static bool cssErrorFilter(const CSSParserString& content, int propertyId, int errorType);

    static PassOwnPtr<InspectorCSSAgent> create(InstrumentingAgents* instrumentingAgents, InspectorCompositeState* state, InspectorDOMAgent* domAgent, InspectorPageAgent* pageAgent, InspectorResourceAgent* resourceAgent)
    {
        return adoptPtr(new InspectorCSSAgent(instrumentingAgents, state, domAgent, pageAgent, resourceAgent));
    }
    ~InspectorCSSAgent();

    bool forcePseudoState(Element*, CSSSelector::PseudoType);
    virtual void setFrontend(InspectorFrontend*);
    virtual void clearFrontend();
    virtual void discardAgent();
    virtual void restore();
    virtual void enable(ErrorString*, PassRefPtr<EnableCallback>);
    virtual void disable(ErrorString*);
    void reset();
    void didCommitLoad(Frame*, DocumentLoader*);
    void mediaQueryResultChanged();
    void didCreateNamedFlow(Document*, NamedFlow*);
    void willRemoveNamedFlow(Document*, NamedFlow*);

private:
    void regionLayoutUpdated(NamedFlow*, int documentNodeId);
    void regionOversetChanged(NamedFlow*, int documentNodeId);

public:
    void didUpdateRegionLayout(Document*, NamedFlow*);
    void didChangeRegionOverset(Document*, NamedFlow*);

    void activeStyleSheetsUpdated(Document*, const StyleSheetVector& newSheets);
    void frameDetachedFromParent(Frame*);

    virtual void getComputedStyleForNode(ErrorString*, int nodeId, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSComputedStyleProperty> >&);
    virtual void getPlatformFontsForNode(ErrorString*, int nodeId, String* cssFamilyName, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::PlatformFontUsage> >&);
    virtual void getInlineStylesForNode(ErrorString*, int nodeId, RefPtr<TypeBuilder::CSS::CSSStyle>& inlineStyle, RefPtr<TypeBuilder::CSS::CSSStyle>& attributes);
    virtual void getMatchedStylesForNode(ErrorString*, int nodeId, const bool* includePseudo, const bool* includeInherited, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::RuleMatch> >& matchedCSSRules, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::PseudoIdMatches> >& pseudoIdMatches, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::InheritedStyleEntry> >& inheritedEntries);
    virtual void getAllStyleSheets(ErrorString*, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSStyleSheetHeader> >& styleSheetInfos);
    virtual void getStyleSheet(ErrorString*, const String& styleSheetId, RefPtr<TypeBuilder::CSS::CSSStyleSheetBody>& result);
    virtual void getStyleSheetText(ErrorString*, const String& styleSheetId, String* result);
    virtual void setStyleSheetText(ErrorString*, const String& styleSheetId, const String& text);
    virtual void setStyleText(ErrorString*, const RefPtr<JSONObject>& styleId, const String& text, RefPtr<TypeBuilder::CSS::CSSStyle>& result);
    virtual void setPropertyText(ErrorString*, const RefPtr<JSONObject>& styleId, int propertyIndex, const String& text, bool overwrite, RefPtr<TypeBuilder::CSS::CSSStyle>& result);
    virtual void toggleProperty(ErrorString*, const RefPtr<JSONObject>& styleId, int propertyIndex, bool disable, RefPtr<TypeBuilder::CSS::CSSStyle>& result);
    virtual void setRuleSelector(ErrorString*, const RefPtr<JSONObject>& ruleId, const String& selector, RefPtr<TypeBuilder::CSS::CSSRule>& result);
    virtual void addRule(ErrorString*, int contextNodeId, const String& selector, RefPtr<TypeBuilder::CSS::CSSRule>& result);
    virtual void getSupportedCSSProperties(ErrorString*, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSPropertyInfo> >& result);
    virtual void forcePseudoState(ErrorString*, int nodeId, const RefPtr<JSONArray>& forcedPseudoClasses);
    virtual void getNamedFlowCollection(ErrorString*, int documentNodeId, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::NamedFlow> >& result);

    PassRefPtr<TypeBuilder::CSS::CSSMedia> buildMediaObject(const MediaList*, MediaListSource, const String&, CSSStyleSheet*);
    PassRefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSMedia> > buildMediaListChain(CSSRule*);

private:
    class StyleSheetAction;
    class SetStyleSheetTextAction;
    class SetStyleTextAction;
    class SetPropertyTextAction;
    class TogglePropertyAction;
    class SetRuleSelectorAction;
    class AddRuleAction;
    class EnableResourceClient;

    InspectorCSSAgent(InstrumentingAgents*, InspectorCompositeState*, InspectorDOMAgent*, InspectorPageAgent*, InspectorResourceAgent*);

    typedef HashMap<String, RefPtr<InspectorStyleSheet> > IdToInspectorStyleSheet;
    typedef HashMap<Node*, RefPtr<InspectorStyleSheetForInlineStyle> > NodeToInspectorStyleSheet; // bogus "stylesheets" with elements' inline styles
    typedef HashMap<RefPtr<Document>, RefPtr<InspectorStyleSheet> > DocumentToViaInspectorStyleSheet; // "via inspector" stylesheets
    typedef HashMap<int, unsigned> NodeIdToForcedPseudoState;

    void wasEnabled(PassRefPtr<EnableCallback>);
    void resetNonPersistentData();
    InspectorStyleSheetForInlineStyle* asInspectorStyleSheet(Element* element);
    Element* elementForId(ErrorString*, int nodeId);
    int documentNodeWithRequestedFlowsId(Document*);
    void collectAllStyleSheets(Vector<InspectorStyleSheet*>&);
    void collectStyleSheets(CSSStyleSheet*, Vector<InspectorStyleSheet*>&);

    void collectPlatformFontsForRenderer(RenderText*, HashMap<String, int>*);

    InspectorStyleSheet* bindStyleSheet(CSSStyleSheet*);
    String unbindStyleSheet(InspectorStyleSheet*);
    InspectorStyleSheet* viaInspectorStyleSheet(Document*, bool createIfAbsent);
    InspectorStyleSheet* assertStyleSheetForId(ErrorString*, const String&);
    TypeBuilder::CSS::StyleSheetOrigin::Enum detectOrigin(CSSStyleSheet* pageStyleSheet, Document* ownerDocument);

    PassRefPtr<TypeBuilder::CSS::CSSRule> buildObjectForRule(CSSStyleRule*, StyleResolver*);
    PassRefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSRule> > buildArrayForRuleList(CSSRuleList*, StyleResolver*);
    PassRefPtr<TypeBuilder::Array<TypeBuilder::CSS::RuleMatch> > buildArrayForMatchedRuleList(CSSRuleList*, StyleResolver*, Element*);
    PassRefPtr<TypeBuilder::CSS::CSSStyle> buildObjectForAttributesStyle(Element*);
    PassRefPtr<TypeBuilder::Array<TypeBuilder::CSS::Region> > buildArrayForRegions(ErrorString*, PassRefPtr<NodeList>, int documentNodeId);
    PassRefPtr<TypeBuilder::CSS::NamedFlow> buildObjectForNamedFlow(ErrorString*, NamedFlow*, int documentNodeId);

    // InspectorDOMAgent::DOMListener implementation
    virtual void didRemoveDocument(Document*);
    virtual void didRemoveDOMNode(Node*);
    virtual void didModifyDOMAttr(Element*);

    // InspectorCSSAgent::Listener implementation
    virtual void styleSheetChanged(InspectorStyleSheet*) OVERRIDE;
    virtual void willReparseStyleSheet() OVERRIDE;
    virtual void didReparseStyleSheet() OVERRIDE;

    void resetPseudoStates();

    InspectorFrontend::CSS* m_frontend;
    InspectorDOMAgent* m_domAgent;
    InspectorPageAgent* m_pageAgent;
    InspectorResourceAgent* m_resourceAgent;

    IdToInspectorStyleSheet m_idToInspectorStyleSheet;
    CSSStyleSheetToInspectorStyleSheet m_cssStyleSheetToInspectorStyleSheet;
    NodeToInspectorStyleSheet m_nodeToInspectorStyleSheet;
    DocumentToViaInspectorStyleSheet m_documentToInspectorStyleSheet;
    NodeIdToForcedPseudoState m_nodeIdToForcedPseudoState;
    HashSet<int> m_namedFlowCollectionsRequested;
    OwnPtr<UpdateRegionLayoutTask> m_updateRegionLayoutTask;
    OwnPtr<ChangeRegionOversetTask> m_changeRegionOversetTask;

    int m_lastStyleSheetId;
    bool m_creatingViaInspectorStyleSheet;
    bool m_isSettingStyleSheetText;

    friend class ChangeRegionOversetTask;
    friend class EnableResourceClient;
    friend class StyleSheetBinder;
    friend class UpdateRegionLayoutTask;
};


} // namespace WebCore

#endif // !defined(InspectorCSSAgent_h)
