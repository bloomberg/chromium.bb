// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/DisplayListPattern.h"

#include "platform/graphics/DisplayList.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "third_party/skia/include/core/SkShader.h"

namespace blink {

DisplayListPattern::DisplayListPattern(PassRefPtr<DisplayList> displayList, RepeatMode mode)
    : Pattern(mode)
    , m_tileDisplayList(displayList)
{
    // All current clients use RepeatModeXY, so we only support this mode for now.
    ASSERT(isRepeatXY());

    // FIXME: we don't have a good way to account for DL memory utilization.
}

DisplayListPattern::~DisplayListPattern()
{
}

PassRefPtr<SkShader> DisplayListPattern::createShader()
{
    SkMatrix localMatrix = affineTransformToSkMatrix(m_patternSpaceTransformation);
    SkRect tileBounds = SkRect::MakeWH(m_tileDisplayList->bounds().width(),
        m_tileDisplayList->bounds().height());

    return adoptRef(SkShader::CreatePictureShader(m_tileDisplayList->picture().get(),
        SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode, &localMatrix, &tileBounds));
}

} // namespace
