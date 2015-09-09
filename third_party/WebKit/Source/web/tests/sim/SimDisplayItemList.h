// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SimDisplayItemList_h
#define SimDisplayItemList_h

#include "public/platform/WebDisplayItemList.h"

namespace blink {

class SimDisplayItemList final : public WebDisplayItemList {
public:
    SimDisplayItemList();

    void appendDrawingItem(const SkPicture*) override;

    bool didDrawText() const { return m_didDrawText; }
    int drawCount() const { return m_drawCount; }

private:
    bool m_didDrawText;
    int m_drawCount;
};

} // namespace blink

#endif
