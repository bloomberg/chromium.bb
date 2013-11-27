/*
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

#include "core/frame/GraphicsLayerDebugInfo.h"

#include "platform/JSONValues.h"
#include "wtf/text/CString.h"

namespace WebCore {

void GraphicsLayerDebugInfo::appendAsTraceFormat(blink::WebString* out) const
{
    RefPtr<JSONArray> jsonArray = JSONArray::create();
    for (size_t i = 0; i < m_currentLayoutRects.size(); i++) {
        const LayoutRect& rect = m_currentLayoutRects[i];
        RefPtr<JSONObject> rectContainer = JSONObject::create();
        RefPtr<JSONArray> rectArray = JSONArray::create();
        rectArray->pushNumber(rect.x().toFloat());
        rectArray->pushNumber(rect.y().toFloat());
        rectArray->pushNumber(rect.maxX().toFloat());
        rectArray->pushNumber(rect.maxY().toFloat());
        rectContainer->setArray("geometry_rect", rectArray);
        jsonArray->pushObject(rectContainer);
    }
    *out = jsonArray->toJSONString();
}

} // namespace WebCore
