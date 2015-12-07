// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InvalidationData_h
#define InvalidationData_h

#include "core/css/invalidation/InvalidationSet.h"

namespace blink {

class CORE_EXPORT InvalidationData final {
public:
    void combine(const InvalidationData& other);

    DescendantInvalidationSet* descendants() const { return m_descendants.get(); }
    SiblingInvalidationSet* siblings() const { return m_siblings.get(); }

    DescendantInvalidationSet& ensureDescendantInvalidationSet();
    SiblingInvalidationSet& ensureSiblingInvalidationSet();
    InvalidationSet& ensureInvalidationSet(InvalidationType);

private:
    RefPtr<DescendantInvalidationSet> m_descendants;
    RefPtr<SiblingInvalidationSet> m_siblings;
};

} // namespace blink

#endif // InvalidationData_h
