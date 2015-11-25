// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef URLSearchParams_h
#define URLSearchParams_h

#include "bindings/core/v8/Iterable.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/UnionTypesCore.h"
#include "platform/heap/Handle.h"
#include "platform/network/EncodedFormData.h"
#include "wtf/Forward.h"
#include "wtf/text/WTFString.h"
#include <base/gtest_prod_util.h>
#include <utility>

namespace blink {

class ExceptionState;

typedef USVStringOrURLSearchParams URLSearchParamsInit;

class CORE_EXPORT URLSearchParams final : public GarbageCollectedFinalized<URLSearchParams>, public ScriptWrappable, public PairIterable<String, String> {
    DEFINE_WRAPPERTYPEINFO();

public:
    // TODO(mkwst): We should support integration with URLUtils, as explored in
    // https://codereview.chromium.org/143313002/. That approach is totally
    // reasonable, but relies on Node switching to Oilpan. Sigbjorn assures me
    // that this will happen Real Soon Now(tm).
    static URLSearchParams* create(const URLSearchParamsInit&);

    // TODO(mkwst): ScriptWrappable doesn't have a destructor with Oilpan, so this
    // won't need to be virtual once that's the default.
    virtual ~URLSearchParams();

    // URLSearchParams interface methods
    String toString() const;
    void append(const String& name, const String& value);
    void deleteAllWithName(const String&);
    String get(const String&) const;
    Vector<String> getAll(const String&) const;
    bool has(const String&) const;
    void set(const String& name, const String& value);
    void setInput(const String&);

    // Internal helpers
    PassRefPtr<EncodedFormData> encodeFormData() const;
    const Vector<std::pair<String, String>>& params() const { return m_params; }

    DECLARE_TRACE();

private:
    FRIEND_TEST_ALL_PREFIXES(URLSearchParamsTest, EncodedFormData);

    explicit URLSearchParams(const String&);
    explicit URLSearchParams(URLSearchParams*);
    Vector<std::pair<String, String>> m_params;

    IterationSource* startIteration(ScriptState*, ExceptionState&) override;
};

} // namespace blink

#endif // URLSearchParams_h
