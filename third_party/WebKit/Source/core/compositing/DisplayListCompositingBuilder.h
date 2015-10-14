// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DisplayListCompositingBuilder_h
#define DisplayListCompositingBuilder_h

#include "core/CoreExport.h"
#include "platform/graphics/CompositedDisplayList.h"
#include "platform/graphics/paint/PaintController.h"

namespace blink {

class CORE_EXPORT DisplayListCompositingBuilder {
public:
    DisplayListCompositingBuilder(const PaintController& paintController)
        : m_paintController(paintController) { }

    void build(CompositedDisplayList&);

private:
    const PaintController& m_paintController;
};

} // namespace blink

#endif // DisplayListCompositingBuilder_h
