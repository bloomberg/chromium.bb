// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutAnalyzer_h
#define LayoutAnalyzer_h

#include "wtf/LinkedStack.h"
#include "wtf/Vector.h"

namespace blink {

class LayoutObject;
class TracedValue;

// Observes the performance of layout and reports statistics via a TracedValue.
// Usage:
// LayoutAnalyzer::Scope analyzer(*this);
class LayoutAnalyzer {
public:
    enum Counter {
        Depth,
        Total,
    };
    static const size_t NumCounters = 2;

    class Scope {
    public:
        explicit Scope(const LayoutObject&);
        ~Scope();

    private:
        const LayoutObject& m_layoutObject;
        LayoutAnalyzer* m_analyzer;
    };

    LayoutAnalyzer() { }

    void reset();
    void push(const LayoutObject&);
    void pop(const LayoutObject&);

    void increment(Counter counter, unsigned delta = 1)
    {
        m_counters[counter] += delta;
    }

    PassRefPtr<TracedValue> toTracedValue();

private:
    const char* nameForCounter(Counter) const;

    WTF::Vector<unsigned> m_counters;
    WTF::LinkedStack<const LayoutObject*> m_stack;
};

} // namespace blink

#endif // LayoutAnalyzer_h
