// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "web/tests/sim/SimDisplayItemList.h"

#include "third_party/skia/include/core/SkPicture.h"

namespace blink {

SimDisplayItemList::SimDisplayItemList()
    : m_didDrawText(false)
    , m_drawCount(0)
{
}

void SimDisplayItemList::appendDrawingItem(const SkPicture* picture)
{
    // There's not much we can tell about a picture, but we can tell if it
    // contains text. Ideally paint slimming would have a higher level
    // display list (or special GraphicsContext) so unit tests could query
    // what kind of things were painted.
    m_didDrawText |= picture->hasText();
    m_drawCount += picture->approximateOpCount();
}

} // namespace blink
