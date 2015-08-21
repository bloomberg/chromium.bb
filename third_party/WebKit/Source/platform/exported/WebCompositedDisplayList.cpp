// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "public/platform/WebCompositedDisplayList.h"

#include "platform/graphics/CompositedDisplayList.h"

namespace blink {

WebCompositedDisplayList::~WebCompositedDisplayList()
{
    // WebPrivateOwnPtr requires explicit clearing here.
    m_private.reset(nullptr);
}

void WebCompositedDisplayList::assign(PassOwnPtr<CompositedDisplayList> compositedDisplayList)
{
    m_private.reset(compositedDisplayList);
}

CompositedDisplayList* WebCompositedDisplayList::compositedDisplayListForTesting()
{
    return m_private.get();
}

} // namespace blink
