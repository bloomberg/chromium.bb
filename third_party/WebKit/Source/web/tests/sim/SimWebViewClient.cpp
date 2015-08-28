// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "web/tests/sim/SimWebViewClient.h"

#include "public/platform/WebLayerTreeView.h"

namespace blink {

SimWebViewClient::SimWebViewClient(WebLayerTreeView& layerTreeView)
    : m_layerTreeView(&layerTreeView)
{
}

} // namespace blink
