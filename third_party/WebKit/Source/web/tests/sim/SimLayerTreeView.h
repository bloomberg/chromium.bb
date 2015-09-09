// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SimLayerTreeView_h
#define SimLayerTreeView_h

#include "public/platform/WebLayerTreeView.h"
#include "wtf/OwnPtr.h"

namespace blink {

class SimLayerTreeView final : public WebLayerTreeView {
public:
    SimLayerTreeView();

    void setNeedsAnimate() override { m_needsAnimate = true; }
    bool needsAnimate() const { return m_needsAnimate; }
    void clearNeedsAnimate() { m_needsAnimate = false; }

    void setDeferCommits(bool deferCommits) override { m_deferCommits = deferCommits; }
    bool deferCommits() const { return m_deferCommits; }

private:
    bool m_needsAnimate;
    bool m_deferCommits;
};

} // namespace blink

#endif
