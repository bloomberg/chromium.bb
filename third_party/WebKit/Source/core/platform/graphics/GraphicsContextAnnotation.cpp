/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/platform/graphics/GraphicsContextAnnotation.h"

#include "core/platform/graphics/GraphicsContext.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/PaintPhase.h"
#include "core/rendering/RenderObject.h"
#include "wtf/text/StringBuilder.h"

namespace {

const char* AnnotationKeyRendererName  = "RENDERER";
const char* AnnotationKeyPaintPhase    = "PHASE";
const char* AnnotationKeyElementId     = "ID";
const char* AnnotationKeyElementClass  = "CLASS";
const char* AnnotationKeyElementTag    = "TAG";

static const char* paintPhaseName(WebCore::PaintPhase phase)
{
    switch (phase) {
    case WebCore::PaintPhaseBlockBackground:
        return "BlockBackground";
    case WebCore::PaintPhaseChildBlockBackground:
        return "ChildBlockBackground";
    case WebCore::PaintPhaseChildBlockBackgrounds:
        return "ChildBlockBackgrounds";
    case WebCore::PaintPhaseFloat:
        return "Float";
    case WebCore::PaintPhaseForeground:
        return "Foreground";
    case WebCore::PaintPhaseOutline:
        return "Outline";
    case WebCore::PaintPhaseChildOutlines:
        return "ChildOutlines";
    case WebCore::PaintPhaseSelfOutline:
        return "SelfOutline";
    case WebCore::PaintPhaseSelection:
        return "Selection";
    case WebCore::PaintPhaseCollapsedTableBorders:
        return "CollapsedTableBorders";
    case WebCore::PaintPhaseTextClip:
        return "TextClip";
    case WebCore::PaintPhaseMask:
        return "Mask";
    case WebCore::PaintPhaseClippingMask:
        return "ClippingMask";
    default:
        ASSERT_NOT_REACHED();
        return "<unknown>";
    }
}

}

namespace WebCore {

GraphicsContextAnnotation::GraphicsContextAnnotation(const PaintInfo& paintInfo, const RenderObject* object)
    : m_rendererName(0)
    , m_paintPhase(0)
{
    ASSERT(paintInfo.context);
    ASSERT(object);

    AnnotationModeFlags mode = paintInfo.context->annotationMode();
    Element* element = object->node() && object->node()->isElementNode() ? toElement(object->node()) : 0;

    if (mode & AnnotateRendererName)
        m_rendererName = object->renderName();

    if (mode & AnnotatePaintPhase)
        m_paintPhase = paintPhaseName(paintInfo.phase);

    if ((mode & AnnotateElementId) && element) {
        const AtomicString id = element->getIdAttribute();
        if (!id.isNull() && !id.isEmpty())
            m_elementId = id.string();
    }

    if ((mode & AnnotateElementClass) && element && element->hasClass()) {
        SpaceSplitString classes = element->classNames();
        if (!classes.isNull() && classes.size() > 0) {
            StringBuilder classBuilder;
            for (size_t i = 0; i < classes.size(); ++i) {
                if (i > 0)
                    classBuilder.append(" ");
                classBuilder.append(classes[i]);
            }

            m_elementClass = classBuilder.toString();
        }
    }

    if ((mode & AnnotateElementTag) && element)
        m_elementTag = element->tagName();
}

void GraphicsContextAnnotation::asAnnotationList(AnnotationList &list) const
{
    list.clear();

    if (m_rendererName)
        list.append(std::make_pair(AnnotationKeyRendererName, m_rendererName));

    if (m_paintPhase)
        list.append(std::make_pair(AnnotationKeyPaintPhase, m_paintPhase));

    if (!m_elementId.isEmpty())
        list.append(std::make_pair(AnnotationKeyElementId, m_elementId));

    if (!m_elementClass.isEmpty())
        list.append(std::make_pair(AnnotationKeyElementClass, m_elementClass));

    if (!m_elementTag.isEmpty())
        list.append(std::make_pair(AnnotationKeyElementTag, m_elementTag));
}

void GraphicsContextAnnotator::annotate(const PaintInfo& paintInfo, const RenderObject* object)
{
    ASSERT(!m_context);

    m_context = paintInfo.context;
    m_context->beginAnnotation(GraphicsContextAnnotation(paintInfo, object));
}

void GraphicsContextAnnotator::finishAnnotation()
{
    ASSERT(m_context);
    m_context->endAnnotation();
}

}
