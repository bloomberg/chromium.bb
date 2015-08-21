// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebCompositedDisplayList_h
#define WebCompositedDisplayList_h

#include "public/platform/WebCommon.h"
#include "public/platform/WebPrivateOwnPtr.h"

namespace blink {

class CompositedDisplayList;

class BLINK_PLATFORM_EXPORT WebCompositedDisplayList {
public:
    WebCompositedDisplayList() {}
    ~WebCompositedDisplayList();

#if INSIDE_BLINK
    void assign(WTF::PassOwnPtr<CompositedDisplayList>);
    CompositedDisplayList* compositedDisplayListForTesting();
#endif

    // TODO(pdr): Add accessor functions for the data in m_private as needed.

private:
    WebPrivateOwnPtr<CompositedDisplayList> m_private;
};

} // namespace blink

#endif // WebCompositedDisplayList_h
