/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef V8PerIsolateData_h
#define V8PerIsolateData_h

#include "bindings/v8/ScopedPersistent.h"
#include "bindings/v8/UnsafePersistent.h"
#include "bindings/v8/WrapperTypeInfo.h"
#include "gin/public/gin_embedders.h"
#include "gin/public/isolate_holder.h"
#include <v8.h>
#include "wtf/Forward.h"
#include "wtf/HashMap.h"
#include "wtf/OwnPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

class DOMDataStore;
class GCEventData;
class StringCache;
struct WrapperTypeInfo;

class ExternalStringVisitor;

typedef WTF::Vector<DOMDataStore*> DOMDataStoreList;

class V8PerIsolateData {
public:
    static void ensureInitialized(v8::Isolate*);
    static V8PerIsolateData* current()
    {
        return from(v8::Isolate::GetCurrent());
    }
    static V8PerIsolateData* from(v8::Isolate* isolate)
    {
        ASSERT(isolate);
        ASSERT(isolate->GetData(gin::kEmbedderBlink));
        return static_cast<V8PerIsolateData*>(isolate->GetData(gin::kEmbedderBlink));
    }
    static void dispose(v8::Isolate*);
    static v8::Isolate* mainThreadIsolate();

    bool isMainThread() { return m_isMainThread; }
    v8::Isolate* isolate() { return m_isolate; }

    typedef HashMap<const void*, UnsafePersistent<v8::FunctionTemplate> > TemplateMap;

    TemplateMap& templateMap(WrapperWorldType worldType)
    {
        if (worldType == MainWorld)
            return m_templatesForMainWorld;
        return m_templatesForNonMainWorld;
    }

    v8::Handle<v8::FunctionTemplate> toStringTemplate();

    StringCache* stringCache() { return m_stringCache.get(); }

    v8::Persistent<v8::Value>& ensureLiveRoot();

    DOMDataStoreList& allStores() { return m_domDataStoreList; }

    void registerDOMDataStore(DOMDataStore* domDataStore)
    {
        ASSERT(m_domDataStoreList.find(domDataStore) == kNotFound);
        m_domDataStoreList.append(domDataStore);
    }

    void unregisterDOMDataStore(DOMDataStore* domDataStore)
    {
        ASSERT(m_domDataStoreList.find(domDataStore) != kNotFound);
        m_domDataStoreList.remove(m_domDataStoreList.find(domDataStore));
    }

    int recursionLevel() const { return m_recursionLevel; }
    int incrementRecursionLevel() { return ++m_recursionLevel; }
    int decrementRecursionLevel() { return --m_recursionLevel; }

    bool performingMicrotaskCheckpoint() const { return m_performingMicrotaskCheckpoint; }
    void setPerformingMicrotaskCheckpoint(bool performingMicrotaskCheckpoint) { m_performingMicrotaskCheckpoint = performingMicrotaskCheckpoint; }

#ifndef NDEBUG
    int internalScriptRecursionLevel() const { return m_internalScriptRecursionLevel; }
    int incrementInternalScriptRecursionLevel() { return ++m_internalScriptRecursionLevel; }
    int decrementInternalScriptRecursionLevel() { return --m_internalScriptRecursionLevel; }
#endif

    GCEventData* gcEventData() { return m_gcEventData.get(); }

    v8::Handle<v8::FunctionTemplate> domTemplate(WrapperWorldType, void* domTemplateKey, v8::FunctionCallback = 0, v8::Handle<v8::Value> data = v8::Handle<v8::Value>(), v8::Handle<v8::Signature> = v8::Handle<v8::Signature>(), int length = 0);
    v8::Handle<v8::FunctionTemplate> existingDOMTemplate(WrapperWorldType, void* domTemplateKey);
    void setDOMTemplate(WrapperWorldType, void* domTemplateKey, v8::Handle<v8::FunctionTemplate>);

    bool hasInstance(const WrapperTypeInfo*, v8::Handle<v8::Value>);
    v8::Handle<v8::Object> findInstanceInPrototypeChain(const WrapperTypeInfo*, v8::Handle<v8::Value>);

    v8::Local<v8::Context> ensureRegexContext();

    const char* previousSamplingState() const { return m_previousSamplingState; }
    void setPreviousSamplingState(const char* name) { m_previousSamplingState = name; }

private:
    explicit V8PerIsolateData(v8::Isolate*);
    ~V8PerIsolateData();
    bool hasInstance(const WrapperTypeInfo*, v8::Handle<v8::Value>, TemplateMap&);
    v8::Handle<v8::Object> findInstanceInPrototypeChain(const WrapperTypeInfo*, v8::Handle<v8::Value>, TemplateMap&);

    v8::Isolate* m_isolate;
    OwnPtr<gin::IsolateHolder> m_isolateHolder;
    bool m_isMainThread; // Caches the result of isMainThread() for performance.
    TemplateMap m_templatesForMainWorld;
    TemplateMap m_templatesForNonMainWorld;
    ScopedPersistent<v8::FunctionTemplate> m_toStringTemplate;
    OwnPtr<StringCache> m_stringCache;

    Vector<DOMDataStore*> m_domDataStoreList;

    ScopedPersistent<v8::Value> m_liveRoot;
    ScopedPersistent<v8::Context> m_regexContext;

    const char* m_previousSamplingState;

    bool m_constructorMode;
    friend class ConstructorMode;

    int m_recursionLevel;

#ifndef NDEBUG
    int m_internalScriptRecursionLevel;
#endif
    OwnPtr<GCEventData> m_gcEventData;
    bool m_performingMicrotaskCheckpoint;
};

} // namespace WebCore

#endif // V8PerIsolateData_h
