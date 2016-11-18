// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/style/AppliedTextDecoration.h"

namespace blink {

AppliedTextDecoration::AppliedTextDecoration(TextDecoration line,
                                             TextDecorationStyle style,
                                             Color color)
    : m_lines(line), m_style(style), m_color(color) {}

bool AppliedTextDecoration::operator==(const AppliedTextDecoration& o) const {
  return m_color == o.m_color && m_lines == o.m_lines && m_style == o.m_style;
}

}  // namespace blink
