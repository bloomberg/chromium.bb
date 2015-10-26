// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/css/invalidation/InvalidationData.h"

namespace blink {

DescendantInvalidationSet& InvalidationData::ensureDescendantInvalidationSet()
{
    if (!m_descendants)
        m_descendants = DescendantInvalidationSet::create();
    return *m_descendants;
}

SiblingInvalidationSet& InvalidationData::ensureSiblingInvalidationSet()
{
    if (!m_siblings)
        m_siblings = SiblingInvalidationSet::create();
    return *m_siblings;
}

InvalidationSet& InvalidationData::ensureInvalidationSet(InvalidationType type)
{
    if (type == InvalidateDescendants)
        return ensureDescendantInvalidationSet();

    return ensureSiblingInvalidationSet();
}

void InvalidationData::combine(const InvalidationData& other)
{
    if (other.descendants()) {
        ensureDescendantInvalidationSet().combine(*other.descendants());
    }

    if (other.siblings()) {
        ensureSiblingInvalidationSet().combine(*other.siblings());
    }
}

} // namespace blink
