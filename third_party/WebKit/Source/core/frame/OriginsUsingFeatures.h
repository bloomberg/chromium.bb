// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OriginsUsingFeatures_h
#define OriginsUsingFeatures_h

#include "platform/heap/Handle.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Document;
class ScriptState;

class OriginsUsingFeatures {
    DISALLOW_ALLOCATION();
public:
    ~OriginsUsingFeatures();

    enum class Feature {
        ElementCreateShadowRoot,
        DocumentRegisterElement,

        NumberOfFeatures // This must be the last item.
    };

    static void count(const ScriptState*, Document&, Feature);

    void documentDetached(Document&);
    void updateMeasurementsAndClear();

    class Value {
    public:
        Value();

        bool isEmpty() const { return !m_countBits; }
        void clear() { m_countBits = 0; }

        void count(Feature);
        bool get(Feature feature) const { return m_countBits & (1 << static_cast<unsigned>(feature)); }

        void aggregate(Value);
        void updateMeasurements(const String& origin);

    private:
        unsigned m_countBits : static_cast<unsigned>(Feature::NumberOfFeatures);
    };

private:
    Vector<std::pair<String, OriginsUsingFeatures::Value>, 1> m_originAndValues;
};

} // namespace blink

#endif // OriginsUsingFeatures_h
