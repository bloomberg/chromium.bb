// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DisplayListPattern_h
#define DisplayListPattern_h

#include "platform/graphics/Pattern.h"

namespace blink {

class DisplayList;

class PLATFORM_EXPORT DisplayListPattern : public Pattern {
public:
    static PassRefPtr<DisplayListPattern> create(PassRefPtr<DisplayList> displayList,
        RepeatMode repeatMode)
    {
        return adoptRef(new DisplayListPattern(displayList, repeatMode));
    }

    virtual ~DisplayListPattern();

protected:
    virtual PassRefPtr<SkShader> createShader() override;

private:
    DisplayListPattern(PassRefPtr<DisplayList>, RepeatMode);

    RefPtr<DisplayList> m_tileDisplayList;
};

} // namespace

#endif
