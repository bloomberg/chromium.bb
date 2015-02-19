/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FilterEffectRenderer_h
#define FilterEffectRenderer_h

#include "platform/graphics/filters/FilterEffect.h"
#include "platform/heap/Handle.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace blink {

class FilterOperations;
class ReferenceFilter;
class LayoutObject;

class FilterEffectRenderer final : public RefCountedWillBeGarbageCollectedFinalized<FilterEffectRenderer> {
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED;
public:
    static PassRefPtrWillBeRawPtr<FilterEffectRenderer> create()
    {
        return adoptRefWillBeNoop(new FilterEffectRenderer());
    }

    virtual ~FilterEffectRenderer();
    DECLARE_TRACE();

    bool build(LayoutObject* renderer, const FilterOperations&);
    void clearIntermediateResults();

    PassRefPtrWillBeRawPtr<FilterEffect> lastEffect() const
    {
        return m_lastEffect;
    }

private:
    FilterEffectRenderer();

    RefPtrWillBeMember<FilterEffect> m_lastEffect;
    WillBeHeapVector<RefPtrWillBeMember<ReferenceFilter>> m_referenceFilters;
};

} // namespace blink

#endif // FilterEffectRenderer_h
