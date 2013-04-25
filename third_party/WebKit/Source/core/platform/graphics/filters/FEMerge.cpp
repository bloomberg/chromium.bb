/*
 * Copyright (C) 2004, 2005, 2006, 2007 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2005 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "core/platform/graphics/filters/FEMerge.h"

#include "SkMergeImageFilter.h"
#include "core/platform/graphics/GraphicsContext.h"
#include "core/platform/graphics/filters/Filter.h"
#include "core/platform/graphics/filters/SkiaImageFilterBuilder.h"
#include "core/platform/text/TextStream.h"
#include "core/rendering/RenderTreeAsText.h"
#include <wtf/OwnArrayPtr.h>

namespace WebCore {

FEMerge::FEMerge(Filter* filter) 
    : FilterEffect(filter)
{
}

PassRefPtr<FEMerge> FEMerge::create(Filter* filter)
{
    return adoptRef(new FEMerge(filter));
}

void FEMerge::applySoftware()
{
    unsigned size = numberOfEffectInputs();
    ASSERT(size > 0);

    ImageBuffer* resultImage = createImageBufferResult();
    if (!resultImage)
        return;

    GraphicsContext* filterContext = resultImage->context();
    for (unsigned i = 0; i < size; ++i) {
        FilterEffect* in = inputEffect(i);
        filterContext->drawImageBuffer(in->asImageBuffer(), ColorSpaceDeviceRGB, drawingRegionOfInputImage(in->absolutePaintRect()));
    }
}

SkImageFilter* FEMerge::createImageFilter(SkiaImageFilterBuilder* builder)
{
    unsigned size = numberOfEffectInputs();

    OwnArrayPtr<SkAutoTUnref<SkImageFilter> > inputRefs = adoptArrayPtr(new SkAutoTUnref<SkImageFilter>[size]);
    OwnArrayPtr<SkImageFilter*> inputs = adoptArrayPtr(new SkImageFilter*[size]);
    for (unsigned i = 0; i < size; ++i) {
        inputRefs[i].reset(builder->build(inputEffect(i)));
        inputs[i] = inputRefs[i].get();
    }
    return new SkMergeImageFilter(inputs.get(), size);
}

TextStream& FEMerge::externalRepresentation(TextStream& ts, int indent) const
{
    writeIndent(ts, indent);
    ts << "[feMerge";
    FilterEffect::externalRepresentation(ts);
    unsigned size = numberOfEffectInputs();
    ASSERT(size > 0);
    ts << " mergeNodes=\"" << size << "\"]\n";
    for (unsigned i = 0; i < size; ++i)
        inputEffect(i)->externalRepresentation(ts, indent + 1);
    return ts;
}

} // namespace WebCore
