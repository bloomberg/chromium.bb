// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AppliedTextDecoration_h
#define AppliedTextDecoration_h

#include "core/rendering/style/RenderStyleConstants.h"

namespace WebCore {

class AppliedTextDecoration {
public:
    explicit AppliedTextDecoration(TextDecoration);
    AppliedTextDecoration();

    TextDecoration line() const { return static_cast<TextDecoration>(m_line); }

    bool operator==(const AppliedTextDecoration&) const;
    bool operator!=(const AppliedTextDecoration& o) const { return !(*this == o); }

private:
    unsigned m_line : TextDecorationBits;
};

} // namespace WebCore

#endif // AppliedTextDecoration_h
