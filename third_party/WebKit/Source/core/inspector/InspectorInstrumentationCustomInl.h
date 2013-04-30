/*
* Copyright (C) 2013 Google Inc. All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are
* met:
*
*     * Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*     * Redistributions in binary form must reproduce the above
* copyright notice, this list of conditions and the following disclaimer
* in the documentation and/or other materials provided with the
* distribution.
*     * Neither the name of Google Inc. nor the names of its
* contributors may be used to endorse or promote products derived from
* this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
* "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
* LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
* A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
* OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
* LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
* DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
* THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef InspectorInstrumentationCustom_inl_h
#define InspectorInstrumentationCustom_inl_h

namespace WebCore {

namespace InspectorInstrumentation {

bool profilerEnabledImpl(InstrumentingAgents*);
bool isDebuggerPausedImpl(InstrumentingAgents*);
void willRemoveDOMNodeImpl(InstrumentingAgents*, Node*);
void didRemoveDOMNodeImpl(InstrumentingAgents*, Node*);
void didPushShadowRootImpl(InstrumentingAgents*, Element* host, ShadowRoot*);
void willPopShadowRootImpl(InstrumentingAgents*, Element* host, ShadowRoot*);
InspectorInstrumentationCookie willProcessRuleImpl(InstrumentingAgents*, StyleRule*, StyleResolver*);
void continueAfterXFrameOptionsDeniedImpl(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
void continueWithPolicyDownloadImpl(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
void continueWithPolicyIgnoreImpl(Frame*, DocumentLoader*, unsigned long identifier, const ResourceResponse&);
void willDestroyCachedResourceImpl(CachedResource*);
void didDispatchDOMStorageEventImpl(InstrumentingAgents*, const String& key, const String& oldValue, const String& newValue, StorageType, SecurityOrigin*, Page*);
bool collectingHTMLParseErrorsImpl(InstrumentingAgents*);

bool canvasAgentEnabled(ScriptExecutionContext*);
bool consoleAgentEnabled(ScriptExecutionContext*);
bool timelineAgentEnabled(ScriptExecutionContext*);
void willEvaluateWorkerScript(WorkerContext*, int workerThreadStartMode);

inline bool profilerEnabled(Page* page)
{
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        return profilerEnabledImpl(instrumentingAgents);
    return false;
}

inline bool isDebuggerPaused(Frame* frame)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForFrame(frame))
        return isDebuggerPausedImpl(instrumentingAgents);
    return false;
}


inline void willRemoveDOMNode(Document* document, Node* node)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document)) {
        willRemoveDOMNodeImpl(instrumentingAgents, node);
        didRemoveDOMNodeImpl(instrumentingAgents, node);
    }
}

inline void didPushShadowRoot(Element* host, ShadowRoot* root)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(host->ownerDocument()))
        didPushShadowRootImpl(instrumentingAgents, host, root);
}

inline void willPopShadowRoot(Element* host, ShadowRoot* root)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(host->ownerDocument()))
        willPopShadowRootImpl(instrumentingAgents, host, root);
}

inline InspectorInstrumentationCookie willProcessRule(Document* document, StyleRule* rule, StyleResolver* styleResolver)
{
    FAST_RETURN_IF_NO_FRONTENDS(InspectorInstrumentationCookie());
    if (!rule)
        return InspectorInstrumentationCookie();
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForDocument(document))
        return willProcessRuleImpl(instrumentingAgents, rule, styleResolver);
    return InspectorInstrumentationCookie();
}

inline void continueAfterXFrameOptionsDenied(Frame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    continueAfterXFrameOptionsDeniedImpl(frame, loader, identifier, r);
}

inline void continueWithPolicyDownload(Frame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    continueWithPolicyDownloadImpl(frame, loader, identifier, r);
}

inline void continueWithPolicyIgnore(Frame* frame, DocumentLoader* loader, unsigned long identifier, const ResourceResponse& r)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    continueWithPolicyIgnoreImpl(frame, loader, identifier, r);
}

inline void willDestroyCachedResource(CachedResource* cachedResource)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    willDestroyCachedResourceImpl(cachedResource);
}

inline void didDispatchDOMStorageEvent(const String& key, const String& oldValue, const String& newValue, StorageType storageType, SecurityOrigin* securityOrigin, Page* page)
{
    FAST_RETURN_IF_NO_FRONTENDS(void());
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        didDispatchDOMStorageEventImpl(instrumentingAgents, key, oldValue, newValue, storageType, securityOrigin, page);
}

inline bool collectingHTMLParseErrors(Page* page)
{
    FAST_RETURN_IF_NO_FRONTENDS(false);
    if (InstrumentingAgents* instrumentingAgents = instrumentingAgentsForPage(page))
        return collectingHTMLParseErrorsImpl(instrumentingAgents);
    return false;
}

} // namespace InspectorInstrumentation

} // namespace WebCore

#endif // !defined(InspectorInstrumentationCustom_inl_h)
