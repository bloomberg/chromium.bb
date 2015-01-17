// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/ClipPathDisplayItem.h"

#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/Path.h"
#include "public/platform/WebDisplayItemList.h"
#include "third_party/skia/include/core/SkScalar.h"

namespace blink {

void BeginClipPathDisplayItem::replay(GraphicsContext* context)
{
    context->save();
    context->clipPath(m_clipPath, m_windRule);
}

void BeginClipPathDisplayItem::appendToWebDisplayItemList(WebDisplayItemList* list) const
{
    // FIXME: Store an SkPath instead of a blink::Path to avoid this conversion on every update
    SkPath& path = const_cast<SkPath&>(m_clipPath.skPath());
    path.setFillType(WebCoreWindRuleToSkFillType(m_windRule));
    list->appendClipPathItem(path, SkRegion::kIntersect_Op, true);
}

void EndClipPathDisplayItem::replay(GraphicsContext* context)
{
    context->restore();
}

void EndClipPathDisplayItem::appendToWebDisplayItemList(WebDisplayItemList* list) const
{
    list->appendEndClipPathItem();
}

#ifndef NDEBUG
void BeginClipPathDisplayItem::dumpPropertiesAsDebugString(WTF::StringBuilder& stringBuilder) const
{
    DisplayItem::dumpPropertiesAsDebugString(stringBuilder);
    stringBuilder.append(WTF::String::format(", pathLength: %f, windRule: \"%s\"",
        m_clipPath.length(), m_windRule == RULE_NONZERO ? "nonzero" : "evenodd"));
}

#endif

} // namespace blink
