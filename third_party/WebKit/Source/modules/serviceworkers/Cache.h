// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Cache_h
#define Cache_h

#include "bindings/v8/ScriptWrappable.h"
#include "wtf/Forward.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/Vector.h"

namespace WebCore {

class ExecutionContext;
class ScriptPromise;

class Cache FINAL : public RefCounted<Cache>, public ScriptWrappable {
public:
    static PassRefPtr<Cache> create(const Vector<String>& urlStrings);
    ~Cache();

    ScriptPromise match(ExecutionContext*, const String& urlString);
    ScriptPromise ready(ExecutionContext*);

private:
    Cache();
};

} // namespace WebCore

#endif // Cache_h
