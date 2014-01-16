/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#ifndef RuleSetAnalyzer_h
#define RuleSetAnalyzer_h

#include "core/css/analyzer/DescendantInvalidationSet.h"
#include "wtf/RefCounted.h"

namespace WebCore {

class CSSSelector;
class RuleFeatureSet;
class RuleData;

// Keeps track of information about a set of CSS rules in order to answer style questions about them.
class RuleSetAnalyzer : public RefCounted<RuleSetAnalyzer> {
public:
    static PassRefPtr<RuleSetAnalyzer> create();

    void collectFeaturesFromRuleData(RuleFeatureSet& features, const RuleData&);
    void combine(const RuleSetAnalyzer& other);
private:
    RuleSetAnalyzer();
    typedef HashMap<AtomicString, RefPtr<DescendantInvalidationSet> > InvalidationSetMap;

    DescendantInvalidationSet& ensureClassInvalidationSet(const AtomicString& className);

    bool updateClassInvalidationSets(const CSSSelector*);

    InvalidationSetMap m_classInvalidationSets;
};

} // namespace WebCore

#endif // RuleSetAnalyzer_h
