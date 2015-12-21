// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ScopeRecorder.h"

#include "core/layout/LayoutObject.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintController.h"

namespace blink {

ScopeRecorder::ScopeRecorder(GraphicsContext& context)
    : m_paintController(context.paintController())
{
    m_paintController.beginScope();
}

ScopeRecorder::~ScopeRecorder()
{
    m_paintController.endScope();
}

} // namespace blink
