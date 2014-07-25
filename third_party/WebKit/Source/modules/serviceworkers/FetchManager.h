// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FetchManager_h
#define FetchManager_h

#include "bindings/core/v8/ScriptPromise.h"
#include "wtf/HashSet.h"
#include "wtf/OwnPtr.h"

namespace blink {

class ExecutionContext;
class FetchRequestData;
class ScriptState;
class ResourceRequest;

class FetchManager {
public:
    FetchManager(ExecutionContext*);
    ~FetchManager();
    ScriptPromise fetch(ScriptState*, PassRefPtrWillBeRawPtr<FetchRequestData>);

    static bool isSimpleMethod(const String&);
    static bool isForbiddenMethod(const String&);
    static bool isUsefulMethod(const String&);
private:
    class Loader;

    // Removes loader from |m_loaders|.
    void onLoaderFinished(Loader*);

    ExecutionContext* m_executionContext;
    HashSet<OwnPtr<Loader> > m_loaders;
};

} // namespace blink

#endif // FetchManager_h
