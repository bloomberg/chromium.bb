/*
 * Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
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
 *
 */

#ifndef ScriptLoader_h
#define ScriptLoader_h

#include "core/fetch/ResourceClient.h"
#include "core/fetch/ResourceOwner.h"
#include "core/fetch/ResourcePtr.h"
#include "core/fetch/ScriptResource.h"
#include "wtf/text/TextPosition.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class ScriptResource;
class ContainerNode;
class Document;
class Element;
class ScriptLoaderClient;
class ScriptRunner;
class ScriptSourceCode;

class ScriptPrep FINAL {
public:
    ScriptPrep(bool succeeded, const ResourcePtr<ScriptResource>& resource)
        : m_succeeded(succeeded)
        , m_resource(resource)
    { }

    bool succeeded() const { return m_succeeded; }
    ScriptResource* resource() const { return m_resource.get(); }

    static ScriptPrep failed() { return ScriptPrep(); }

private:
    ScriptPrep()
        : m_succeeded(false)
    { }

    bool m_succeeded;
    ResourcePtr<ScriptResource> m_resource;
};

class ScriptLoader FINAL : public ResourceOwner<ScriptResource> {
public:
    static PassOwnPtr<ScriptLoader> create(Element*, bool createdByParser, bool isEvaluated);
    virtual ~ScriptLoader();

    Element* element() const { return m_element; }

    enum LegacyTypeSupport { DisallowLegacyTypeInTypeAttribute, AllowLegacyTypeInTypeAttribute };
    ScriptPrep prepareScript(const TextPosition& scriptStartPosition = TextPosition::minimumPosition(), LegacyTypeSupport = DisallowLegacyTypeInTypeAttribute);

    String scriptCharset() const { return m_characterEncoding; }
    String scriptContent() const;
    void executeScript(const ScriptSourceCode&);
    void execute(ScriptResource*);

    // XML parser calls these
    void dispatchLoadEvent();
    void dispatchErrorEvent();
    bool isScriptTypeSupported(LegacyTypeSupport) const;

    bool haveFiredLoadEvent() const { return m_haveFiredLoad; }
    bool willBeParserExecuted() const { return m_willBeParserExecuted; }
    bool readyToBeParserExecuted() const { return m_readyToBeParserExecuted; }
    bool willExecuteWhenDocumentFinishedParsing() const { return m_willExecuteWhenDocumentFinishedParsing; }

    void setHaveFiredLoadEvent(bool haveFiredLoad) { m_haveFiredLoad = haveFiredLoad; }
    bool isParserInserted() const { return m_parserInserted; }
    bool alreadyStarted() const { return m_alreadyStarted; }
    bool forceAsync() const { return m_forceAsync; }

    // Helper functions used by our parent classes.
    void didNotifySubtreeInsertionsToDocument();
    void childrenChanged();
    void handleSourceAttribute(const String& sourceUrl);
    void handleAsyncAttribute();
    void cancel(Document* contextDocument);

private:
    ScriptLoader(Element*, bool createdByParser, bool isEvaluated);

    bool ignoresLoadRequest() const;
    bool isScriptForEventSupported() const;

    ResourcePtr<ScriptResource> fetchScript(const String& sourceUrl);

    ScriptLoaderClient* client() const;

    // ResourceClient
    virtual void notifyFinished(Resource*) OVERRIDE;

    enum FinishType {
        FinishSuccessfully,
        FinishWithCancel,
        FinishWithError
    };

    void finishLoading(Document* contextDocument, FinishType);
    void notifyRunnerFinishLoading(ScriptRunner*, FinishType);

    Element* m_element;
    WTF::OrdinalNumber m_startLineNumber;
    bool m_parserInserted : 1;
    bool m_isExternalScript : 1;
    bool m_alreadyStarted : 1;
    bool m_haveFiredLoad : 1;
    bool m_willBeParserExecuted : 1; // Same as "The parser will handle executing the script."
    bool m_readyToBeParserExecuted : 1;
    bool m_willExecuteWhenDocumentFinishedParsing : 1;
    bool m_forceAsync : 1;
    bool m_willExecuteInOrder : 1;
    String m_characterEncoding;
    String m_fallbackCharacterEncoding;
};

ScriptLoader* toScriptLoaderIfPossible(Element*);

inline PassOwnPtr<ScriptLoader> ScriptLoader::create(Element* element, bool createdByParser, bool isEvaluated)
{
    return adoptPtr(new ScriptLoader(element, createdByParser, isEvaluated));
}

}


#endif
