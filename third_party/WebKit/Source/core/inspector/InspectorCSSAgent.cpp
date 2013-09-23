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
#include "core/inspector/InspectorCSSAgent.h"

#include "CSSPropertyNames.h"
#include "FetchInitiatorTypeNames.h"
#include "InspectorTypeBuilder.h"
#include "StylePropertyShorthand.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/css/CSSComputedStyleDeclaration.h"
#include "core/css/CSSImportRule.h"
#include "core/css/CSSMediaRule.h"
#include "core/css/CSSParser.h"
#include "core/css/CSSRule.h"
#include "core/css/CSSRuleList.h"
#include "core/css/CSSStyleRule.h"
#include "core/css/CSSStyleSheet.h"
#include "core/css/MediaList.h"
#include "core/css/StylePropertySet.h"
#include "core/css/StyleRule.h"
#include "core/css/StyleSheet.h"
#include "core/css/StyleSheetContents.h"
#include "core/css/StyleSheetList.h"
#include "core/css/resolver/StyleResolver.h"
#include "core/dom/NamedFlow.h"
#include "core/dom/NamedFlowCollection.h"
#include "core/dom/Node.h"
#include "core/dom/NodeList.h"
#include "core/fetch/CSSStyleSheetResource.h"
#include "core/fetch/ResourceClient.h"
#include "core/fetch/ResourceFetcher.h"
#include "core/fetch/StyleSheetResourceClient.h"
#include "core/html/HTMLHeadElement.h"
#include "core/inspector/InspectorDOMAgent.h"
#include "core/inspector/InspectorHistory.h"
#include "core/inspector/InspectorPageAgent.h"
#include "core/inspector/InspectorResourceAgent.h"
#include "core/inspector/InspectorState.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/ContentSecurityPolicy.h"
#include "core/platform/JSONValues.h"
#include "core/platform/graphics/Font.h"
#include "core/platform/graphics/GlyphBuffer.h"
#include "core/platform/graphics/TextRun.h"
#include "core/platform/graphics/WidthIterator.h"
#include "core/rendering/InlineTextBox.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/RenderRegion.h"
#include "core/rendering/RenderText.h"
#include "core/rendering/RenderTextFragment.h"
#include "wtf/CurrentTime.h"
#include "wtf/HashSet.h"
#include "wtf/Vector.h"
#include "wtf/text/CString.h"
#include "wtf/text/StringConcatenate.h"

namespace CSSAgentState {
static const char cssAgentEnabled[] = "cssAgentEnabled";
}

typedef WebCore::InspectorBackendDispatcher::CSSCommandHandler::EnableCallback EnableCallback;

namespace WebCore {

enum ForcePseudoClassFlags {
    PseudoNone = 0,
    PseudoHover = 1 << 0,
    PseudoFocus = 1 << 1,
    PseudoActive = 1 << 2,
    PseudoVisited = 1 << 3
};

class StyleSheetAppender {
public:
    StyleSheetAppender(CSSStyleSheetToInspectorStyleSheet& cssStyleSheetToInspectorStyleSheet, Vector<CSSStyleSheet*>& result)
        : m_cssStyleSheetToInspectorStyleSheet(cssStyleSheetToInspectorStyleSheet)
        , m_result(result) { }

    void run(CSSStyleSheet* styleSheet)
    {
        RefPtr<InspectorStyleSheet> inspectorStyleSheet = m_cssStyleSheetToInspectorStyleSheet.get(styleSheet);
        // Avoid creating m_childRuleCSSOMWrappers in the stylesheet if it is in the process of re-parsing.
        // Otherwise m_childRuleCSSOMWrappers size will be initialized only for a part of rules, resulting in an ASSERT failure in CSSStyleSheet::item().
        // Instead, wait for the RuleMutationScope destruction and handle the complete CSSStyleSheet.
        if (inspectorStyleSheet && inspectorStyleSheet->isReparsing())
            return;
        m_result.append(styleSheet);
        for (unsigned i = 0, size = styleSheet->length(); i < size; ++i) {
            CSSRule* rule = styleSheet->item(i);
            if (rule->type() == CSSRule::IMPORT_RULE) {
                CSSStyleSheet* importedStyleSheet = toCSSImportRule(rule)->styleSheet();
                if (importedStyleSheet)
                    run(importedStyleSheet);
            }
        }
    }

private:
    CSSStyleSheetToInspectorStyleSheet& m_cssStyleSheetToInspectorStyleSheet;
    Vector<CSSStyleSheet*>& m_result;
};

static unsigned computePseudoClassMask(JSONArray* pseudoClassArray)
{
    DEFINE_STATIC_LOCAL(String, active, ("active"));
    DEFINE_STATIC_LOCAL(String, hover, ("hover"));
    DEFINE_STATIC_LOCAL(String, focus, ("focus"));
    DEFINE_STATIC_LOCAL(String, visited, ("visited"));
    if (!pseudoClassArray || !pseudoClassArray->length())
        return PseudoNone;

    unsigned result = PseudoNone;
    for (size_t i = 0; i < pseudoClassArray->length(); ++i) {
        RefPtr<JSONValue> pseudoClassValue = pseudoClassArray->get(i);
        String pseudoClass;
        bool success = pseudoClassValue->asString(&pseudoClass);
        if (!success)
            continue;
        if (pseudoClass == active)
            result |= PseudoActive;
        else if (pseudoClass == hover)
            result |= PseudoHover;
        else if (pseudoClass == focus)
            result |= PseudoFocus;
        else if (pseudoClass == visited)
            result |= PseudoVisited;
    }

    return result;
}

class UpdateRegionLayoutTask {
public:
    UpdateRegionLayoutTask(InspectorCSSAgent*);
    void scheduleFor(NamedFlow*, int documentNodeId);
    void unschedule(NamedFlow*);
    void reset();
    void onTimer(Timer<UpdateRegionLayoutTask>*);

private:
    InspectorCSSAgent* m_cssAgent;
    Timer<UpdateRegionLayoutTask> m_timer;
    HashMap<NamedFlow*, int> m_namedFlows;
};

UpdateRegionLayoutTask::UpdateRegionLayoutTask(InspectorCSSAgent* cssAgent)
    : m_cssAgent(cssAgent)
    , m_timer(this, &UpdateRegionLayoutTask::onTimer)
{
}

void UpdateRegionLayoutTask::scheduleFor(NamedFlow* namedFlow, int documentNodeId)
{
    m_namedFlows.add(namedFlow, documentNodeId);

    if (!m_timer.isActive())
        m_timer.startOneShot(0);
}

void UpdateRegionLayoutTask::unschedule(NamedFlow* namedFlow)
{
    m_namedFlows.remove(namedFlow);
}

void UpdateRegionLayoutTask::reset()
{
    m_timer.stop();
    m_namedFlows.clear();
}

void UpdateRegionLayoutTask::onTimer(Timer<UpdateRegionLayoutTask>*)
{
    // The timer is stopped on m_cssAgent destruction, so this method will never be called after m_cssAgent has been destroyed.
    Vector<std::pair<NamedFlow*, int> > namedFlows;

    for (HashMap<NamedFlow*, int>::iterator it = m_namedFlows.begin(), end = m_namedFlows.end(); it != end; ++it)
        namedFlows.append(std::make_pair(it->key, it->value));

    for (unsigned i = 0, size = namedFlows.size(); i < size; ++i) {
        NamedFlow* namedFlow = namedFlows.at(i).first;
        int documentNodeId = namedFlows.at(i).second;

        if (m_namedFlows.contains(namedFlow)) {
            m_cssAgent->regionLayoutUpdated(namedFlow, documentNodeId);
            m_namedFlows.remove(namedFlow);
        }
    }

    if (!m_namedFlows.isEmpty() && !m_timer.isActive())
        m_timer.startOneShot(0);
}

class ChangeRegionOversetTask {
public:
    ChangeRegionOversetTask(InspectorCSSAgent*);
    void scheduleFor(NamedFlow*, int documentNodeId);
    void unschedule(NamedFlow*);
    void reset();
    void onTimer(Timer<ChangeRegionOversetTask>*);

private:
    InspectorCSSAgent* m_cssAgent;
    Timer<ChangeRegionOversetTask> m_timer;
    HashMap<NamedFlow*, int> m_namedFlows;
};

ChangeRegionOversetTask::ChangeRegionOversetTask(InspectorCSSAgent* cssAgent)
    : m_cssAgent(cssAgent)
    , m_timer(this, &ChangeRegionOversetTask::onTimer)
{
}

void ChangeRegionOversetTask::scheduleFor(NamedFlow* namedFlow, int documentNodeId)
{
    m_namedFlows.add(namedFlow, documentNodeId);

    if (!m_timer.isActive())
        m_timer.startOneShot(0);
}

void ChangeRegionOversetTask::unschedule(NamedFlow* namedFlow)
{
    m_namedFlows.remove(namedFlow);
}

void ChangeRegionOversetTask::reset()
{
    m_timer.stop();
    m_namedFlows.clear();
}

void ChangeRegionOversetTask::onTimer(Timer<ChangeRegionOversetTask>*)
{
    // The timer is stopped on m_cssAgent destruction, so this method will never be called after m_cssAgent has been destroyed.
    for (HashMap<NamedFlow*, int>::iterator it = m_namedFlows.begin(), end = m_namedFlows.end(); it != end; ++it)
        m_cssAgent->regionOversetChanged(it->key, it->value);

    m_namedFlows.clear();
}

class InspectorCSSAgent::StyleSheetAction : public InspectorHistory::Action {
    WTF_MAKE_NONCOPYABLE(StyleSheetAction);
public:
    StyleSheetAction(const String& name, InspectorStyleSheet* styleSheet)
        : InspectorHistory::Action(name)
        , m_styleSheet(styleSheet)
    {
    }

protected:
    RefPtr<InspectorStyleSheet> m_styleSheet;
};

class InspectorCSSAgent::EnableResourceClient : public StyleSheetResourceClient {
public:
    EnableResourceClient(InspectorCSSAgent*, const Vector<InspectorStyleSheet*>&, PassRefPtr<EnableCallback>);

    virtual void setCSSStyleSheet(const String&, const KURL&, const String&, const CSSStyleSheetResource*) OVERRIDE;

private:
    RefPtr<EnableCallback> m_callback;
    InspectorCSSAgent* m_cssAgent;
    int m_pendingResources;
    Vector<InspectorStyleSheet*> m_styleSheets;
};

InspectorCSSAgent::EnableResourceClient::EnableResourceClient(InspectorCSSAgent* cssAgent, const Vector<InspectorStyleSheet*>& styleSheets, PassRefPtr<EnableCallback> callback)
    : m_callback(callback)
    , m_cssAgent(cssAgent)
    , m_pendingResources(styleSheets.size())
    , m_styleSheets(styleSheets)
{
    for (size_t i = 0; i < styleSheets.size(); ++i) {
        InspectorStyleSheet* styleSheet = styleSheets.at(i);
        Document* document = styleSheet->ownerDocument();
        FetchRequest request(ResourceRequest(styleSheet->finalURL()), FetchInitiatorTypeNames::inspector);
        ResourcePtr<Resource> resource = document->fetcher()->fetchCSSStyleSheet(request);
        resource->addClient(this);
    }
}

void InspectorCSSAgent::EnableResourceClient::setCSSStyleSheet(const String&, const KURL& url, const String&, const CSSStyleSheetResource* resource)
{
    const_cast<CSSStyleSheetResource*>(resource)->removeClient(this);
    --m_pendingResources;
    if (m_pendingResources)
        return;

    // enable always succeeds.
    if (m_callback->isActive())
        m_cssAgent->wasEnabled(m_callback.release());
    delete this;
}

class InspectorCSSAgent::SetStyleSheetTextAction : public InspectorCSSAgent::StyleSheetAction {
    WTF_MAKE_NONCOPYABLE(SetStyleSheetTextAction);
public:
    SetStyleSheetTextAction(InspectorStyleSheet* styleSheet, const String& text)
        : InspectorCSSAgent::StyleSheetAction("SetStyleSheetText", styleSheet)
        , m_text(text)
    {
    }

    virtual bool perform(ExceptionState& es)
    {
        if (!m_styleSheet->getText(&m_oldText))
            return false;
        return redo(es);
    }

    virtual bool undo(ExceptionState& es)
    {
        if (m_styleSheet->setText(m_oldText, es)) {
            m_styleSheet->reparseStyleSheet(m_oldText);
            return true;
        }
        return false;
    }

    virtual bool redo(ExceptionState& es)
    {
        if (m_styleSheet->setText(m_text, es)) {
            m_styleSheet->reparseStyleSheet(m_text);
            return true;
        }
        return false;
    }

    virtual String mergeId()
    {
        return String::format("SetStyleSheetText %s", m_styleSheet->id().utf8().data());
    }

    virtual void merge(PassOwnPtr<Action> action)
    {
        ASSERT(action->mergeId() == mergeId());

        SetStyleSheetTextAction* other = static_cast<SetStyleSheetTextAction*>(action.get());
        m_text = other->m_text;
    }

private:
    String m_text;
    String m_oldText;
};

class InspectorCSSAgent::SetStyleTextAction : public InspectorCSSAgent::StyleSheetAction {
    WTF_MAKE_NONCOPYABLE(SetStyleTextAction);
public:
    SetStyleTextAction(InspectorStyleSheet* styleSheet, const InspectorCSSId& cssId, const String& text)
        : InspectorCSSAgent::StyleSheetAction("SetPropertyText", styleSheet)
        , m_cssId(cssId)
        , m_text(text)
    {
    }

    virtual String toString()
    {
        return mergeId() + ": " + m_oldText + " -> " + m_text;
    }

    virtual bool perform(ExceptionState& es)
    {
        return redo(es);
    }

    virtual bool undo(ExceptionState& es)
    {
        String placeholder;
        return m_styleSheet->setStyleText(m_cssId, m_oldText, &placeholder, es);
    }

    virtual bool redo(ExceptionState& es)
    {
        return m_styleSheet->setStyleText(m_cssId, m_text, &m_oldText, es);
    }

    virtual String mergeId()
    {
        return String::format("SetStyleText %s:%u", m_cssId.styleSheetId().utf8().data(), m_cssId.ordinal());
    }

    virtual void merge(PassOwnPtr<Action> action)
    {
        ASSERT(action->mergeId() == mergeId());

        SetStyleTextAction* other = static_cast<SetStyleTextAction*>(action.get());
        m_text = other->m_text;
    }

private:
    InspectorCSSId m_cssId;
    String m_text;
    String m_oldText;
};

class InspectorCSSAgent::SetPropertyTextAction : public InspectorCSSAgent::StyleSheetAction {
    WTF_MAKE_NONCOPYABLE(SetPropertyTextAction);
public:
    SetPropertyTextAction(InspectorStyleSheet* styleSheet, const InspectorCSSId& cssId, unsigned propertyIndex, const String& text, bool overwrite)
        : InspectorCSSAgent::StyleSheetAction("SetPropertyText", styleSheet)
        , m_cssId(cssId)
        , m_propertyIndex(propertyIndex)
        , m_text(text)
        , m_overwrite(overwrite)
    {
    }

    virtual String toString()
    {
        return mergeId() + ": " + m_oldText + " -> " + m_text;
    }

    virtual bool perform(ExceptionState& es)
    {
        return redo(es);
    }

    virtual bool undo(ExceptionState& es)
    {
        String placeholder;
        return m_styleSheet->setPropertyText(m_cssId, m_propertyIndex, m_overwrite ? m_oldText : "", true, &placeholder, es);
    }

    virtual bool redo(ExceptionState& es)
    {
        String oldText;
        bool result = m_styleSheet->setPropertyText(m_cssId, m_propertyIndex, m_text, m_overwrite, &oldText, es);
        m_oldText = oldText.stripWhiteSpace();
        // FIXME: remove this once the model handles this case.
        if (!m_oldText.endsWith(';'))
            m_oldText.append(';');
        return result;
    }

    virtual String mergeId()
    {
        return String::format("SetPropertyText %s:%u:%s", m_styleSheet->id().utf8().data(), m_propertyIndex, m_overwrite ? "true" : "false");
    }

    virtual void merge(PassOwnPtr<Action> action)
    {
        ASSERT(action->mergeId() == mergeId());

        SetPropertyTextAction* other = static_cast<SetPropertyTextAction*>(action.get());
        m_text = other->m_text;
    }

private:
    InspectorCSSId m_cssId;
    unsigned m_propertyIndex;
    String m_text;
    String m_oldText;
    bool m_overwrite;
};

class InspectorCSSAgent::TogglePropertyAction : public InspectorCSSAgent::StyleSheetAction {
    WTF_MAKE_NONCOPYABLE(TogglePropertyAction);
public:
    TogglePropertyAction(InspectorStyleSheet* styleSheet, const InspectorCSSId& cssId, unsigned propertyIndex, bool disable)
        : InspectorCSSAgent::StyleSheetAction("ToggleProperty", styleSheet)
        , m_cssId(cssId)
        , m_propertyIndex(propertyIndex)
        , m_disable(disable)
    {
    }

    virtual bool perform(ExceptionState& es)
    {
        return redo(es);
    }

    virtual bool undo(ExceptionState& es)
    {
        return m_styleSheet->toggleProperty(m_cssId, m_propertyIndex, !m_disable, es);
    }

    virtual bool redo(ExceptionState& es)
    {
        return m_styleSheet->toggleProperty(m_cssId, m_propertyIndex, m_disable, es);
    }

private:
    InspectorCSSId m_cssId;
    unsigned m_propertyIndex;
    bool m_disable;
};

class InspectorCSSAgent::SetRuleSelectorAction : public InspectorCSSAgent::StyleSheetAction {
    WTF_MAKE_NONCOPYABLE(SetRuleSelectorAction);
public:
    SetRuleSelectorAction(InspectorStyleSheet* styleSheet, const InspectorCSSId& cssId, const String& selector)
        : InspectorCSSAgent::StyleSheetAction("SetRuleSelector", styleSheet)
        , m_cssId(cssId)
        , m_selector(selector)
    {
    }

    virtual bool perform(ExceptionState& es)
    {
        m_oldSelector = m_styleSheet->ruleSelector(m_cssId, es);
        if (es.hadException())
            return false;
        return redo(es);
    }

    virtual bool undo(ExceptionState& es)
    {
        return m_styleSheet->setRuleSelector(m_cssId, m_oldSelector, es);
    }

    virtual bool redo(ExceptionState& es)
    {
        return m_styleSheet->setRuleSelector(m_cssId, m_selector, es);
    }

private:
    InspectorCSSId m_cssId;
    String m_selector;
    String m_oldSelector;
};

class InspectorCSSAgent::AddRuleAction : public InspectorCSSAgent::StyleSheetAction {
    WTF_MAKE_NONCOPYABLE(AddRuleAction);
public:
    AddRuleAction(InspectorStyleSheet* styleSheet, const String& selector)
        : InspectorCSSAgent::StyleSheetAction("AddRule", styleSheet)
        , m_selector(selector)
    {
    }

    virtual bool perform(ExceptionState& es)
    {
        return redo(es);
    }

    virtual bool undo(ExceptionState& es)
    {
        return m_styleSheet->deleteRule(m_newId, es);
    }

    virtual bool redo(ExceptionState& es)
    {
        CSSStyleRule* cssStyleRule = m_styleSheet->addRule(m_selector, es);
        if (es.hadException())
            return false;
        m_newId = m_styleSheet->ruleId(cssStyleRule);
        return true;
    }

    InspectorCSSId newRuleId() { return m_newId; }

private:
    InspectorCSSId m_newId;
    String m_selector;
    String m_oldSelector;
};

// static
CSSStyleRule* InspectorCSSAgent::asCSSStyleRule(CSSRule* rule)
{
    if (!rule || rule->type() != CSSRule::STYLE_RULE)
        return 0;
    return toCSSStyleRule(rule);
}

template <typename CharType, size_t bufferLength>
static size_t vendorPrefixLowerCase(const CharType* string, size_t stringLength, char (&buffer)[bufferLength])
{
    static const char lowerCaseOffset = 'a' - 'A';

    if (string[0] != '-')
        return 0;

    for (size_t i = 0; i < stringLength - 1; i++) {
        CharType c = string[i + 1];
        if (c == '-')
            return i;
        if (i == bufferLength)
            break;
        if (c < 'A' || c > 'z')
            break;
        if (c >= 'a')
            buffer[i] = c;
        else if (c <= 'Z')
            buffer[i] = c + lowerCaseOffset;
        else
            break;
    }
    return 0;
}

template <size_t patternLength>
static bool equals(const char* prefix, size_t prefixLength, const char (&pattern)[patternLength])
{
    if (prefixLength != patternLength - 1)
        return false;
    for (size_t i = 0; i < patternLength - 1; i++) {
        if (prefix[i] != pattern[i])
            return false;
    }
    return true;
}

static bool hasNonWebkitVendorSpecificPrefix(const CSSParserString& string)
{
    // Known prefixes: http://wiki.csswg.org/spec/vendor-prefixes
    const size_t stringLength = string.length();
    if (stringLength < 4)
        return false;

    char buffer[6];
    size_t prefixLength = string.is8Bit() ?
        vendorPrefixLowerCase(string.characters8(), stringLength, buffer) :
        vendorPrefixLowerCase(string.characters16(), stringLength, buffer);

    if (!prefixLength || prefixLength == stringLength - 2)
        return false;

    switch (buffer[0]) {
    case 'a':
        return (prefixLength == 2 && buffer[1] == 'h') || equals(buffer + 1, prefixLength - 1, "tsc");
    case 'e':
        return equals(buffer + 1, prefixLength - 1, "pub");
    case 'h':
        return prefixLength == 2 && buffer[1] == 'p';
    case 'i':
        return equals(buffer + 1, prefixLength - 1, "books");
    case 'k':
        return equals(buffer + 1, prefixLength - 1, "html");
    case 'm':
        if (prefixLength == 2)
            return buffer[1] == 's';
        if (prefixLength == 3)
            return (buffer[1] == 'o' && buffer[2] == 'z') || (buffer[1] == 's' || buffer[2] == 'o');
        break;
    case 'o':
        return prefixLength == 1;
    case 'p':
        return equals(buffer + 1, prefixLength - 1, "rince");
    case 'r':
        return (prefixLength == 2 && buffer[1] == 'o') || equals(buffer + 1, prefixLength - 1, "im");
    case 't':
        return prefixLength == 2 && buffer[1] == 'c';
    case 'w':
        return (prefixLength == 3 && buffer[1] == 'a' && buffer[2] == 'p') || equals(buffer + 1, prefixLength - 1, "easy");
    case 'x':
        return prefixLength == 2 && buffer[1] == 'v';
    }
    return false;
}

static bool isValidPropertyName(const CSSParserString& content)
{
    if (content.equalIgnoringCase("animation")
        || content.equalIgnoringCase("font-size-adjust")
        || content.equalIgnoringCase("transform")
        || content.equalIgnoringCase("user-select")
        || content.equalIgnoringCase("-webkit-flex-pack")
        || content.equalIgnoringCase("-webkit-text-size-adjust"))
        return true;

    return false;
}

// static
bool InspectorCSSAgent::cssErrorFilter(const CSSParserString& content, int propertyId, int errorType)
{
    const size_t contentLength = content.length();

    switch (errorType) {
    case CSSParser::PropertyDeclarationError:
        // Ignore errors like "*property: value". This trick is used for IE7: http://stackoverflow.com/questions/4563651/what-does-an-asterisk-do-in-a-css-property-name
        if (contentLength && content[0] == '*')
            return false;

        // The "filter" property is commonly used instead of "opacity" for IE9.
        if (propertyId == CSSPropertyFilter)
            return false;

        break;

    case CSSParser::InvalidPropertyValueError:
        // The "filter" property is commonly used instead of "opacity" for IE9.
        if (propertyId == CSSPropertyFilter)
            return false;

        // Value might be a vendor-specific function.
        if (hasNonWebkitVendorSpecificPrefix(content))
            return false;

        // IE-only "cursor: hand".
        if (propertyId == CSSPropertyCursor && content.equalIgnoringCase("hand"))
            return false;

        // Ignore properties like "property: value \9" (common IE hack) or "property: value \0" (IE 8 hack).
        if (contentLength > 2 && content[contentLength - 2] == '\\' && (content[contentLength - 1] == '9' || content[contentLength - 1] == '0'))
            return false;

        if (contentLength > 3) {

            // property: value\0/;
            if (content[contentLength - 3] == '\\' && content[contentLength - 2] == '0' && content[contentLength - 1] == '/')
                return false;

            // property: value !ie;
            if (content[contentLength - 3] == '!' && content[contentLength - 2] == 'i' && content[contentLength - 1] == 'e')
                return false;
        }

        // Popular value prefixes valid in other browsers.
        if (content.startsWithIgnoringCase("linear-gradient"))
            return false;
        if (content.startsWithIgnoringCase("-webkit-flexbox"))
            return false;
        if (propertyId == CSSPropertyUnicodeBidi && content.startsWithIgnoringCase("isolate"))
            return false;

        break;

    case CSSParser::InvalidPropertyError:
        if (hasNonWebkitVendorSpecificPrefix(content))
            return false;

        // Another hack to make IE-only property.
        if (contentLength && content[0] == '_')
            return false;

        // IE-only set of properties.
        if (content.startsWithIgnoringCase("scrollbar-"))
            return false;

        if (isValidPropertyName(content))
            return false;

        break;

    case CSSParser::InvalidRuleError:
        // Block error reporting for @-rules for now to avoid noise.
        if (contentLength > 4 && content[0] == '@')
            return false;
        return true;

    case CSSParser::InvalidSelectorPseudoError:
        if (hasNonWebkitVendorSpecificPrefix(content))
            return false;
    }
    return true;
}

InspectorCSSAgent::InspectorCSSAgent(InstrumentingAgents* instrumentingAgents, InspectorCompositeState* state, InspectorDOMAgent* domAgent, InspectorPageAgent* pageAgent, InspectorResourceAgent* resourceAgent)
    : InspectorBaseAgent<InspectorCSSAgent>("CSS", instrumentingAgents, state)
    , m_frontend(0)
    , m_domAgent(domAgent)
    , m_pageAgent(pageAgent)
    , m_resourceAgent(resourceAgent)
    , m_lastStyleSheetId(1)
    , m_creatingViaInspectorStyleSheet(false)
    , m_isSettingStyleSheetText(false)
{
    m_domAgent->setDOMListener(this);
}

InspectorCSSAgent::~InspectorCSSAgent()
{
    ASSERT(!m_domAgent);
    reset();
}

void InspectorCSSAgent::setFrontend(InspectorFrontend* frontend)
{
    ASSERT(!m_frontend);
    m_frontend = frontend->css();
}

void InspectorCSSAgent::clearFrontend()
{
    ASSERT(m_frontend);
    m_frontend = 0;
    resetNonPersistentData();
}

void InspectorCSSAgent::discardAgent()
{
    m_domAgent->setDOMListener(0);
    m_domAgent = 0;
}

void InspectorCSSAgent::restore()
{
    if (m_state->getBoolean(CSSAgentState::cssAgentEnabled))
        wasEnabled(0);
}

void InspectorCSSAgent::reset()
{
    m_idToInspectorStyleSheet.clear();
    m_cssStyleSheetToInspectorStyleSheet.clear();
    m_nodeToInspectorStyleSheet.clear();
    m_documentToInspectorStyleSheet.clear();
    resetNonPersistentData();
}

void InspectorCSSAgent::resetNonPersistentData()
{
    m_namedFlowCollectionsRequested.clear();
    if (m_updateRegionLayoutTask)
        m_updateRegionLayoutTask->reset();
    if (m_changeRegionOversetTask)
        m_changeRegionOversetTask->reset();
    resetPseudoStates();
}

void InspectorCSSAgent::enable(ErrorString*, PassRefPtr<EnableCallback> prpCallback)
{
    m_state->setBoolean(CSSAgentState::cssAgentEnabled, true);

    Vector<InspectorStyleSheet*> styleSheets;
    collectAllStyleSheets(styleSheets);

    // Re-issue stylesheet requets for resources that are no longer in memory cache.
    Vector<InspectorStyleSheet*> styleSheetsToFetch;
    HashSet<String> urlsToFetch;
    for (size_t i = 0; i < styleSheets.size(); ++i) {
        InspectorStyleSheet* styleSheet = styleSheets.at(i);
        String url = styleSheet->finalURL();
        if (urlsToFetch.contains(url))
            continue;
        CSSStyleSheet* pageStyleSheet = styleSheet->pageStyleSheet();
        if (pageStyleSheet->isInline() || !pageStyleSheet->contents()->loadCompleted())
            continue;
        Document* document = styleSheet->ownerDocument();
        if (!document)
            continue;
        Resource* cachedResource = document->fetcher()->cachedResource(url);
        if (cachedResource)
            continue;
        urlsToFetch.add(styleSheet->finalURL());
        styleSheetsToFetch.append(styleSheet);
    }

    if (styleSheetsToFetch.isEmpty()) {
        wasEnabled(prpCallback);
        return;
    }
    new EnableResourceClient(this, styleSheetsToFetch, prpCallback);
}

void InspectorCSSAgent::wasEnabled(PassRefPtr<EnableCallback> callback)
{
    if (!m_state->getBoolean(CSSAgentState::cssAgentEnabled)) {
        // We were disabled while fetching resources.
        return;
    }

    // Re-read stylesheets, we know for sure we have content for all of them.
    Vector<InspectorStyleSheet*> styleSheets;
    collectAllStyleSheets(styleSheets);
    for (size_t i = 0; i < styleSheets.size(); ++i)
        m_frontend->styleSheetAdded(styleSheets.at(i)->buildObjectForStyleSheetInfo());
    if (callback)
        callback->sendSuccess();

    m_instrumentingAgents->setInspectorCSSAgent(this);
}

void InspectorCSSAgent::disable(ErrorString*)
{
    m_instrumentingAgents->setInspectorCSSAgent(0);
    m_state->setBoolean(CSSAgentState::cssAgentEnabled, false);
}

void InspectorCSSAgent::didCommitLoad(Frame* frame, DocumentLoader* loader)
{
    if (loader->frame() != frame->page()->mainFrame())
        return;

    reset();
}

void InspectorCSSAgent::mediaQueryResultChanged()
{
    if (m_frontend)
        m_frontend->mediaQueryResultChanged();
}

void InspectorCSSAgent::didCreateNamedFlow(Document* document, NamedFlow* namedFlow)
{
    int documentNodeId = documentNodeWithRequestedFlowsId(document);
    if (!documentNodeId)
        return;

    ErrorString errorString;
    m_frontend->namedFlowCreated(buildObjectForNamedFlow(&errorString, namedFlow, documentNodeId));
}

void InspectorCSSAgent::willRemoveNamedFlow(Document* document, NamedFlow* namedFlow)
{
    int documentNodeId = documentNodeWithRequestedFlowsId(document);
    if (!documentNodeId)
        return;

    if (m_updateRegionLayoutTask)
        m_updateRegionLayoutTask->unschedule(namedFlow);

    m_frontend->namedFlowRemoved(documentNodeId, namedFlow->name().string());
}

void InspectorCSSAgent::didUpdateRegionLayout(Document* document, NamedFlow* namedFlow)
{
    int documentNodeId = documentNodeWithRequestedFlowsId(document);
    if (!documentNodeId)
        return;

    if (!m_updateRegionLayoutTask)
        m_updateRegionLayoutTask = adoptPtr(new UpdateRegionLayoutTask(this));
    m_updateRegionLayoutTask->scheduleFor(namedFlow, documentNodeId);
}

void InspectorCSSAgent::regionLayoutUpdated(NamedFlow* namedFlow, int documentNodeId)
{
    if (namedFlow->flowState() == NamedFlow::FlowStateNull)
        return;

    ErrorString errorString;
    RefPtr<NamedFlow> protector(namedFlow);

    m_frontend->regionLayoutUpdated(buildObjectForNamedFlow(&errorString, namedFlow, documentNodeId));
}

void InspectorCSSAgent::didChangeRegionOverset(Document* document, NamedFlow* namedFlow)
{
    int documentNodeId = documentNodeWithRequestedFlowsId(document);
    if (!documentNodeId)
        return;

    if (!m_changeRegionOversetTask)
        m_changeRegionOversetTask = adoptPtr(new ChangeRegionOversetTask(this));
    m_changeRegionOversetTask->scheduleFor(namedFlow, documentNodeId);
}

void InspectorCSSAgent::regionOversetChanged(NamedFlow* namedFlow, int documentNodeId)
{
    if (namedFlow->flowState() == NamedFlow::FlowStateNull)
        return;

    ErrorString errorString;
    RefPtr<NamedFlow> protector(namedFlow);

    m_frontend->regionOversetChanged(buildObjectForNamedFlow(&errorString, namedFlow, documentNodeId));
}

void InspectorCSSAgent::activeStyleSheetsUpdated(Document* document, const StyleSheetVector& newSheets)
{
    if (m_isSettingStyleSheetText)
        return;
    HashSet<CSSStyleSheet*> removedSheets;
    for (CSSStyleSheetToInspectorStyleSheet::iterator it = m_cssStyleSheetToInspectorStyleSheet.begin(); it != m_cssStyleSheetToInspectorStyleSheet.end(); ++it) {
        if (it->value->canBind() && (!it->key->ownerDocument() || it->key->ownerDocument() == document))
            removedSheets.add(it->key);
    }

    Vector<CSSStyleSheet*> newSheetsVector;
    for (size_t i = 0, size = newSheets.size(); i < size; ++i) {
        StyleSheet* newSheet = newSheets.at(i).get();
        if (newSheet->isCSSStyleSheet()) {
            StyleSheetAppender appender(m_cssStyleSheetToInspectorStyleSheet, newSheetsVector);
            appender.run(static_cast<CSSStyleSheet*>(newSheet));
        }
    }

    HashSet<CSSStyleSheet*> addedSheets;
    for (size_t i = 0; i < newSheetsVector.size(); ++i) {
        CSSStyleSheet* newCSSSheet = newSheetsVector.at(i);
        if (removedSheets.contains(newCSSSheet))
            removedSheets.remove(newCSSSheet);
        else
            addedSheets.add(newCSSSheet);
    }

    for (HashSet<CSSStyleSheet*>::iterator it = removedSheets.begin(); it != removedSheets.end(); ++it) {
        RefPtr<InspectorStyleSheet> inspectorStyleSheet = m_cssStyleSheetToInspectorStyleSheet.get(*it);
        ASSERT(inspectorStyleSheet);
        if (m_idToInspectorStyleSheet.contains(inspectorStyleSheet->id())) {
            String id = unbindStyleSheet(inspectorStyleSheet.get());
            if (m_frontend)
                m_frontend->styleSheetRemoved(id);
        }
    }

    for (HashSet<CSSStyleSheet*>::iterator it = addedSheets.begin(); it != addedSheets.end(); ++it) {
        if (!m_cssStyleSheetToInspectorStyleSheet.contains(*it)) {
            InspectorStyleSheet* newStyleSheet = bindStyleSheet(static_cast<CSSStyleSheet*>(*it));
            if (m_frontend)
                m_frontend->styleSheetAdded(newStyleSheet->buildObjectForStyleSheetInfo());
        }
    }
}

void InspectorCSSAgent::frameDetachedFromParent(Frame* frame)
{
    Document* document = frame->document();
    if (!document)
        return;
    StyleSheetVector newSheets;
    activeStyleSheetsUpdated(document, newSheets);
}

bool InspectorCSSAgent::forcePseudoState(Element* element, CSSSelector::PseudoType pseudoType)
{
    if (m_nodeIdToForcedPseudoState.isEmpty())
        return false;

    int nodeId = m_domAgent->boundNodeId(element);
    if (!nodeId)
        return false;

    NodeIdToForcedPseudoState::iterator it = m_nodeIdToForcedPseudoState.find(nodeId);
    if (it == m_nodeIdToForcedPseudoState.end())
        return false;

    unsigned forcedPseudoState = it->value;
    switch (pseudoType) {
    case CSSSelector::PseudoActive:
        return forcedPseudoState & PseudoActive;
    case CSSSelector::PseudoFocus:
        return forcedPseudoState & PseudoFocus;
    case CSSSelector::PseudoHover:
        return forcedPseudoState & PseudoHover;
    case CSSSelector::PseudoVisited:
        return forcedPseudoState & PseudoVisited;
    default:
        return false;
    }
}

void InspectorCSSAgent::getMatchedStylesForNode(ErrorString* errorString, int nodeId, const bool* includePseudo, const bool* includeInherited, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::RuleMatch> >& matchedCSSRules, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::PseudoIdMatches> >& pseudoIdMatches, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::InheritedStyleEntry> >& inheritedEntries)
{
    Element* element = elementForId(errorString, nodeId);
    if (!element)
        return;

    Element* originalElement = element;
    PseudoId elementPseudoId = element->pseudoId();
    if (elementPseudoId)
        element = element->parentOrShadowHostElement();

    // Matched rules.
    StyleResolver* styleResolver = element->ownerDocument()->styleResolver();
    RefPtr<CSSRuleList> matchedRules = styleResolver->pseudoStyleRulesForElement(element, elementPseudoId, StyleResolver::AllCSSRules);
    matchedCSSRules = buildArrayForMatchedRuleList(matchedRules.get(), styleResolver, originalElement);

    // Pseudo elements.
    if (!elementPseudoId && (!includePseudo || *includePseudo)) {
        RefPtr<TypeBuilder::Array<TypeBuilder::CSS::PseudoIdMatches> > pseudoElements = TypeBuilder::Array<TypeBuilder::CSS::PseudoIdMatches>::create();
        for (PseudoId pseudoId = FIRST_PUBLIC_PSEUDOID; pseudoId < AFTER_LAST_INTERNAL_PSEUDOID; pseudoId = static_cast<PseudoId>(pseudoId + 1)) {
            RefPtr<CSSRuleList> matchedRules = styleResolver->pseudoStyleRulesForElement(element, pseudoId, StyleResolver::AllCSSRules);
            if (matchedRules && matchedRules->length()) {
                RefPtr<TypeBuilder::CSS::PseudoIdMatches> matches = TypeBuilder::CSS::PseudoIdMatches::create()
                    .setPseudoId(static_cast<int>(pseudoId))
                    .setMatches(buildArrayForMatchedRuleList(matchedRules.get(), styleResolver, element));
                pseudoElements->addItem(matches.release());
            }
        }

        pseudoIdMatches = pseudoElements.release();
    }

    // Inherited styles.
    if (!elementPseudoId && (!includeInherited || *includeInherited)) {
        RefPtr<TypeBuilder::Array<TypeBuilder::CSS::InheritedStyleEntry> > entries = TypeBuilder::Array<TypeBuilder::CSS::InheritedStyleEntry>::create();
        Element* parentElement = element->parentElement();
        while (parentElement) {
            StyleResolver* parentStyleResolver = parentElement->ownerDocument()->styleResolver();
            RefPtr<CSSRuleList> parentMatchedRules = parentStyleResolver->styleRulesForElement(parentElement, StyleResolver::AllCSSRules);
            RefPtr<TypeBuilder::CSS::InheritedStyleEntry> entry = TypeBuilder::CSS::InheritedStyleEntry::create()
                .setMatchedCSSRules(buildArrayForMatchedRuleList(parentMatchedRules.get(), styleResolver, parentElement));
            if (parentElement->style() && parentElement->style()->length()) {
                InspectorStyleSheetForInlineStyle* styleSheet = asInspectorStyleSheet(parentElement);
                if (styleSheet)
                    entry->setInlineStyle(styleSheet->buildObjectForStyle(styleSheet->styleForId(InspectorCSSId(styleSheet->id(), 0))));
            }

            entries->addItem(entry.release());
            parentElement = parentElement->parentElement();
        }

        inheritedEntries = entries.release();
    }
}

void InspectorCSSAgent::getInlineStylesForNode(ErrorString* errorString, int nodeId, RefPtr<TypeBuilder::CSS::CSSStyle>& inlineStyle, RefPtr<TypeBuilder::CSS::CSSStyle>& attributesStyle)
{
    Element* element = elementForId(errorString, nodeId);
    if (!element)
        return;

    InspectorStyleSheetForInlineStyle* styleSheet = asInspectorStyleSheet(element);
    if (!styleSheet)
        return;

    inlineStyle = styleSheet->buildObjectForStyle(element->style());
    RefPtr<TypeBuilder::CSS::CSSStyle> attributes = buildObjectForAttributesStyle(element);
    attributesStyle = attributes ? attributes.release() : 0;
}

void InspectorCSSAgent::getComputedStyleForNode(ErrorString* errorString, int nodeId, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSComputedStyleProperty> >& style)
{
    Node* node = m_domAgent->assertNode(errorString, nodeId);
    if (!node)
        return;

    RefPtr<CSSComputedStyleDeclaration> computedStyleInfo = CSSComputedStyleDeclaration::create(node, true);
    RefPtr<InspectorStyle> inspectorStyle = InspectorStyle::create(InspectorCSSId(), computedStyleInfo, 0);
    style = inspectorStyle->buildArrayForComputedStyle();
}

void InspectorCSSAgent::collectPlatformFontsForRenderer(RenderText* renderer, HashMap<String, int>* fontStats)
{
    for (InlineTextBox* box = renderer->firstTextBox(); box; box = box->nextTextBox()) {
        RenderStyle* style = renderer->style(box->isFirstLineStyle());
        const Font& font = style->font();
        TextRun run = box->constructTextRunForInspector(style, font);
        WidthIterator it(&font, run, 0, false);
        GlyphBuffer glyphBuffer;
        it.advance(run.length(), &glyphBuffer);
        for (int i = 0; i < glyphBuffer.size(); ++i) {
            String familyName = glyphBuffer.fontDataAt(i)->platformData().fontFamilyName();
            int value = fontStats->contains(familyName) ? fontStats->get(familyName) : 0;
            fontStats->set(familyName, value + 1);
        }
    }
}

void InspectorCSSAgent::getPlatformFontsForNode(ErrorString* errorString, int nodeId,
    String* cssFamilyName, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::PlatformFontUsage> >& platformFonts)
{
    Node* node = m_domAgent->assertNode(errorString, nodeId);
    if (!node)
        return;

    RefPtr<CSSComputedStyleDeclaration> computedStyleInfo = CSSComputedStyleDeclaration::create(node, true);
    *cssFamilyName = computedStyleInfo->getPropertyValue(CSSPropertyFontFamily);

    Vector<Node*> textNodes;
    if (node->nodeType() == Node::TEXT_NODE) {
        if (node->renderer())
            textNodes.append(node);
    } else {
        for (Node* child = node->firstChild(); child; child = child->nextSibling()) {
            if (child->nodeType() == Node::TEXT_NODE && child->renderer())
                textNodes.append(child);
        }
    }

    HashMap<String, int> fontStats;
    for (size_t i = 0; i < textNodes.size(); ++i) {
        RenderText* renderer = toRenderText(textNodes[i]->renderer());
        collectPlatformFontsForRenderer(renderer, &fontStats);
        if (renderer->isTextFragment()) {
            RenderTextFragment* textFragment = toRenderTextFragment(renderer);
            if (textFragment->firstLetter()) {
                RenderObject* firstLetter = textFragment->firstLetter();
                for (RenderObject* current = firstLetter->firstChild(); current; current = current->nextSibling()) {
                    if (current->isText())
                        collectPlatformFontsForRenderer(toRenderText(current), &fontStats);
                }
            }
        }
    }

    platformFonts = TypeBuilder::Array<TypeBuilder::CSS::PlatformFontUsage>::create();
    for (HashMap<String, int>::iterator it = fontStats.begin(), end = fontStats.end(); it != end; ++it) {
        RefPtr<TypeBuilder::CSS::PlatformFontUsage> platformFont = TypeBuilder::CSS::PlatformFontUsage::create()
            .setFamilyName(it->key)
            .setGlyphCount(it->value);
        platformFonts->addItem(platformFont);
    }
}

void InspectorCSSAgent::getAllStyleSheets(ErrorString*, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSStyleSheetHeader> >& styleInfos)
{
    styleInfos = TypeBuilder::Array<TypeBuilder::CSS::CSSStyleSheetHeader>::create();
    Vector<InspectorStyleSheet*> styleSheets;
    collectAllStyleSheets(styleSheets);
    for (size_t i = 0; i < styleSheets.size(); ++i)
        styleInfos->addItem(styleSheets.at(i)->buildObjectForStyleSheetInfo());
}

void InspectorCSSAgent::getStyleSheet(ErrorString* errorString, const String& styleSheetId, RefPtr<TypeBuilder::CSS::CSSStyleSheetBody>& styleSheetObject)
{
    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet)
        return;

    Document* doc = inspectorStyleSheet->pageStyleSheet() ? inspectorStyleSheet->pageStyleSheet()->ownerDocument() : 0;
    if (!doc || !doc->styleResolver())
        return;

    RefPtr<TypeBuilder::CSS::CSSStyleSheetBody> result = TypeBuilder::CSS::CSSStyleSheetBody::create()
        .setStyleSheetId(styleSheetId)
        .setRules(buildArrayForRuleList(inspectorStyleSheet->pageStyleSheet()->rules().get(), doc->styleResolver()));

    bool success = inspectorStyleSheet->fillObjectForStyleSheet(result);
    if (success)
        styleSheetObject = result;
}

void InspectorCSSAgent::getStyleSheetText(ErrorString* errorString, const String& styleSheetId, String* result)
{
    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet)
        return;

    inspectorStyleSheet->getText(result);
}

void InspectorCSSAgent::setStyleSheetText(ErrorString* errorString, const String& styleSheetId, const String& text)
{
    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, styleSheetId);
    if (!inspectorStyleSheet)
        return;

    TrackExceptionState es;
    m_domAgent->history()->perform(adoptPtr(new SetStyleSheetTextAction(inspectorStyleSheet, text)), es);
    *errorString = InspectorDOMAgent::toErrorString(es);
}

void InspectorCSSAgent::setStyleText(ErrorString* errorString, const RefPtr<JSONObject>& fullStyleId, const String& text, RefPtr<TypeBuilder::CSS::CSSStyle>& result)
{
    InspectorCSSId compoundId(fullStyleId);
    ASSERT(!compoundId.isEmpty());

    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, compoundId.styleSheetId());
    if (!inspectorStyleSheet)
        return;

    TrackExceptionState es;
    m_domAgent->history()->perform(adoptPtr(new SetStyleTextAction(inspectorStyleSheet, compoundId, text)), es);
    if (!es.hadException())
        result = inspectorStyleSheet->buildObjectForStyle(inspectorStyleSheet->styleForId(compoundId));
    *errorString = InspectorDOMAgent::toErrorString(es);
}

void InspectorCSSAgent::setPropertyText(ErrorString* errorString, const RefPtr<JSONObject>& fullStyleId, int propertyIndex, const String& text, bool overwrite, RefPtr<TypeBuilder::CSS::CSSStyle>& result)
{
    InspectorCSSId compoundId(fullStyleId);
    ASSERT(!compoundId.isEmpty());

    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, compoundId.styleSheetId());
    if (!inspectorStyleSheet)
        return;

    TrackExceptionState es;
    bool success = m_domAgent->history()->perform(adoptPtr(new SetPropertyTextAction(inspectorStyleSheet, compoundId, propertyIndex, text, overwrite)), es);
    if (success)
        result = inspectorStyleSheet->buildObjectForStyle(inspectorStyleSheet->styleForId(compoundId));
    *errorString = InspectorDOMAgent::toErrorString(es);
}

void InspectorCSSAgent::toggleProperty(ErrorString* errorString, const RefPtr<JSONObject>& fullStyleId, int propertyIndex, bool disable, RefPtr<TypeBuilder::CSS::CSSStyle>& result)
{
    InspectorCSSId compoundId(fullStyleId);
    ASSERT(!compoundId.isEmpty());

    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, compoundId.styleSheetId());
    if (!inspectorStyleSheet)
        return;

    TrackExceptionState es;
    bool success = m_domAgent->history()->perform(adoptPtr(new TogglePropertyAction(inspectorStyleSheet, compoundId, propertyIndex, disable)), es);
    if (success)
        result = inspectorStyleSheet->buildObjectForStyle(inspectorStyleSheet->styleForId(compoundId));
    *errorString = InspectorDOMAgent::toErrorString(es);
}

void InspectorCSSAgent::setRuleSelector(ErrorString* errorString, const RefPtr<JSONObject>& fullRuleId, const String& selector, RefPtr<TypeBuilder::CSS::CSSRule>& result)
{
    InspectorCSSId compoundId(fullRuleId);
    ASSERT(!compoundId.isEmpty());

    InspectorStyleSheet* inspectorStyleSheet = assertStyleSheetForId(errorString, compoundId.styleSheetId());
    if (!inspectorStyleSheet)
        return;

    TrackExceptionState es;
    bool success = m_domAgent->history()->perform(adoptPtr(new SetRuleSelectorAction(inspectorStyleSheet, compoundId, selector)), es);

    if (success) {
        CSSStyleRule* rule = inspectorStyleSheet->ruleForId(compoundId);
        result = inspectorStyleSheet->buildObjectForRule(rule, buildMediaListChain(rule));
    }
    *errorString = InspectorDOMAgent::toErrorString(es);
}

void InspectorCSSAgent::addRule(ErrorString* errorString, const int contextNodeId, const String& selector, RefPtr<TypeBuilder::CSS::CSSRule>& result)
{
    Node* node = m_domAgent->assertNode(errorString, contextNodeId);
    if (!node)
        return;

    InspectorStyleSheet* inspectorStyleSheet = viaInspectorStyleSheet(&node->document(), true);
    if (!inspectorStyleSheet) {
        *errorString = "No target stylesheet found";
        return;
    }

    TrackExceptionState es;
    OwnPtr<AddRuleAction> action = adoptPtr(new AddRuleAction(inspectorStyleSheet, selector));
    AddRuleAction* rawAction = action.get();
    bool success = m_domAgent->history()->perform(action.release(), es);
    if (!success) {
        *errorString = InspectorDOMAgent::toErrorString(es);
        return;
    }

    InspectorCSSId ruleId = rawAction->newRuleId();
    CSSStyleRule* rule = inspectorStyleSheet->ruleForId(ruleId);
    result = inspectorStyleSheet->buildObjectForRule(rule, buildMediaListChain(rule));
}

void InspectorCSSAgent::getSupportedCSSProperties(ErrorString*, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSPropertyInfo> >& cssProperties)
{
    RefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSPropertyInfo> > properties = TypeBuilder::Array<TypeBuilder::CSS::CSSPropertyInfo>::create();
    for (int i = firstCSSProperty; i <= lastCSSProperty; ++i) {
        CSSPropertyID id = convertToCSSPropertyID(i);
        RefPtr<TypeBuilder::CSS::CSSPropertyInfo> property = TypeBuilder::CSS::CSSPropertyInfo::create()
            .setName(getPropertyNameString(id));

        const StylePropertyShorthand& shorthand = shorthandForProperty(id);
        if (!shorthand.length()) {
            properties->addItem(property.release());
            continue;
        }
        RefPtr<TypeBuilder::Array<String> > longhands = TypeBuilder::Array<String>::create();
        for (unsigned j = 0; j < shorthand.length(); ++j) {
            CSSPropertyID longhandID = shorthand.properties()[j];
            longhands->addItem(getPropertyNameString(longhandID));
        }
        property->setLonghands(longhands);
        properties->addItem(property.release());
    }
    cssProperties = properties.release();
}

void InspectorCSSAgent::forcePseudoState(ErrorString* errorString, int nodeId, const RefPtr<JSONArray>& forcedPseudoClasses)
{
    Element* element = m_domAgent->assertElement(errorString, nodeId);
    if (!element)
        return;

    unsigned forcedPseudoState = computePseudoClassMask(forcedPseudoClasses.get());
    NodeIdToForcedPseudoState::iterator it = m_nodeIdToForcedPseudoState.find(nodeId);
    unsigned currentForcedPseudoState = it == m_nodeIdToForcedPseudoState.end() ? 0 : it->value;
    bool needStyleRecalc = forcedPseudoState != currentForcedPseudoState;
    if (!needStyleRecalc)
        return;

    if (forcedPseudoState)
        m_nodeIdToForcedPseudoState.set(nodeId, forcedPseudoState);
    else
        m_nodeIdToForcedPseudoState.remove(nodeId);
    element->ownerDocument()->setNeedsStyleRecalc();
}

void InspectorCSSAgent::getNamedFlowCollection(ErrorString* errorString, int documentNodeId, RefPtr<TypeBuilder::Array<TypeBuilder::CSS::NamedFlow> >& result)
{
    Document* document = m_domAgent->assertDocument(errorString, documentNodeId);
    if (!document)
        return;

    m_namedFlowCollectionsRequested.add(documentNodeId);

    Vector<RefPtr<NamedFlow> > namedFlowsVector = document->namedFlows()->namedFlows();
    RefPtr<TypeBuilder::Array<TypeBuilder::CSS::NamedFlow> > namedFlows = TypeBuilder::Array<TypeBuilder::CSS::NamedFlow>::create();

    for (Vector<RefPtr<NamedFlow> >::iterator it = namedFlowsVector.begin(); it != namedFlowsVector.end(); ++it)
        namedFlows->addItem(buildObjectForNamedFlow(errorString, it->get(), documentNodeId));

    result = namedFlows.release();
}

PassRefPtr<TypeBuilder::CSS::CSSMedia> InspectorCSSAgent::buildMediaObject(const MediaList* media, MediaListSource mediaListSource, const String& sourceURL, CSSStyleSheet* parentStyleSheet)
{
    // Make certain compilers happy by initializing |source| up-front.
    TypeBuilder::CSS::CSSMedia::Source::Enum source = TypeBuilder::CSS::CSSMedia::Source::InlineSheet;
    switch (mediaListSource) {
    case MediaListSourceMediaRule:
        source = TypeBuilder::CSS::CSSMedia::Source::MediaRule;
        break;
    case MediaListSourceImportRule:
        source = TypeBuilder::CSS::CSSMedia::Source::ImportRule;
        break;
    case MediaListSourceLinkedSheet:
        source = TypeBuilder::CSS::CSSMedia::Source::LinkedSheet;
        break;
    case MediaListSourceInlineSheet:
        source = TypeBuilder::CSS::CSSMedia::Source::InlineSheet;
        break;
    }

    RefPtr<TypeBuilder::CSS::CSSMedia> mediaObject = TypeBuilder::CSS::CSSMedia::create()
        .setText(media->mediaText())
        .setSource(source);

    if (parentStyleSheet && mediaListSource != MediaListSourceLinkedSheet) {
        if (InspectorStyleSheet* inspectorStyleSheet = m_cssStyleSheetToInspectorStyleSheet.get(parentStyleSheet))
            mediaObject->setParentStyleSheetId(inspectorStyleSheet->id());
    }
    if (!sourceURL.isEmpty()) {
        mediaObject->setSourceURL(sourceURL);

        CSSRule* parentRule = media->parentRule();
        if (!parentRule)
            return mediaObject.release();
        InspectorStyleSheet* inspectorStyleSheet = bindStyleSheet(parentRule->parentStyleSheet());
        RefPtr<TypeBuilder::CSS::SourceRange> mediaRange = inspectorStyleSheet->ruleHeaderSourceRange(parentRule);
        if (mediaRange)
            mediaObject->setRange(mediaRange);
    }
    return mediaObject.release();
}

PassRefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSMedia> > InspectorCSSAgent::buildMediaListChain(CSSRule* rule)
{
    if (!rule)
        return 0;
    RefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSMedia> > mediaArray = TypeBuilder::Array<TypeBuilder::CSS::CSSMedia>::create();
    bool hasItems = false;
    MediaList* mediaList;
    CSSRule* parentRule = rule;
    String sourceURL;
    while (parentRule) {
        CSSStyleSheet* parentStyleSheet = 0;
        bool isMediaRule = true;
        if (parentRule->type() == CSSRule::MEDIA_RULE) {
            CSSMediaRule* mediaRule = static_cast<CSSMediaRule*>(parentRule);
            mediaList = mediaRule->media();
            parentStyleSheet = mediaRule->parentStyleSheet();
        } else if (parentRule->type() == CSSRule::IMPORT_RULE) {
            CSSImportRule* importRule = toCSSImportRule(parentRule);
            mediaList = importRule->media();
            parentStyleSheet = importRule->parentStyleSheet();
            isMediaRule = false;
        } else {
            mediaList = 0;
        }

        if (parentStyleSheet) {
            sourceURL = parentStyleSheet->contents()->baseURL();
            if (sourceURL.isEmpty())
                sourceURL = InspectorDOMAgent::documentURLString(parentStyleSheet->ownerDocument());
        } else {
            sourceURL = "";
        }

        if (mediaList && mediaList->length()) {
            mediaArray->addItem(buildMediaObject(mediaList, isMediaRule ? MediaListSourceMediaRule : MediaListSourceImportRule, sourceURL, parentStyleSheet));
            hasItems = true;
        }

        if (parentRule->parentRule()) {
            parentRule = parentRule->parentRule();
        } else {
            CSSStyleSheet* styleSheet = parentRule->parentStyleSheet();
            while (styleSheet) {
                mediaList = styleSheet->media();
                if (mediaList && mediaList->length()) {
                    Document* doc = styleSheet->ownerDocument();
                    if (doc)
                        sourceURL = doc->url();
                    else if (!styleSheet->contents()->baseURL().isEmpty())
                        sourceURL = styleSheet->contents()->baseURL();
                    else
                        sourceURL = "";
                    mediaArray->addItem(buildMediaObject(mediaList, styleSheet->ownerNode() ? MediaListSourceLinkedSheet : MediaListSourceInlineSheet, sourceURL, styleSheet));
                    hasItems = true;
                }
                parentRule = styleSheet->ownerRule();
                if (parentRule)
                    break;
                styleSheet = styleSheet->parentStyleSheet();
            }
        }
    }
    return hasItems ? mediaArray : 0;
}

InspectorStyleSheetForInlineStyle* InspectorCSSAgent::asInspectorStyleSheet(Element* element)
{
    NodeToInspectorStyleSheet::iterator it = m_nodeToInspectorStyleSheet.find(element);
    if (it != m_nodeToInspectorStyleSheet.end())
        return it->value.get();

    CSSStyleDeclaration* style = element->isStyledElement() ? element->style() : 0;
    if (!style)
        return 0;

    String newStyleSheetId = String::number(m_lastStyleSheetId++);
    RefPtr<InspectorStyleSheetForInlineStyle> inspectorStyleSheet = InspectorStyleSheetForInlineStyle::create(m_pageAgent, m_resourceAgent, newStyleSheetId, element, TypeBuilder::CSS::StyleSheetOrigin::Regular, this);
    m_idToInspectorStyleSheet.set(newStyleSheetId, inspectorStyleSheet);
    m_nodeToInspectorStyleSheet.set(element, inspectorStyleSheet);
    return inspectorStyleSheet.get();
}

Element* InspectorCSSAgent::elementForId(ErrorString* errorString, int nodeId)
{
    Node* node = m_domAgent->nodeForId(nodeId);
    if (!node) {
        *errorString = "No node with given id found";
        return 0;
    }
    if (!node->isElementNode()) {
        *errorString = "Not an element node";
        return 0;
    }
    return toElement(node);
}

int InspectorCSSAgent::documentNodeWithRequestedFlowsId(Document* document)
{
    int documentNodeId = m_domAgent->boundNodeId(document);
    if (!documentNodeId || !m_namedFlowCollectionsRequested.contains(documentNodeId))
        return 0;

    return documentNodeId;
}

void InspectorCSSAgent::collectAllStyleSheets(Vector<InspectorStyleSheet*>& result)
{
    Vector<Document*> documents = m_domAgent->documents();
    for (Vector<Document*>::iterator it = documents.begin(); it != documents.end(); ++it) {
        StyleSheetList* list = (*it)->styleSheets();
        for (unsigned i = 0; i < list->length(); ++i) {
            StyleSheet* styleSheet = list->item(i);
            if (styleSheet->isCSSStyleSheet())
                collectStyleSheets(static_cast<CSSStyleSheet*>(styleSheet), result);
        }
    }
}

void InspectorCSSAgent::collectStyleSheets(CSSStyleSheet* styleSheet, Vector<InspectorStyleSheet*>& result)
{
    InspectorStyleSheet* inspectorStyleSheet = bindStyleSheet(static_cast<CSSStyleSheet*>(styleSheet));
    result.append(inspectorStyleSheet);
    for (unsigned i = 0, size = styleSheet->length(); i < size; ++i) {
        CSSRule* rule = styleSheet->item(i);
        if (rule->type() == CSSRule::IMPORT_RULE) {
            CSSStyleSheet* importedStyleSheet = toCSSImportRule(rule)->styleSheet();
            if (importedStyleSheet)
                collectStyleSheets(importedStyleSheet, result);
        }
    }
}

InspectorStyleSheet* InspectorCSSAgent::bindStyleSheet(CSSStyleSheet* styleSheet)
{
    RefPtr<InspectorStyleSheet> inspectorStyleSheet = m_cssStyleSheetToInspectorStyleSheet.get(styleSheet);
    if (!inspectorStyleSheet) {
        String id = String::number(m_lastStyleSheetId++);
        Document* document = styleSheet->ownerDocument();
        inspectorStyleSheet = InspectorStyleSheet::create(m_pageAgent, m_resourceAgent, id, styleSheet, detectOrigin(styleSheet, document), InspectorDOMAgent::documentURLString(document), this);
        m_idToInspectorStyleSheet.set(id, inspectorStyleSheet);
        m_cssStyleSheetToInspectorStyleSheet.set(styleSheet, inspectorStyleSheet);
        if (m_creatingViaInspectorStyleSheet)
            m_documentToInspectorStyleSheet.add(document, inspectorStyleSheet);
    }
    return inspectorStyleSheet.get();
}

String InspectorCSSAgent::unbindStyleSheet(InspectorStyleSheet* inspectorStyleSheet)
{
    String id = inspectorStyleSheet->id();
    m_idToInspectorStyleSheet.remove(id);
    if (inspectorStyleSheet->pageStyleSheet())
        m_cssStyleSheetToInspectorStyleSheet.remove(inspectorStyleSheet->pageStyleSheet());
    return id;
}

InspectorStyleSheet* InspectorCSSAgent::viaInspectorStyleSheet(Document* document, bool createIfAbsent)
{
    if (!document) {
        ASSERT(!createIfAbsent);
        return 0;
    }

    if (!document->isHTMLDocument() && !document->isSVGDocument())
        return 0;

    RefPtr<InspectorStyleSheet> inspectorStyleSheet = m_documentToInspectorStyleSheet.get(document);
    if (inspectorStyleSheet || !createIfAbsent)
        return inspectorStyleSheet.get();

    TrackExceptionState es;
    RefPtr<Element> styleElement = document->createElement("style", es);
    if (!es.hadException())
        styleElement->setAttribute("type", "text/css", es);
    if (!es.hadException()) {
        ContainerNode* targetNode;
        // HEAD is absent in ImageDocuments, for example.
        if (document->head())
            targetNode = document->head();
        else if (document->body())
            targetNode = document->body();
        else
            return 0;

        InlineStyleOverrideScope overrideScope(document);
        m_creatingViaInspectorStyleSheet = true;
        targetNode->appendChild(styleElement, es);
        // At this point the added stylesheet will get bound through the updateActiveStyleSheets() invocation.
        // We just need to pick the respective InspectorStyleSheet from m_documentToInspectorStyleSheet.
        m_creatingViaInspectorStyleSheet = false;
    }
    if (es.hadException())
        return 0;

    return m_documentToInspectorStyleSheet.get(document);
}

InspectorStyleSheet* InspectorCSSAgent::assertStyleSheetForId(ErrorString* errorString, const String& styleSheetId)
{
    IdToInspectorStyleSheet::iterator it = m_idToInspectorStyleSheet.find(styleSheetId);
    if (it == m_idToInspectorStyleSheet.end()) {
        *errorString = "No style sheet with given id found";
        return 0;
    }
    return it->value.get();
}

TypeBuilder::CSS::StyleSheetOrigin::Enum InspectorCSSAgent::detectOrigin(CSSStyleSheet* pageStyleSheet, Document* ownerDocument)
{
    if (m_creatingViaInspectorStyleSheet)
        return TypeBuilder::CSS::StyleSheetOrigin::Inspector;

    TypeBuilder::CSS::StyleSheetOrigin::Enum origin = TypeBuilder::CSS::StyleSheetOrigin::Regular;
    if (pageStyleSheet && !pageStyleSheet->ownerNode() && pageStyleSheet->href().isEmpty())
        origin = TypeBuilder::CSS::StyleSheetOrigin::User_agent;
    else if (pageStyleSheet && pageStyleSheet->ownerNode() && pageStyleSheet->ownerNode()->nodeName() == "#document")
        origin = TypeBuilder::CSS::StyleSheetOrigin::User;
    else {
        InspectorStyleSheet* viaInspectorStyleSheetForOwner = viaInspectorStyleSheet(ownerDocument, false);
        if (viaInspectorStyleSheetForOwner && pageStyleSheet == viaInspectorStyleSheetForOwner->pageStyleSheet())
            origin = TypeBuilder::CSS::StyleSheetOrigin::Inspector;
    }
    return origin;
}

PassRefPtr<TypeBuilder::CSS::CSSRule> InspectorCSSAgent::buildObjectForRule(CSSStyleRule* rule, StyleResolver* styleResolver)
{
    if (!rule)
        return 0;

    // CSSRules returned by StyleResolver::styleRulesForElement lack parent pointers since that infomation is not cheaply available.
    // Since the inspector wants to walk the parent chain, we construct the full wrappers here.
    // FIXME: This could be factored better. StyleResolver::styleRulesForElement should return a StyleRule vector, not a CSSRuleList.
    if (!rule->parentStyleSheet()) {
        rule = styleResolver->inspectorCSSOMWrappers().getWrapperForRuleInSheets(rule->styleRule(), styleResolver->document().styleEngine());
        if (!rule)
            return 0;
    }
    return bindStyleSheet(rule->parentStyleSheet())->buildObjectForRule(rule, buildMediaListChain(rule));
}

PassRefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSRule> > InspectorCSSAgent::buildArrayForRuleList(CSSRuleList* ruleList, StyleResolver* styleResolver)
{
    RefPtr<TypeBuilder::Array<TypeBuilder::CSS::CSSRule> > result = TypeBuilder::Array<TypeBuilder::CSS::CSSRule>::create();
    if (!ruleList)
        return result.release();

    RefPtr<CSSRuleList> refRuleList = ruleList;
    CSSRuleVector rules;
    InspectorStyleSheet::collectFlatRules(refRuleList, &rules);

    for (unsigned i = 0, size = rules.size(); i < size; ++i) {
        CSSStyleRule* styleRule = asCSSStyleRule(rules.at(i).get());
        if (!styleRule)
            continue;
        result->addItem(buildObjectForRule(styleRule, styleResolver));
    }

    return result.release();
}

static inline bool matchesPseudoElement(const CSSSelector* selector, PseudoId elementPseudoId)
{
    // According to http://www.w3.org/TR/css3-selectors/#pseudo-elements, "Only one pseudo-element may appear per selector."
    // As such, check the last selector in the tag history.
    for (; !selector->isLastInTagHistory(); ++selector) { }
    PseudoId selectorPseudoId = selector->matchesPseudoElement() ? CSSSelector::pseudoId(selector->pseudoType()) : NOPSEUDO;

    // FIXME: This only covers the case of matching pseudo-element selectors against PseudoElements.
    // We should come up with a solution for matching pseudo-element selectors against ordinary Elements, too.
    return selectorPseudoId == elementPseudoId;
}

PassRefPtr<TypeBuilder::Array<TypeBuilder::CSS::RuleMatch> > InspectorCSSAgent::buildArrayForMatchedRuleList(CSSRuleList* ruleList, StyleResolver* styleResolver, Element* element)
{
    RefPtr<TypeBuilder::Array<TypeBuilder::CSS::RuleMatch> > result = TypeBuilder::Array<TypeBuilder::CSS::RuleMatch>::create();
    if (!ruleList)
        return result.release();

    for (unsigned i = 0, size = ruleList->length(); i < size; ++i) {
        CSSStyleRule* rule = asCSSStyleRule(ruleList->item(i));
        RefPtr<TypeBuilder::CSS::CSSRule> ruleObject = buildObjectForRule(rule, styleResolver);
        if (!ruleObject)
            continue;
        RefPtr<TypeBuilder::Array<int> > matchingSelectors = TypeBuilder::Array<int>::create();
        const CSSSelectorList& selectorList = rule->styleRule()->selectorList();
        long index = 0;
        PseudoId elementPseudoId = element->pseudoId();
        for (const CSSSelector* selector = selectorList.first(); selector; selector = CSSSelectorList::next(selector)) {
            const CSSSelector* firstTagHistorySelector = selector;
            bool matched = false;
            if (elementPseudoId)
                matched = matchesPseudoElement(selector, elementPseudoId); // Modifies |selector|.
            matched |= element->webkitMatchesSelector(firstTagHistorySelector->selectorText(), IGNORE_EXCEPTION);
            if (matched)
                matchingSelectors->addItem(index);
            ++index;
        }
        RefPtr<TypeBuilder::CSS::RuleMatch> match = TypeBuilder::CSS::RuleMatch::create()
            .setRule(ruleObject.release())
            .setMatchingSelectors(matchingSelectors.release());
        result->addItem(match);
    }

    return result;
}

PassRefPtr<TypeBuilder::CSS::CSSStyle> InspectorCSSAgent::buildObjectForAttributesStyle(Element* element)
{
    if (!element->isStyledElement())
        return 0;

    // FIXME: Ugliness below.
    StylePropertySet* attributeStyle = const_cast<StylePropertySet*>(element->presentationAttributeStyle());
    if (!attributeStyle)
        return 0;

    ASSERT(attributeStyle->isMutable());
    MutableStylePropertySet* mutableAttributeStyle = static_cast<MutableStylePropertySet*>(attributeStyle);

    RefPtr<InspectorStyle> inspectorStyle = InspectorStyle::create(InspectorCSSId(), mutableAttributeStyle->ensureCSSStyleDeclaration(), 0);
    return inspectorStyle->buildObjectForStyle();
}

PassRefPtr<TypeBuilder::Array<TypeBuilder::CSS::Region> > InspectorCSSAgent::buildArrayForRegions(ErrorString* errorString, PassRefPtr<NodeList> regionList, int documentNodeId)
{
    RefPtr<TypeBuilder::Array<TypeBuilder::CSS::Region> > regions = TypeBuilder::Array<TypeBuilder::CSS::Region>::create();

    for (unsigned i = 0; i < regionList->length(); ++i) {
        TypeBuilder::CSS::Region::RegionOverset::Enum regionOverset;

        switch (toElement(regionList->item(i))->renderRegion()->regionOversetState()) {
        case RegionFit:
            regionOverset = TypeBuilder::CSS::Region::RegionOverset::Fit;
            break;
        case RegionEmpty:
            regionOverset = TypeBuilder::CSS::Region::RegionOverset::Empty;
            break;
        case RegionOverset:
            regionOverset = TypeBuilder::CSS::Region::RegionOverset::Overset;
            break;
        case RegionUndefined:
            continue;
        default:
            ASSERT_NOT_REACHED();
            continue;
        }

        RefPtr<TypeBuilder::CSS::Region> region = TypeBuilder::CSS::Region::create()
            .setRegionOverset(regionOverset)
            // documentNodeId was previously asserted
            .setNodeId(m_domAgent->pushNodeToFrontend(errorString, documentNodeId, regionList->item(i)));

        regions->addItem(region);
    }

    return regions.release();
}

PassRefPtr<TypeBuilder::CSS::NamedFlow> InspectorCSSAgent::buildObjectForNamedFlow(ErrorString* errorString, NamedFlow* webkitNamedFlow, int documentNodeId)
{
    RefPtr<NodeList> contentList = webkitNamedFlow->getContent();
    RefPtr<TypeBuilder::Array<int> > content = TypeBuilder::Array<int>::create();

    for (unsigned i = 0; i < contentList->length(); ++i) {
        // documentNodeId was previously asserted
        content->addItem(m_domAgent->pushNodeToFrontend(errorString, documentNodeId, contentList->item(i)));
    }

    RefPtr<TypeBuilder::CSS::NamedFlow> namedFlow = TypeBuilder::CSS::NamedFlow::create()
        .setDocumentNodeId(documentNodeId)
        .setName(webkitNamedFlow->name().string())
        .setOverset(webkitNamedFlow->overset())
        .setContent(content)
        .setRegions(buildArrayForRegions(errorString, webkitNamedFlow->getRegions(), documentNodeId));

    return namedFlow.release();
}

void InspectorCSSAgent::didRemoveDocument(Document* document)
{
    if (document)
        m_documentToInspectorStyleSheet.remove(document);
}

void InspectorCSSAgent::didRemoveDOMNode(Node* node)
{
    if (!node)
        return;

    int nodeId = m_domAgent->boundNodeId(node);
    if (nodeId)
        m_nodeIdToForcedPseudoState.remove(nodeId);

    NodeToInspectorStyleSheet::iterator it = m_nodeToInspectorStyleSheet.find(node);
    if (it == m_nodeToInspectorStyleSheet.end())
        return;

    m_idToInspectorStyleSheet.remove(it->value->id());
    m_nodeToInspectorStyleSheet.remove(node);
}

void InspectorCSSAgent::didModifyDOMAttr(Element* element)
{
    if (!element)
        return;

    NodeToInspectorStyleSheet::iterator it = m_nodeToInspectorStyleSheet.find(element);
    if (it == m_nodeToInspectorStyleSheet.end())
        return;

    it->value->didModifyElementAttribute();
}

void InspectorCSSAgent::styleSheetChanged(InspectorStyleSheet* styleSheet)
{
    if (m_frontend)
        m_frontend->styleSheetChanged(styleSheet->id());
}

void InspectorCSSAgent::willReparseStyleSheet()
{
    ASSERT(!m_isSettingStyleSheetText);
    m_isSettingStyleSheetText = true;
}

void InspectorCSSAgent::didReparseStyleSheet()
{
    ASSERT(m_isSettingStyleSheetText);
    m_isSettingStyleSheetText = false;
}

void InspectorCSSAgent::resetPseudoStates()
{
    HashSet<Document*> documentsToChange;
    for (NodeIdToForcedPseudoState::iterator it = m_nodeIdToForcedPseudoState.begin(), end = m_nodeIdToForcedPseudoState.end(); it != end; ++it) {
        Element* element = toElement(m_domAgent->nodeForId(it->key));
        if (element && element->ownerDocument())
            documentsToChange.add(element->ownerDocument());
    }

    m_nodeIdToForcedPseudoState.clear();
    for (HashSet<Document*>::iterator it = documentsToChange.begin(), end = documentsToChange.end(); it != end; ++it)
        (*it)->setNeedsStyleRecalc();
}

} // namespace WebCore

