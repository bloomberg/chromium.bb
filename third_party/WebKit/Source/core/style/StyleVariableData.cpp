// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/style/StyleVariableData.h"

#include "core/style/DataEquivalency.h"

namespace blink {

bool StyleVariableData::operator==(const StyleVariableData& other) const
{
    if (m_data.size() != other.m_data.size()) {
        return false;
    }

    for (const auto& iter : m_data) {
        RefPtr<CSSVariableData> otherData = other.m_data.get(iter.key);
        if (!dataEquivalent(iter.value, otherData))
            return false;
    }

    return true;
}

} // namespace blink
