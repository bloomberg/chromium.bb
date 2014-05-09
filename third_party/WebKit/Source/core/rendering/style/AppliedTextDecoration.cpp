// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/rendering/style/AppliedTextDecoration.h"

namespace WebCore {

AppliedTextDecoration::AppliedTextDecoration(TextDecoration line)
    : m_line(line)
{
}

AppliedTextDecoration::AppliedTextDecoration()
    : m_line(TextDecorationUnderline)
{
}

bool AppliedTextDecoration::operator==(const AppliedTextDecoration& o) const
{
    return m_line == o.m_line;
}

} // namespace WebCore
