// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "core/html/canvas/ClipList.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/Path.h"
#include "platform/transforms/AffineTransform.h"

namespace blink {

ClipList::ClipList(const ClipList& other) : m_clipList(other.m_clipList) { }

void ClipList::clipPath(const Path& path, WindRule windRule, AntiAliasingMode antiAliasingMode, const AffineTransform& ctm)
{
    ClipOp newClip;
    newClip.m_antiAliasingMode = antiAliasingMode;
    newClip.m_windRule = windRule;
    newClip.m_path = path;
    newClip.m_path.transform(ctm);
    m_clipList.append(newClip);
}

void ClipList::playback(GraphicsContext* context) const
{
    for (const ClipOp* it = m_clipList.begin(); it < m_clipList.end(); it++) {
        context->clipPath(it->m_path, it->m_windRule, it->m_antiAliasingMode);
    }
}

ClipList::ClipOp::ClipOp()
    : m_antiAliasingMode(AntiAliased)
    , m_windRule(RULE_NONZERO)
{ }

ClipList::ClipOp::ClipOp(const ClipOp& other)
    : m_path(other.m_path)
    , m_antiAliasingMode(other.m_antiAliasingMode)
    , m_windRule(other.m_windRule)
{ }

} // namespace blink
