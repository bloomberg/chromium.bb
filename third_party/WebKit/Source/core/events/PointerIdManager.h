// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PointerIdManager_h
#define PointerIdManager_h

#include "public/platform/WebPointerProperties.h"
#include "wtf/Allocator.h"
#include "wtf/ListHashSet.h"

namespace blink {

/**
  Helper class for tracking the primary pointer id for each type of PointerEvents.
*/
class PointerIdManager {
    DISALLOW_NEW();
public:
    PointerIdManager();
    ~PointerIdManager();
    void clear();
    void add(WebPointerProperties::PointerType, unsigned);
    void remove(WebPointerProperties::PointerType, unsigned);
    bool isPrimary(WebPointerProperties::PointerType, unsigned);

private:
    // TODO(crbug.com/537319): Switch to /one/ set of ids to guarantee uniqueness.
    ListHashSet<unsigned> m_ids[static_cast<int>(WebPointerProperties::PointerType::LastEntry) + 1];
    bool m_hasPrimaryId[static_cast<int>(WebPointerProperties::PointerType::LastEntry) + 1];
};

} // namespace blink

#endif // PointerIdManager_h
