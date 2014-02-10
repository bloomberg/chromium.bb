// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeprecatedScheduleStyleRecalcDuringCompositingUpdate_h
#define DeprecatedScheduleStyleRecalcDuringCompositingUpdate_h

#include "core/dom/DocumentLifecycle.h"

namespace WebCore {

class DeprecatedScheduleStyleRecalcDuringCompositingUpdate {
    WTF_MAKE_NONCOPYABLE(DeprecatedScheduleStyleRecalcDuringCompositingUpdate);
public:
    explicit DeprecatedScheduleStyleRecalcDuringCompositingUpdate(DocumentLifecycle&);
    ~DeprecatedScheduleStyleRecalcDuringCompositingUpdate();

private:
    DocumentLifecycle& m_lifecycle;
    DocumentLifecycle::DeprecatedTransition m_deprecatedTransition;
    DocumentLifecycle::State m_originalState;
};

}

#endif
