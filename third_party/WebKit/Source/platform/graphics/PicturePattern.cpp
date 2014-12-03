// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/PicturePattern.h"

#include "platform/graphics/Picture.h"
#include "platform/graphics/skia/SkiaUtils.h"
#include "third_party/skia/include/core/SkShader.h"

namespace blink {

PicturePattern::PicturePattern(PassRefPtr<Picture> picture, RepeatMode mode)
    : Pattern(mode)
    , m_tilePicture(picture)
{
    // All current clients use RepeatModeXY, so we only support this mode for now.
    ASSERT(isRepeatXY());

    // FIXME: we don't have a good way to account for DL memory utilization.
}

PicturePattern::~PicturePattern()
{
}

PassRefPtr<SkShader> PicturePattern::createShader()
{
    SkMatrix localMatrix = affineTransformToSkMatrix(m_patternSpaceTransformation);
    SkRect tileBounds = SkRect::MakeWH(m_tilePicture->bounds().width(),
        m_tilePicture->bounds().height());

    return adoptRef(SkShader::CreatePictureShader(m_tilePicture->skPicture().get(),
        SkShader::kRepeat_TileMode, SkShader::kRepeat_TileMode, &localMatrix, &tileBounds));
}

} // namespace
