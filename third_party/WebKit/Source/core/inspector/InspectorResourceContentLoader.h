// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorResourceContentLoader_h
#define InspectorResourceContentLoader_h

#include "core/fetch/RawResource.h"
#include "core/fetch/StyleSheetResourceClient.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/Vector.h"

namespace WebCore {

class Page;
class RawResourceClient;
class StyleSheetResourceClient;
class VoidCallback;

class InspectorResourceContentLoader FINAL : private RawResourceClient, private StyleSheetResourceClient {
    WTF_MAKE_NONCOPYABLE(InspectorResourceContentLoader);
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorResourceContentLoader(Page*, PassOwnPtr<VoidCallback>);
    virtual ~InspectorResourceContentLoader();
    bool hasFinished();

private:
    virtual void notifyFinished(Resource*) OVERRIDE;
    virtual void setCSSStyleSheet(const String& /* href */, const KURL& /* baseURL */, const String& /* charset */, const CSSStyleSheetResource*);
    void resourceFinished(Resource*);
    void worked();
    void removeAsClientFromResource(Resource*);

    OwnPtr<VoidCallback> m_callback;
    int m_pendingResourceCount;
    HashSet<Resource*> m_pendingResources;
    Vector<ResourcePtr<Resource> > m_resources;
};

} // namespace WebCore


#endif // !defined(InspectorResourceContentLoader_h)
