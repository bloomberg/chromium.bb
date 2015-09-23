// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "web/tests/sim/SimLayerTreeView.h"

namespace blink {

SimLayerTreeView::SimLayerTreeView()
    : m_needsAnimate(false)
    , m_deferCommits(true)
{
}

} // namespace blink
