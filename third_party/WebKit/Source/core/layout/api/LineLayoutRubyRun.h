// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LineLayoutRubyRun_h
#define LineLayoutRubyRun_h

#include "core/layout/LayoutRubyRun.h"
#include "core/layout/api/LineLayoutBlockFlow.h"

namespace blink {

class LineLayoutRubyRun : public LineLayoutBlockFlow {
public:
    explicit LineLayoutRubyRun(LayoutRubyRun* layoutRubyRun)
        : LineLayoutBlockFlow(layoutRubyRun)
    {
    }

    explicit LineLayoutRubyRun(const LineLayoutItem& item)
        : LineLayoutBlockFlow(item)
    {
        ASSERT(!item || item.isRubyRun());
    }

    explicit LineLayoutRubyRun(std::nullptr_t) : LineLayoutBlockFlow(nullptr) { }

    LineLayoutRubyRun() { }

    void getOverhang(bool firstLine, LineLayoutItem startLayoutItem, LineLayoutItem endLayoutItem, int& startOverhang, int& endOverhang) const
    {
        toRubyRun()->getOverhang(firstLine, startLayoutItem, endLayoutItem, startOverhang, endOverhang);
    }

    LayoutRubyText* rubyText() const
    {
        return toRubyRun()->rubyText();
    }

    LayoutRubyBase* rubyBase() const
    {
        return toRubyRun()->rubyBase();
    }

private:
    LayoutRubyRun* toRubyRun()
    {
        return toLayoutRubyRun(layoutObject());
    }

    const LayoutRubyRun* toRubyRun() const
    {
        return toLayoutRubyRun(layoutObject());
    }
};

} // namespace blink

#endif // LineLayoutRubyRun_h
