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

#ifndef ScopeContentDistribution_h
#define ScopeContentDistribution_h

#include "wtf/Forward.h"
#include "wtf/Vector.h"

namespace WebCore {

class InsertionPoint;
class ShadowRoot;

class ScopeContentDistribution {
public:
    ScopeContentDistribution();

    InsertionPoint* insertionPointAssignedTo() const { return m_insertionPointAssignedTo.get(); }
    void setInsertionPointAssignedTo(PassRefPtr<InsertionPoint>);

    void registerInsertionPoint(InsertionPoint*);
    void unregisterInsertionPoint(InsertionPoint*);
    bool hasShadowElementChildren() const { return m_numberOfShadowElementChildren > 0; }
    bool hasContentElementChildren() const { return m_numberOfContentElementChildren > 0; }

    void registerElementShadow() { ++m_numberOfElementShadowChildren; }
    void unregisterElementShadow() { ASSERT(m_numberOfElementShadowChildren > 0); --m_numberOfElementShadowChildren; }
    unsigned numberOfElementShadowChildren() const { return m_numberOfElementShadowChildren; }
    bool hasElementShadowChildren() const { return m_numberOfElementShadowChildren > 0; }

    void invalidateInsertionPointList();
    const Vector<RefPtr<InsertionPoint> >& ensureInsertionPointList(ShadowRoot*);

private:
    RefPtr<InsertionPoint> m_insertionPointAssignedTo;
    unsigned m_numberOfShadowElementChildren;
    unsigned m_numberOfContentElementChildren;
    unsigned m_numberOfElementShadowChildren;
    bool m_insertionPointListIsValid;
    Vector<RefPtr<InsertionPoint> > m_insertionPointList;
};

} // namespace WebCore

#endif
