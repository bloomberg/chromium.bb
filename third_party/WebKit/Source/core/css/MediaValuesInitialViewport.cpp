// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/css/MediaValuesInitialViewport.h"

#include "core/dom/Document.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/api/LayoutViewItem.h"

namespace blink {

MediaValues* MediaValuesInitialViewport::create(LocalFrame& frame) {
  return new MediaValuesInitialViewport(frame);
}

MediaValuesInitialViewport::MediaValuesInitialViewport(LocalFrame& frame)
    : MediaValuesDynamic(&frame) {}

double MediaValuesInitialViewport::viewportWidth() const {
  DCHECK(m_frame->view() && m_frame->document());
  int viewportWidth = m_frame->view()->frameRect().width();
  return adjustDoubleForAbsoluteZoom(
      viewportWidth, m_frame->document()->layoutViewItem().styleRef());
}

double MediaValuesInitialViewport::viewportHeight() const {
  DCHECK(m_frame->view() && m_frame->document());
  int viewportHeight = m_frame->view()->frameRect().height();
  return adjustDoubleForAbsoluteZoom(
      viewportHeight, m_frame->document()->layoutViewItem().styleRef());
}

}  // namespace blink
