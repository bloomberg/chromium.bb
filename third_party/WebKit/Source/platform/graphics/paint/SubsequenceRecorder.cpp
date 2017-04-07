// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/paint/SubsequenceRecorder.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/paint/PaintController.h"

namespace blink {

SubsequenceRecorder::SubsequenceRecorder(GraphicsContext& context,
                                         const DisplayItemClient& client)
    : m_paintController(context.getPaintController()),
      m_client(client),
      m_beginSubsequenceIndex(0) {
  if (m_paintController.displayItemConstructionIsDisabled())
    return;

  m_beginSubsequenceIndex = m_paintController.newDisplayItemList().size();

#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
  m_paintController.beginSubsequence(m_client);
#endif
}

SubsequenceRecorder::~SubsequenceRecorder() {
#if CHECK_DISPLAY_ITEM_CLIENT_ALIVENESS
  m_paintController.endSubsequence();
#endif

  if (m_paintController.displayItemConstructionIsDisabled())
    return;

  // Skip empty subsequences.
  if (m_paintController.newDisplayItemList().size() == m_beginSubsequenceIndex)
    return;

  m_paintController.addCachedSubsequence(
      m_client, m_beginSubsequenceIndex,
      m_paintController.newDisplayItemList().size() - 1);
}

}  // namespace blink
